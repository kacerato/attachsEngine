"""Blender 4.5 offline bake of real FBX geometry + Unity prefab materials.

blender -b --python tools/bake-synty-icons.py -- library [--limit N]
Writes transparent sprites, GLB resources and per-icon provenance locally.
Unity scene/lighting/shaders are not claimed as native engine resources.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path
import bpy
from mathutils import Vector

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("library", type=Path)
parser.add_argument("--limit", type=int, default=0)
parser.add_argument("--palettes-only", action="store_true")
args = parser.parse_args(sys.argv[sys.argv.index("--") + 1:])
library = args.library.resolve()
catalog = json.loads((library / "catalog.json").read_text(encoding="utf8"))
by_guid = {asset["guid"]: asset for asset in catalog["assets"]}
palette_outputs = []
(library / "palettes").mkdir(exist_ok=True)
for asset in catalog["assets"]:
    if Path(asset["path"]).suffix.lower() != ".png":
        continue
    output = library / "palettes" / Path(asset["path"]).name
    image = bpy.data.images.load(str(library / asset["output"]), check_existing=False)
    original_size = list(image.size)
    if max(image.size) > 1024:
        factor = 1024 / max(image.size)
        image.scale(round(image.size[0] * factor), round(image.size[1] * factor))
    image.filepath_raw = str(output)
    image.file_format = "PNG"
    image.save()
    palette_outputs.append({"guid": asset["guid"], "sourceSha256": asset["sha256"],
        "sourceSize": original_size, "size": list(image.size),
        "output": output.relative_to(library).as_posix(),
        "sha256": hashlib.sha256(output.read_bytes()).hexdigest()})
    bpy.data.images.remove(image)
(library / "baked-palettes.json").write_text(json.dumps(palette_outputs, indent=2), encoding="utf8")
print("SYNTY_PALETTES", len(palette_outputs), flush=True)
if args.palettes_only:
    raise SystemExit(0)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "BLENDER_WORKBENCH"
scene.render.resolution_x = scene.render.resolution_y = 256
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.render.image_settings.color_mode = "RGBA"
scene.render.film_transparent = True
scene.display.shading.light = "STUDIO"
scene.display.shading.color_type = "TEXTURE"
scene.display.shading.show_shadows = False
scene.display.shading.show_cavity = False
scene.display.shading.show_specular_highlight = True
scene.display.render_aa = "8"
scene.view_settings.view_transform = "Standard"
scene.view_settings.look = "None"
camera_data = bpy.data.cameras.new("IconCamera")
camera = bpy.data.objects.new("IconCamera", camera_data)
scene.collection.objects.link(camera)
scene.camera = camera
camera_data.type = "ORTHO"
materials = {}


def unity_material(guid):
    if guid in materials:
        return materials[guid]
    source = by_guid[guid]
    text = (library / source["output"]).read_text(encoding="utf8")
    texture = re.search(r"- _MainTex:\s*m_Texture:.*?guid: ([a-f0-9]+)", text)
    if not texture or texture.group(1) not in by_guid:
        raise ValueError("Material has no resolvable texture: " + source["path"])
    image_source = by_guid[texture.group(1)]
    # Cook the palette offline instead of making every model decode 16M pixels
    # on the phone. Originals remain intact; UVs and material bindings survive.
    cooked = next(row for row in palette_outputs if row["guid"] == image_source["guid"])
    image = bpy.data.images.load(str(library / cooked["output"]), check_existing=True)
    material = bpy.data.materials.new(Path(source["path"]).stem)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    bsdf = nodes.get("Principled BSDF")
    bsdf.inputs["Metallic"].default_value = 0
    bsdf.inputs["Roughness"].default_value = .8
    tex = nodes.new("ShaderNodeTexImage")
    tex.image = image
    tex.interpolation = "Closest"
    material.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    nodes.active = tex
    materials[guid] = material
    return material


results = []
for index, icon in enumerate(catalog["icons"]):
    if args.limit and index >= args.limit:
        break
    name = icon["name"]
    provenance = library / "bakes" / (name + ".json")
    sprite = library / "sprites" / (name + ".png")
    model = library / "models" / (name + ".glb")
    if provenance.exists() and sprite.exists() and model.exists():
        cached = json.loads(provenance.read_text(encoding="utf8"))
        if cached.get("textureDimension") == 1024:
            results.append(cached)
            continue
    source = by_guid[icon["modelGuid"]]
    prefab = by_guid[icon["prefabGuid"]]
    text = (library / prefab["output"]).read_text(encoding="utf8")
    material_block = re.search(r"m_Materials:\s*((?:- .*\n\s*)+)", text)
    if not material_block:
        raise ValueError("No prefab material binding: " + name)
    guids = re.findall(r"guid: ([a-f0-9]+)", material_block.group(1))
    bound_materials = [unity_material(guid) for guid in guids]
    bpy.ops.import_scene.fbx(filepath=str(library / source["output"]))
    objects = [obj for obj in bpy.context.scene.objects if obj != camera]
    meshes = [obj for obj in objects if obj.type == "MESH"]
    if not meshes:
        raise ValueError("FBX has no geometry: " + name)
    for obj in meshes:
        obj.data.materials.clear()
        for material in bound_materials:
            obj.data.materials.append(material)
        for polygon in obj.data.polygons:
            if polygon.material_index >= len(bound_materials):
                raise ValueError("Material slot mismatch: " + name)
    points = [obj.matrix_world @ Vector(corner) for obj in meshes for corner in obj.bound_box]
    low = Vector(tuple(min(point[i] for point in points) for i in range(3)))
    high = Vector(tuple(max(point[i] for point in points) for i in range(3)))
    center = (low + high) * .5
    radius = max((point - center).length for point in points)
    if radius <= 0:
        raise ValueError("Empty bounds: " + name)
    direction = Vector((2.5, -6, 2.5)).normalized()
    camera.location = center + direction * radius * 5
    camera.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
    camera_data.ortho_scale = radius * 2.35
    camera_data.clip_start = max(.0001, radius / 100)
    camera_data.clip_end = radius * 20
    sprite.parent.mkdir(exist_ok=True)
    model.parent.mkdir(exist_ok=True)
    scene.render.filepath = str(sprite)
    bpy.ops.render.render(write_still=True)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.export_scene.gltf(filepath=str(model), export_format="GLB",
                              use_selection=True, export_yup=True, export_apply=True)
    row = {"name": name, "sourceSha256": source["sha256"], "sourceGuid": source["guid"],
           "prefabGuid": prefab["guid"], "materialGuids": guids,
           "sprite": sprite.relative_to(library).as_posix(),
           "model": model.relative_to(library).as_posix(),
           "renderer": "Blender 4.5 Workbench / real geometry / RGBA",
           "textureDimension": 1024,
           "vertices": sum(len(obj.data.vertices) for obj in meshes),
           "bounds": [list(low), list(high)]}
    provenance.parent.mkdir(exist_ok=True)
    provenance.write_text(json.dumps(row, indent=2), encoding="utf8")
    results.append(row)
    for obj in objects:
        data = obj.data
        bpy.data.objects.remove(obj, do_unlink=True)
        if isinstance(data, bpy.types.Mesh) and data.users == 0:
            bpy.data.meshes.remove(data)
    print("SYNTY_BAKED", index + 1, len(catalog["icons"]), name, flush=True)
(library / "baked-catalog.json").write_text(json.dumps(results, indent=2), encoding="utf8")
print("SYNTY_COMPLETE", len(results), flush=True)
