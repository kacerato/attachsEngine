"""Recipe-driven skinned FBX -> GLB adapter for the engine's native importer.

Run with Blender 4.5: blender -b --python import_character.py -- package recipe.json result.glb
Sources stay untouched. Unlike the static-mesh adapter, this preserves bones,
hierarchy, bind poses, skin and morphs. It does not port Unity behaviours.
"""
import hashlib
import json
import math
import os
import struct
import sys
from pathlib import Path

import bpy
from mathutils import Vector

root, recipe_file, output = [Path(p).resolve() for p in sys.argv[sys.argv.index('--') + 1:]]
if output.suffix.lower() != '.glb' or output.is_relative_to(root):
    raise ValueError('Publish a GLB outside the original package; sources must remain untouched')
recipe = json.loads(recipe_file.read_text(encoding='utf-8-sig'))
if recipe.get('version') != 1:
    raise ValueError('Unknown character recipe version')
sources = {}


def source(relative):
    path = (root / relative).resolve(strict=True)
    if not path.is_relative_to(root) or not path.is_file():
        raise ValueError('Missing or escaping package dependency: ' + relative)
    sources[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
    return path


def curves(action):
    # Blender 4.5 layered actions keep the slot in a channel bag.
    if action.is_action_legacy:
        return list(action.fcurves)
    return [curve for layer in action.layers for strip in layer.strips
            for bag in strip.channelbags for curve in bag.fcurves]


bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source(recipe['model'])))
base_objects = set(bpy.context.scene.objects)
rigs = [o for o in base_objects if o.type == 'ARMATURE']
meshes = [o for o in base_objects if o.type == 'MESH']
if len(rigs) != 1 or not meshes or not any(o.find_armature() == rigs[0] for o in meshes):
    raise ValueError('Recipe requires exactly one real rig with skinned geometry')
rig = rigs[0]
base_bones = {b.name for b in rig.data.bones}
if len(base_bones) > 256:
    raise ValueError('Skin exceeds the current engine joint limit')
imported_clips = []


def retain_action(obj, name):
    if not obj.animation_data or not obj.animation_data.action:
        return False
    action = obj.animation_data.action
    slot = obj.animation_data.action_slot
    if not curves(action):
        raise ValueError('Empty animation action: ' + action.name)
    action.use_fake_user = True
    track = obj.animation_data.nla_tracks.new()
    track.name = name
    strip = track.strips.new(name, int(action.frame_range[0]), action)
    if not action.is_action_legacy:
        strip.action_slot = slot
    obj.animation_data.action = None
    imported_clips.append({'name': name, 'source': recipe['model'], 'target': obj.name,
                           'frames': list(action.frame_range), 'curves': len(curves(action))})
    return True


for obj in base_objects:
    retain_action(obj, recipe.get('defaultAnimationName', 'Imported'))

for entry in recipe.get('animations', []):
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(filepath=str(source(entry['file'])))
    added = set(bpy.context.scene.objects) - before
    animated = [o for o in added if o.type == 'ARMATURE' and o.animation_data
                and o.animation_data.action]
    if len(animated) != 1:
        raise ValueError('Animation FBX must contain one animated rig: ' + entry['file'])
    other = animated[0]
    if {b.name for b in other.data.bones} != base_bones:
        raise ValueError('Different skeleton: use explicit engine retargeting, not this adapter')
    # Equal names alone cannot establish rest-pose equivalence.
    for bone in rig.data.bones:
        peer = other.data.bones[bone.name]
        if (bone.parent.name if bone.parent else None) != (peer.parent.name if peer.parent else None):
            raise ValueError('Animation changes bone hierarchy: ' + bone.name)
        error = max(abs(bone.matrix_local[i][j] - peer.matrix_local[i][j])
                    for i in range(4) for j in range(4))
        if error > .001:
            raise ValueError('Animation changes rest pose; explicit retarget required: ' + bone.name)
    for owner in added:
        if not owner.animation_data or not owner.animation_data.action:
            continue
        matching = [rig] if owner == other else [o for o in base_objects
                    if owner.name == o.name or (owner.name.startswith(o.name + '.')
                                               and owner.name[len(o.name) + 1:].isdigit())]
        if len(matching) != 1:
            raise ValueError('Animated FBX object has no unique destination: ' + owner.name)
        target = matching[0]
        target.animation_data_create()
        target.animation_data.action = owner.animation_data.action
        if not owner.animation_data.action.is_action_legacy:
            target.animation_data.action_slot = owner.animation_data.action_slot
        retain_action(target, entry['name'])
        imported_clips[-1]['source'] = entry['file']
    for obj in added:
        bpy.data.objects.remove(obj, do_unlink=True)

# Explicit material recipes resolve stale workstation paths from FBX files.
# Every visible material requires a mapping: no silent white-texture fallback.
bindings = recipe.get('materials', {})
material_names = {m.name for o in meshes for m in o.data.materials if m}
if material_names - bindings.keys():
    raise ValueError('Material mappings missing: ' + ', '.join(sorted(material_names - bindings.keys())))
