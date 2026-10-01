import hashlib, json, pathlib, zipfile
root = pathlib.Path(__file__).resolve().parent.parent
evidence = root / 'docs/validacao/evidencias/fields2d50-20261001'
apk = root / 'android/app/build/outputs/apk/debug/app-debug.apk'
generated = root / 'android/app/build/generated/aetherAssets'
sha = lambda data: hashlib.sha256(data).hexdigest()
matrix = (root / 'docs/componentes/MATRIZ-PROPRIEDADES.md').read_text(encoding='utf-8')
assert '45 schemas; 44 tipos no Add; 44 fachadas geradas' in matrix
facades = (root / 'managed/Astra.Scripting/Generated/Components.g.cs').read_text(encoding='utf-8')
for name in ['GravityField2D', 'WindField2D', 'DragField2D', 'RadialField2D']:
    assert 'struct ' + name in facades
    assert name in matrix
assert 'std::array<EditorCreationEntry,77>' in (root / 'native/editor/editor_creation_catalog.h').read_text(encoding='utf-8')
assert 'kUiIconCount = 251' in (root / 'native/ui/ui_icon_id.h').read_text(encoding='utf-8')
for log, marker in [('native-final.log', '4/4 scenarios passed'), ('prior-fields3d.log', '4/4 scenarios passed'), ('prior-bulk50.log', '4/4 scenarios passed')]:
    assert marker in (evidence / log).read_text(encoding='utf-8')
result = {
 'counts': {'components': 4, 'semantic_properties_per_type': 10, 'semantic_property_entries': 40, 'distinct_shared_properties': 8, 'public_runtime_api': 6, 'total': 50},
 'registry': {'schemas': 45, 'attachable_types': 44, 'generated_facades': 44, 'creation_recipes': 77, 'atlas_icons': 251, 'named_catalog_icons': 249},
 'abi': 33, 'field_payload_bytes_preserved': 64, 'body_payload_bytes_preserved': 48,
 'apk_sha256': sha(apk.read_bytes()), 'apk_size': apk.stat().st_size, 'packaged': [],
 'host': {'new_native_scenarios_passed': 4, 'prior_native_regressions_passed': 8, 'managed_protocol_tests_passed': 1, 'managed_abi_tests_passed': 2, 'script_compile': '0 warnings 0 errors', 'visual_renderer': 'Actual editor UI software raster with real font/atlas; not Vulkan scene', 'fixture': 'archive reopened, actual GameWorld loaded and Box2D fixed step passed'},
 'physical_device': {'tested_this_package': False, 'installed_this_package': False, 'managed_probe_execution': 'not executed on Android'},
 'editable_fixture': 'build/acceptance/Fields2D50-20261001',
 'performance': {'configuration': 'Host Debug; 96 Box2D bodies, 32 overlapping fields, 60 steps after 5 warm-up steps', 'mean_ms': 1.269, 'p95_ms': 1.871, 'max_ms': 2.254, 'log': 'benchmark.log'}
}
with zipfile.ZipFile(apk) as archive:
    for name in ['dotnet/Astra.Scripting.dll', 'dotnet/Aether.Rendering.dll', 'ui/astra-ui-icons.aeui']:
        data = archive.read('assets/' + name)
        matches = data == (generated / name).read_bytes()
        if not matches: raise RuntimeError('Packaged asset differs: ' + name)
        if name.endswith('Astra.Scripting.dll'):
            for symbol in [b'WindField2D', b'PhysicsField2DRuntime', b'PhysicsFieldSample2D']:
                assert symbol in data, 'Missing packaged SDK symbol: ' + repr(symbol)
        result['packaged'].append({'path': 'assets/' + name, 'size': len(data), 'sha256': sha(data), 'matches_generated_asset': matches})
    for name in ['lib/arm64-v8a/libaether_android.so', 'lib/arm64-v8a/libaether_transform.so']:
        data = archive.read(name)
        result['packaged'].append({'path': name, 'size': len(data), 'sha256': sha(data)})
(evidence / 'package-manifest.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print('APK SHA256', result['apk_sha256'])
print('SDK Vector2, Rendering and icon atlas packaged bytes match generated assets')
print('ABI33 package; physical device execution remains unverified')
