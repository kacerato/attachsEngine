"""Recipe-driven Blender FBX adapter; never runs on Android.

Usage: blender -b --python tools/convert-fbx-asset.py -- source-dir recipe.json output.glb
Material bindings, source model and normalization belong to the asset recipe.
"""
import pathlib
import json
import sys
import bpy
from mathutils import Vector, Matrix

source, recipe_path, destination = (pathlib.Path(value).resolve() for value in sys.argv[sys.argv.index('--') + 1:])
recipe = json.loads(recipe_path.read_text(encoding='utf8'))
if recipe.get('version') != 1:
    raise ValueError('Unsupported FBX import recipe version')

def source_path(relative):
    path = (source / relative).resolve()
    if not path.is_relative_to(source) or not path.is_file():
        raise ValueError('Missing or escaping asset path: ' + str(relative))
    return path

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source_path(recipe['model'])))
objects = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH']
points = [obj.matrix_world @ Vector(corner) for obj in objects for corner in obj.bound_box]
low = Vector(tuple(min(p[i] for p in points) for i in range(3)))
high = Vector(tuple(max(p[i] for p in points) for i in range(3)))
print('ASSET_SOURCE_BOUNDS', tuple(low), tuple(high))
length = recipe.get('targetHorizontalLength')
scale = float(length) / max(high.x-low.x, high.y-low.y) if length is not None else 1.0
if not 0 < scale < 100000:
    raise ValueError('Invalid normalization scale')
pivot = recipe.get('pivot', 'preserve')
if pivot not in ('preserve','bottomCenter'):
    raise ValueError('Unsupported pivot mode')
centre = Vector(((low.x+high.x)*.5, (low.y+high.y)*.5, low.z)) if pivot == 'bottomCenter' else Vector((0,0,0))
for obj in objects:
    world = obj.matrix_world.copy()
    obj.parent = None
    obj.matrix_world = Matrix.Scale(scale, 4) @ Matrix.Translation(-centre) @ world

for material in bpy.data.materials:
    print('ASSET_MATERIAL', material.name)
    binding = recipe.get('materials', {}).get(material.name)
    if binding is None:
        continue  # preserve imported FBX materials unless explicitly overridden
    material.use_nodes = True
    nodes, links = material.node_tree.nodes, material.node_tree.links
    nodes.clear()
    output = nodes.new('ShaderNodeOutputMaterial')
    bsdf = nodes.new('ShaderNodeBsdfPrincipled')
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])
    for suffix, socket in [('Base_color', 'Base Color'), ('Roughness', 'Roughness'),
                            ('Metallic', 'Metallic'), ('Normal_OpenGL', 'Normal'),
                            ('Opacity', 'Alpha'), ('Emissive', 'Emission Color')]:
        relative = binding.get('textures', {}).get(suffix)
        if relative is None:
            continue
        path = source_path(relative)
        tex = nodes.new('ShaderNodeTexImage')
        tex.image = bpy.data.images.load(str(path), check_existing=True)
        if suffix not in ('Base_color', 'Emissive'):
            tex.image.colorspace_settings.name = 'Non-Color'
        if suffix == 'Normal_OpenGL':
            normal = nodes.new('ShaderNodeNormalMap')
            links.new(tex.outputs['Color'], normal.inputs['Color'])
            links.new(normal.outputs['Normal'], bsdf.inputs['Normal'])
        else:
            links.new(tex.outputs['Color'], bsdf.inputs[socket])
        if suffix == 'Opacity':
            material.surface_render_method = 'DITHERED'
    material.use_backface_culling = not binding.get('doubleSided', False)
bpy.ops.object.select_all(action='DESELECT')
for obj in objects:
    obj.select_set(True)
    obj.modifiers.new('Export triangulation', 'TRIANGULATE')
bpy.context.view_layer.objects.active = objects[0]
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
destination.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.export_scene.gltf(filepath=str(destination), export_format='GLB',
                          use_selection=True, export_yup=True, export_apply=True, export_tangents=True)
