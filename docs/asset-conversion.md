# Offline asset conversion

`tools/convert-fbx-asset.py` is a recipe-driven **external-process** Blender
adapter, not runtime/editor coupling. `samples/boat/import.json` is a test asset
recipe; the tool has no boat material names or geometry behaviour.

## Dependency boundary

Pinned tools are recorded in `tools/asset-tools.lock.json`. The Blender portable
archive's SHA256 was checked against the official release checksum. It stays in
`build/tools`, outside Android packaging. Keep upstream notices with any tool
redistribution. The generated model does not embed Blender code.

Blender/ASTC conversion is currently a **Windows host pipeline**. Direct FBX
import on Android is not implemented. A future Android importer must consume
equivalent import settings and emit the same cooked resources; it must not spawn
a Windows executable or introduce FBX parsing into the render loop.

## Recipe v1

Pass source directory, recipe JSON, output GLB after Blender's `--` argument.
`model` and texture paths must resolve inside the source directory. Optional
`targetHorizontalLength` normalizes in metres; omitted preserves source units.
`pivot` is `preserve` or `bottomCenter`. Materials omitted from the recipe retain
their imported FBX definition. Explicit overrides bind PBR texture semantics;
normal/roughness/metallic/opacity use non-colour data. Triangulation precedes
tangent export. GLB export uses Y-up.

Then `cook-gltf-map.py` emits AEMAP/AETX with configurable provenance via
`--source-author`, `--source-license`, `--source-url`. The source licence text
must be supplied beside the destination. No licence is inferred from a model.
The boat source archive contains no redistribution grant; see its notice.

`map_instance_cooker.append_instance` merges a cooked level-zero instance with
resource remapping and independent index slices. It bakes node transforms and
keeps visual geometry out of baked static collision; the scene owns its dynamic
collider. Current sample binding is material-range based, not a persistent entity
binding: replacing that with serialized scene instance IDs remains future work.

## Current limits

No animations, skinning or arbitrary shader graphs are promised by this path.
Separate AO maps are retained in the user archive but are not yet bound by the
boat recipe. FBX material conversion warnings about combined texture samplers
are expected here because roughness/metallic channels share identical sampling.
Validate any asset with differing UV transforms/samplers individually.
