import bpy,sys,math,json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
for name in sys.argv[sys.argv.index('--')+1:]:
    source=root/'project/Assets'/f'{name}.glb'
    original=root/'sources'/f'{name}-original.glb';original.parent.mkdir(exist_ok=True,parents=True)
    if not original.exists():source.replace(original)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(original))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    triangles=sum(len(o.data.polygons) for o in meshes)
    for o in meshes:
        count=len(o.data.polygons)
        if count>1200:
            bpy.context.view_layer.objects.active=o
            m=o.modifiers.new('Mobile geometry budget','DECIMATE');m.ratio=min(1,14000/max(1,triangles));m.use_collapse_triangulate=True
            bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.ops.export_scene.gltf(filepath=str(source),export_format='GLB',export_materials='EXPORT',export_image_format='AUTO',export_yup=True)
    print('OPTIMIZED',name,triangles,'->',sum(len(o.data.polygons) for o in meshes),source.stat().st_size,flush=True)
