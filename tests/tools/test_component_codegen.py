"""Regression checks for the batch generator's failure boundaries."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('component_codegen', ROOT / 'tools/generate-component-contracts.py')
codegen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codegen)


class BatchContracts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / 'tools/component_contracts', self.root / 'tools/component_contracts')
        shutil.copytree(ROOT / 'native/scene', self.root / 'native/scene')

    def edit(self, name, change):
        path = self.root / 'tools/component_contracts' / name
        data = json.loads(path.read_text(encoding='utf-8')); change(data)
        path.write_text(json.dumps(data, ensure_ascii=False), encoding='utf-8')

    def test_one_contract_updates_native_field_and_property_binding(self):
        before, _ = codegen.render(self.root)
        def change(data):
            prop = data['tables'][0]['items'][0]
            self.assertEqual(prop['id'], 'vertical_fov')
            prop['default'] = {'cpp': '61'}
            prop['min'] = {'cpp': '2'}
        self.edit('properties_camera.json', change)
        after, _ = codegen.render(self.root)
        changed = [name for name in before if before[name] != after[name]]
        self.assertEqual(len(changed), 2)
        self.assertTrue(any('fields' in name for name in changed))
        self.assertIn('camera_cameraNumbers.inc', changed)
        for name, text in after.items():
            (self.root / 'native/scene/generated' / name).write_bytes(text.encode('utf-8'))
        compiler = shutil.which('c++')
        self.assertIsNotNone(compiler, 'C++20 compiler required for the single-edit integration proof')
        source = self.root / 'proof.cpp'
        source.write_text('''#include "scene/camera.h"
#include <sstream>
int main() {
 ae::scene::Camera camera;
 if (camera.verticalFov != 61 || !camera.valid()) return 1;
 const auto &p = camera.descriptor.numbers[0];
 if (p.id != "vertical_fov" || p.minimum != 2 || p.read(camera) != 61) return 2;
 *p.write(camera) = 73;
 std::ostringstream saved; camera.write(saved);
 ae::scene::Camera loaded; std::istringstream input(saved.str());
 if (!loaded.read(input, camera.descriptor.version) || loaded.verticalFov != 73) return 3;
 *p.write(loaded) = 1;
 if (loaded.valid()) return 4;
 *p.write(loaded) = 73;
 const auto &mode = loaded.descriptor.enums[0];
 mode.write(loaded, mode.options[1].value);
 if (loaded.projection != ae::scene::CameraProjection::Orthographic || mode.read(loaded) != 1) return 5;
 std::ostringstream enumSaved; loaded.write(enumSaved);
 ae::scene::Camera enumLoaded; std::istringstream enumInput(enumSaved.str());
 if (!enumLoaded.read(enumInput, loaded.descriptor.version) || mode.read(enumLoaded) != 1) return 6;
 mode.write(enumLoaded, 42);
 return enumLoaded.valid() ? 7 : 0;
}
''', encoding='utf-8')
        binary = self.root / ('proof.exe' if sys.platform == 'win32' else 'proof')
        build = subprocess.run([compiler, '-std=c++20', '-O0', '-I', str(self.root / 'native'),
                                '-I', str(ROOT / 'native'), str(source), '-o', str(binary)], capture_output=True, text=True)
        self.assertEqual(build.returncode, 0, build.stderr)
        self.assertEqual(subprocess.run([str(binary)]).returncode, 0)

    def test_duplicate_property_is_rejected(self):
        self.edit('properties_camera.json', lambda d: d['tables'][0]['items'].append(d['tables'][0]['items'][0]))
        with self.assertRaisesRegex(ValueError, 'duplicate property'):
            codegen.render(self.root)

    def test_unknown_field_is_rejected_instead_of_ignored(self):
        self.edit('properties_camera.json', lambda d: d['tables'][0]['items'][0].update(defualt=60))
        with self.assertRaisesRegex(ValueError, 'unknown fields'):
            codegen.render(self.root)

    def test_unconsumed_field_default_is_rejected(self):
        self.edit('properties_camera.json', lambda d: d['field_groups'].clear())
        with self.assertRaisesRegex(ValueError, 'unused field defaults'):
            codegen.render(self.root)

    def test_disconnected_generated_code_is_rejected(self):
        path = self.root / 'native/scene/camera.h'
        path.write_text(path.read_text(encoding='utf-8').replace('#include "scene/generated/camera_cameraNumbers.inc"', ''), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'disconnected output'):
            codegen.render(self.root)

    def test_check_rejects_stale_output_without_repairing_it(self):
        path = self.root / 'native/scene/generated/camera_cameraNumbers.inc'
        path.write_bytes(b'// stale\n')
        result = subprocess.run([sys.executable, str(ROOT / 'tools/generate-component-contracts.py'), '--root', str(self.root), '--check'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn('stale generated outputs', result.stderr)
        self.assertEqual(path.read_bytes(), b'// stale\n')


if __name__ == '__main__':
    unittest.main()
