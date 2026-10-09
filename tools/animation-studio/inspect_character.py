"""Inspect supplied FBX rigs with Blender without changing the source files.

blender -b --python tools/animation-studio/inspect_character.py -- file.fbx ...
"""
import json
import sys
from pathlib import Path
import bpy

for name in sys.argv[sys.argv.index('--') + 1:]:
    path = Path(name).resolve(strict=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path))
    print('ASTRA_CHARACTER_INSPECTION ' + json.dumps({
        'path': str(path),
        'armatures': [{'name': o.name, 'bones': len(o.data.bones),
                       'roots': [b.name for b in o.data.bones if b.parent is None],
                       'names': [b.name for b in o.data.bones]}
                      for o in bpy.context.scene.objects if o.type == 'ARMATURE'],
        'meshes': [{'name': o.name, 'vertices': len(o.data.vertices),
                    'armature': o.find_armature().name if o.find_armature() else None,
                    'materials': [m.name if m else None for m in o.data.materials],
                    'morphs': len(o.data.shape_keys.key_blocks) - 1 if o.data.shape_keys else 0}
                   for o in bpy.context.scene.objects if o.type == 'MESH'],
        'actions': [{'name': a.name, 'range': list(a.frame_range)} for a in bpy.data.actions],
        'images': [{'name': i.name, 'path': i.filepath} for i in bpy.data.images],
    }, ensure_ascii=False))