for material_name in sorted(material_names):
    material = bpy.data.materials[material_name]
    mapping = bindings[material_name]
    material.use_nodes = True
    nodes, links = material.node_tree.nodes, material.node_tree.links
    nodes.clear()
    shader = nodes.new('ShaderNodeBsdfPrincipled')
    shader.inputs['Roughness'].default_value = float(mapping.get('roughness', .8))
    shader.inputs['Metallic'].default_value = float(mapping.get('metallic', 0))
    destination = nodes.new('ShaderNodeOutputMaterial')
    links.new(shader.outputs['BSDF'], destination.inputs['Surface'])
    for key, socket in [('baseColor', 'Base Color'), ('normal', 'Normal')]:
        if key not in mapping:
            continue
        image = bpy.data.images.load(str(source(mapping[key])), check_existing=True)
        if key == 'normal':
            image.colorspace_settings.name = 'Non-Color'
        texture = nodes.new('ShaderNodeTexImage')
        texture.image = image
        if key == 'normal':
            normal = nodes.new('ShaderNodeNormalMap')
            links.new(texture.outputs['Color'], normal.inputs['Color'])
            links.new(normal.outputs['Normal'], shader.inputs[socket])
        else:
            links.new(texture.outputs['Color'], shader.inputs[socket])
    material.use_backface_culling = not mapping.get('doubleSided', False)

# Uniform normalization belongs to a shared root; never unparent skinned meshes
# or apply independent mesh transforms, which would destroy the bind contract.
points = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
low = Vector(tuple(min(p[i] for p in points) for i in range(3)))
high = Vector(tuple(max(p[i] for p in points) for i in range(3)))
height = high.z - low.z
target = float(recipe.get('heightMeters', height))
if not math.isfinite(target) or not .01 <= target <= 1000 or height <= 1e-8:
    raise ValueError('Invalid uniform height normalization')
container = bpy.data.objects.new(recipe['name'], None)
bpy.context.scene.collection.objects.link(container)
for obj in base_objects:
    if obj.parent is None:
        world = obj.matrix_world.copy()
        obj.parent = container
        obj.matrix_world = world
container.scale = (target / height,) * 3
container.location = Vector((-(low.x + high.x) / 2, -(low.y + high.y) / 2, -low.z)) * (target / height)
bpy.context.scene.render.fps = int(recipe.get('frameRate', 30))
output.parent.mkdir(parents=True, exist_ok=True)
temporary = output.with_name(output.stem + '.staging.glb')
bpy.ops.export_scene.gltf(filepath=str(temporary), export_format='GLB',
                          export_yup=True, export_apply=False, export_tangents=True,
                          export_animations=True, export_animation_mode='NLA_TRACKS',
                          export_force_sampling=True, export_image_format='AUTO')
if not temporary.is_file() or temporary.stat().st_size < 20:
    raise ValueError('Exporter did not produce a GLB')
blob = temporary.read_bytes()
if struct.unpack_from('<4sII', blob) != (b'glTF', 2, len(blob)):
    raise ValueError('Exporter produced an invalid GLB container')
json_size, json_tag = struct.unpack_from('<I4s', blob, 12)
if json_tag != b'JSON':
    raise ValueError('GLB JSON chunk missing')
gltf = json.loads(blob[20:20 + json_size])
if not gltf.get('skins') or not any('skin' in n and 'mesh' in n for n in gltf['nodes']):
    raise ValueError('Exporter lost the skinned mesh')
published_clips = {a.get('name') for a in gltf.get('animations', [])}
if {a['name'] for a in imported_clips} - published_clips:
    raise ValueError('Exporter lost an animation: ' + repr(published_clips))
preserved = all(hashlib.sha256((root / p).read_bytes()).hexdigest() == h for p, h in sources.items())
if not preserved:
    raise ValueError('A source changed during conversion; refusing publication')
os.replace(temporary, output)
report = {'version': 1, 'name': recipe['name'], 'sources': sources,
          'sourcePreserved': preserved,
          'format': 'glTF 2.0 / GLB', 'blender': bpy.app.version_string,
          'bones': sorted(base_bones), 'meshes': len(meshes), 'animations': imported_clips,
          'heightMeters': target, 'materials': sorted(material_names),
          'publishedAnimations': sorted(published_clips),
          'publishedSkinJoints': [len(s['joints']) for s in gltf['skins']],
          'outputSha256': hashlib.sha256(output.read_bytes()).hexdigest(),
          'limitations': ['Unity behaviours/controllers/shaders are not converted',
                          'Additional FBX clips require matching hierarchy and rest pose',
                          'Renderer limits: 256 joints, four retained skin influences']}
output.with_suffix('.import.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
print('ASTRA_CHARACTER_IMPORTED ' + json.dumps(report, ensure_ascii=False))
