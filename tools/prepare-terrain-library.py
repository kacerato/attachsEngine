#!/usr/bin/env python3
"""Import supplied TerrainLayer/MicroSplat materials without resizing originals.

Requires Pillow, PyYAML and UnityPy (for binary Unity TerrainLayer files).
Source archives are extracted with extract-terrain-packages.py first. GUIDs
resolve through those manifests, never by guessing similarly named textures.
The catalog preserves every source property, channel and cluster association.
Optional resolution derivatives are separate, explicit --profile arguments.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path
from PIL import Image
import yaml
import UnityPy
from terrain_material_contracts import resolve_catalog

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--packages', type=Path, default=ROOT/'build/terrain-packages')
parser.add_argument('--output', type=Path, default=ROOT/'assets/terrain/library')
parser.add_argument('--profile', type=int, action='append', default=[])
args = parser.parse_args()
if any(size not in (512, 1024, 2048, 4096) for size in args.profile):
    parser.error('Profiles must be explicit 512, 1024, 2048 or 4096 limits.')

entries = []
by_guid = {}
for manifest in sorted(args.packages.glob('*/manifest.json')):
    data = json.loads(manifest.read_text(encoding='utf-8'))
    for entry in data['entries']:
        item = dict(entry, package=data['package'], archive_sha256=data['sha256'], root=manifest.parent)
        entries.append(item)
        by_guid.setdefault(entry['guid'], []).append(item)
for folder in ('sources', 'textures', 'thumbnails', 'profiles'):
    (args.output/folder).mkdir(parents=True, exist_ok=True)

textures, materials, unresolved = {}, [], []

def source_file(item):
    return item['root']/item['path']

def texture(guid, owner):
    if not guid or guid == '0'*32:
        return None
    candidates = by_guid.get(guid, [])
    local = [e for e in candidates if e['package'] == owner['package']]
    candidates = local or candidates
    if not candidates:
        unresolved.append(dict(owner=owner['path'], guid=guid))
        return None
    hashes = {e['sha256'] for e in candidates}
    if len(hashes) != 1:
        raise ValueError(f'Ambiguous GUID {guid} in {owner["path"]}')
    entry = candidates[0]
    digest = entry['sha256']
    if digest in textures:
        return digest
    path = source_file(entry)
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise ValueError(f'Source hash changed: {path}')
    relative = f'sources/{digest}{path.suffix.lower()}'
    if not (args.output/relative).exists() or not (args.output/relative).samefile(path):
        shutil.copyfile(path, args.output/relative)
    # Decode full dimensions; this is a lossless format conversion, not a
    # device-quality policy. Preserve a 16-bit single-channel map as 16-bit PNG.
    with Image.open(path) as image:
        image.load()
        original_mode = image.mode
        converted = image.copy() if image.mode in ('RGB', 'RGBA', 'L', 'I;16', 'I') else image.convert('RGBA')
        # PNG sources are already directly usable. Reuse the exact original
        # bytes instead of keeping a second full-resolution PNG conversion.
        runtime = relative if path.suffix.lower() == '.png' else f'textures/{digest}.png'
        if not (args.output/runtime).exists():
            converted.save(args.output/runtime, compress_level=6)
        thumbnail = converted.convert('RGBA')
        thumbnail.thumbnail((160, 160), Image.Resampling.LANCZOS)
        thumbnail.save(args.output/f'thumbnails/{digest}.png')
        profiles = {}
        for limit in args.profile:
            derivative = converted.copy()
            derivative.thumbnail((limit, limit), Image.Resampling.LANCZOS)
            target = f'profiles/{limit}/{digest}.png'
            (args.output/target).parent.mkdir(parents=True, exist_ok=True)
            derivative.save(args.output/target)
            profiles[str(limit)] = dict(path=target, dimensions=list(derivative.size))
        textures[digest] = dict(id=digest, guid=guid, source=relative, source_path=entry['path'],
            package=entry['package'], archive_sha256=entry['archive_sha256'], source_sha256=digest,
            runtime=runtime, runtime_sha256=hashlib.sha256((args.output/runtime).read_bytes()).hexdigest(),
            dimensions=list(image.size), source_mode=original_mode, runtime_mode=converted.mode,
            thumbnail=f'thumbnails/{digest}.png', profiles=profiles)
    return digest

def read_asset(entry):
    path = source_file(entry)
    content = path.read_bytes()
    if content.startswith(b'%YAML'):
        text = re.sub(r'^%.*\n|^--- !u!.*\n', '', content.decode('utf-8'), flags=re.M)
        return next(iter(yaml.safe_load(text).values())), None
    env = UnityPy.load(str(path))
    for obj in env.objects:
        if obj.type.name in ('TerrainLayer', 'MonoBehaviour'):
            return obj.read_typetree(), obj.assets_file.externals
    raise ValueError(f'No supported material data in {path}')

def guid_of(pointer, externals=None):
    if not isinstance(pointer, dict):
        return None
    if externals is None:
        return pointer.get('guid')
    index = pointer.get('m_FileID', 0)
    # Unity's serialized GUID bytes encode low nibble first (different from
    # UUID byte order). YAML GUID tokens already use the editor representation.
    if not 0 < index <= len(externals):
        return None
    return ''.join(f'{b & 15:x}{b >> 4:x}' for b in externals[index-1].guid)

for entry in entries:
    name = entry['path']
    if name.endswith('.terrainlayer'):
        data, externals = read_asset(entry)
        channels = {}
        for source, target, space in (
            ('m_DiffuseTexture','albedo','sRGB'), ('m_NormalMapTexture','normal','linear'),
            ('m_MaskMapTexture','mask','linear')):
            identity = texture(guid_of(data.get(source), externals), entry)
            if identity:
                channels[target] = dict(texture=identity, color_space=space)
        materials.append(dict(id=entry['guid'], name=data.get('m_Name',Path(name).stem),
            package=entry['package'], source=name, kind='TerrainLayer', maps=channels,
            source_properties=data, mask_contract='R metallic; G AO; B height; A smoothness'))
    elif name.endswith('/MicroSplatConfig.asset'):
        data, externals = read_asset(entry)
        for collection in ('sourceTextures', 'sourceTextures2', 'sourceTextures3'):
            for index, layer in enumerate(data.get(collection) or []):
                channels = {}
                for channel, pointer in layer.items():
                    if not isinstance(pointer, dict):
                        continue
                    guid = guid_of(pointer, externals)
                    # A TerrainLayer reference is a material dependency, not an image.
                    if channel == 'terrainLayer':
                        continue
                    identity = texture(guid, entry)
                    if identity:
                        channels[channel] = dict(texture=identity,
                            color_space='sRGB' if channel in ('diffuse','emis','traxDiffuse') else 'linear',
                            channel=layer.get(channel+'Channel'))
                if not channels:
                    continue
                diffuse = channels.get('diffuse',{}).get('texture')
                label = Path(textures[diffuse]['source_path']).stem if diffuse else f'Layer {index+1}'
                materials.append(dict(id=f'{entry["guid"]}-{collection}-{index}',name=label,
                    package=entry['package'],source=name,kind='MicroSplat',cluster_set=entry['guid'],
                    cluster_variant={'sourceTextures':0,'sourceTextures2':1,'sourceTextures3':2}[collection],
                    cluster_layer=index,maps=channels,source_properties=layer,
                    packing={key:data.get(key) for key in ('packingMode','pbrWorkflow','diffuseIsLinear','clusterMode',
                        'allTextureChannelHeight','allTextureChannelSmoothness','allTextureChannelAO')}))

catalog = dict(version=1, default_profile='original', originals_preserved=True,
    optional_profiles=args.profile, textures=list(textures.values()), materials=materials,
    unresolved_dependencies=unresolved)
resolve_catalog(catalog)
(args.output/'.gitignore').write_text('# Obsolete duplicate PNG conversions; runtime uses the exact original.\n'+''.join(f'/textures/{t["id"]}.png\n' for t in catalog['textures'] if t['runtime']==t['source']),encoding='utf-8')
(args.output/'catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
shutil.copyfile(ROOT/'assets/terrain/LICENSE-supplied.txt',args.output/'LICENSE-supplied.txt')
print(json.dumps(dict(materials=len(materials),unique_textures=len(textures),
    unresolved=len(unresolved),original_bytes=sum((args.output/t['source']).stat().st_size for t in textures.values()),
    default_profile='original',optional_profiles=args.profile)))
