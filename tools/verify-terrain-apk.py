import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
import zipfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description='Audit the generated terrain APK without claiming runtime acceptance.')
parser.add_argument('--apk', type=Path, required=True)
parser.add_argument('--native', type=Path, required=True, help='Exact compiled libaether_android.so to compare with the APK')
parser.add_argument('--output-prefix', default='terrain',
                    help='Evidence filename prefix (default: iteration2; use device79 for APK 79).')
args = parser.parse_args()
if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,63}', args.output_prefix):
    parser.error('--output-prefix must use 1-64 letters, digits, underscores or hyphens and start with a letter or digit')
apk = args.apk.resolve()
sha = lambda b: hashlib.sha256(b).hexdigest()
with zipfile.ZipFile(apk) as z:
    assert z.testzip() is None, 'ZIP CRC failed'
    native = z.read('lib/arm64-v8a/libaether_android.so')
    assert native[:4] == b'\x7fELF' and native[18:20] == b'\xb7\x00', 'ARM64 ELF required'
    assert native == args.native.read_bytes(), 'packaged native runtime differs from this build'
    assert b'AETERRAIN9' in native, 'revision 9 terrain recipe with independent FX authoring missing'
    # Optimized literal comparisons need not retain AETERRAIN1 as contiguous ELF bytes.
    # Legacy compatibility is checked by the host migration test and device reopen flow.
    assert 'Terreno e voxel'.encode() in native, 'terrain menu missing'
    for i in range(4):
        assert (root / f'assets/terrain/layer-{i}.png').read_bytes() in native, f'terrain texture {i} missing'
    library = root / 'assets/terrain/library'
    catalog_bytes = (library / 'catalog.json').read_bytes()
    assert z.read('assets/terrain/library/catalog.json') == catalog_bytes, 'terrain catalog differs from branch'
    catalog = json.loads(catalog_bytes)
    original_bytes = 0
    for texture in catalog['textures']:
        relative = texture['runtime']
        packaged = z.read('assets/terrain/library/' + relative)
        assert packaged == (library / relative).read_bytes(), 'original terrain pixels changed: ' + relative
        assert sha(packaged) == texture['runtime_sha256'], 'terrain original hash differs: ' + relative
        original_bytes += len(packaged)
        thumbnail = texture['thumbnail']
        assert z.read('assets/terrain/library/' + thumbnail) == (library / thumbnail).read_bytes(), 'terrain thumbnail differs: ' + thumbnail
    assert len(catalog['materials']) == 82 and len(catalog['textures']) == 156, 'terrain catalog inventory differs'
    assert not any(name.startswith('assets/terrain/library/sources/') and name.lower().endswith('.psd') for name in z.namelist()), 'source PSD archives must not enter APK'
    atlas = z.read('assets/ui/astra-ui-icons.aeui')
    assert atlas == (root / 'assets/astra-visual/ui/astra-ui-icons.aeui').read_bytes(), 'atlas differs from branch'
    magic, version, width, height, icon_count = struct.unpack_from('<5I', atlas)
    assert magic == 0x49554541 and version == 1, 'AEUI atlas format invalid'
    pixel_offset = 20 + icon_count * 8
    assert len(atlas) == pixel_offset + width * height * 4, 'AEUI atlas size invalid'
    header = (root / 'native/ui/ui_icon_id.h').read_text(encoding='utf-8')
    assert int(re.search(r'kUiIconCount\s*=\s*(\d+)', header).group(1)) == icon_count, 'enum/atlas count differs'
    terrain_icons = {}
    for identifier, name in [('TerrainSolidBase', 'solid-base'), ('TerrainSurface', 'surface'),
                             ('TerrainManage', 'manage'), ('TerrainEdit', 'edit'),
                             ('TerrainRaise','raise'),('TerrainLower','lower'),('TerrainFlatten','flatten'),
                             ('TerrainSmooth','smooth'),('TerrainDig','dig'),('TerrainPaint','paint'),
                             ('TerrainPath','path'),('TerrainNoise','noise'),('TerrainTerrace','terrace'),
                             ('TerrainRestore','restore'),('TerrainSharpen','sharpen'),('TerrainHole','hole'),
                             ('TerrainBrushAxes','brush-axes'),('TerrainPathPoints','path-points'),
                             ('TerrainPathProfile','path-profile'),('TerrainRoundedBrush','rounded-brush')]:
        match = re.search(r'\b' + identifier + r'\s*=\s*(\d+)\s*,\s*//\s*terrain/' + name + r'\b', header)
        assert match, 'terrain authoring icon enum missing: ' + identifier
        index = int(match.group(1))
        assert 1 <= index <= icon_count, 'terrain icon index outside atlas: ' + identifier
        x, y, cell_width, cell_height = struct.unpack_from('<4H', atlas, 20 + (index - 1) * 8)
        assert cell_width == cell_height == 96 and x + cell_width <= width and y + cell_height <= height
        pixels = b''.join(atlas[pixel_offset + ((y + row) * width + x) * 4:
                                pixel_offset + ((y + row) * width + x + cell_width) * 4]
                          for row in range(cell_height))
        assert any(pixels[3::4]) and 0 in pixels[3::4], 'terrain icon is empty or lacks transparency: ' + identifier
        terrain_icons['terrain/' + name] = dict(enum=identifier, index=index,
                                                rect=[x, y, cell_width, cell_height], rgba_sha256=sha(pixels))
    for name in ['LICENSE-supplied.txt', 'manifest.json']:
        assert z.read('assets/licenses/terrain/' + name) == (root / 'assets/terrain' / name).read_bytes()
    managed = {}
    for name in ['Aether.Core.dll', 'Aether.Scene.dll', 'Aether.Rendering.dll', 'Astra.Scripting.dll']:
        data = z.read('assets/dotnet/' + name)
        assert data == (root / 'android/app/build/managed/rendering-net8.0-8.0.27' / name).read_bytes(), name + ' differs'
        managed[name] = sha(data)
    elf_alignment = {}
    for name in z.namelist():
        if '.so' not in name.rsplit('/', 1)[-1]:
            continue
        data = z.read(name)
        if data[:4] != b'\x7fELF':
            continue
        assert data[4:6] == b'\x02\x01', name + ' must be ELF64 little endian'
        phoff = struct.unpack_from('<Q', data, 32)[0]
        phsize, phnum = struct.unpack_from('<HH', data, 54)
        aligns = [struct.unpack_from('<Q', data, phoff + i * phsize + 48)[0]
                  for i in range(phnum) if struct.unpack_from('<I', data, phoff + i * phsize)[0] == 1]
        assert aligns, name + ' has no loadable segment'
        elf_alignment[name] = sorted(set(aligns))
    assert not any('VkLayer_khronos_validation' in n for n in z.namelist()), 'debug validation layer in release'
    result = dict(apk=str(apk), bytes=apk.stat().st_size, sha256=sha(apk.read_bytes()), native_sha256=sha(native),
                  atlas_sha256=sha(atlas), atlas_icon_count=icon_count, terrain_authoring_icons=terrain_icons,
                  terrain_materials=len(catalog['materials']), terrain_original_textures=len(catalog['textures']), terrain_original_bytes=original_bytes,
                  managed=managed, checks=['ZIP CRC', 'ARM64 ELF', 'AETERRAIN9 recipe marker', 'native library equality',
                  'terrain creation menu', 'four embedded terrain textures', '82 catalog materials and 156 original runtime textures with thumbnails and exact hashes', 'current UI atlas', 'terrain license and provenance',
                  'twenty terrain authoring icons and matching runtime enum',
                  'managed assembly equality', 'no Vulkan debug validation layer'],
                  elf_load_alignment=elf_alignment,
                  elf_16k_compatible=all(min(a) >= 16384 for a in elf_alignment.values()),
                  runtime_verified=False)
out = root / 'build/terrain-evidence' / f'{args.output_prefix}-apk-audit.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
print(json.dumps({k:result[k] for k in ('apk','bytes','sha256','atlas_icon_count','checks','elf_16k_compatible','runtime_verified')}, indent=2))
