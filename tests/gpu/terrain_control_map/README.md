# Terrain control-map GPU regression

Standalone host Vulkan test. Compiles the production `terrain_surface.glsl` and
`material_uv_transform.glsl`; publishes material rows with the production C++
`materialExtensionEntry` helper. It does not load an editor session or Android.

Configure with a host C++20 compiler, Vulkan headers and `glslc`:

```powershell
cmake -S tests/gpu/terrain_control_map -B build/terrain-control-gpu -G Ninja `
  -DCMAKE_CXX_COMPILER=clang++ -DVULKAN_HEADERS=<directory-containing-vulkan> `
  -DGLSLC=<path-to-glslc>
cmake --build build/terrain-control-gpu -j 1
ctest --test-dir build/terrain-control-gpu --output-on-failure
```

The test creates a 32×32 offscreen target and a 65×65 linear control texture.
Two coarse triangles have only corner vertices, with base-layer weights at
every vertex. The narrow painted stripe is therefore invisible to vertex-only
interpolation. Readback checks verify the stripe, its exterior, exact UV zero
and one with a repeat sampler, closed-side vertex fallback, absent material
extension, a shader compiled without the control-map macro, and asymmetric
scale/offset hits and misses. Each pass saves `terrain-control-mode-N.ppm` in
the shader build directory for visual review.

The PBR passes publish cold authoring data through production
`appendTerrainSurfaceTable`, then sample real albedo, normal, packed RGBA,
gradient and emission textures through `terrain_pbr.glsl`. Readback verifies
channel mapping/remapping/inversion, tint/saturation, world normals under object
transforms, normal strength, local/world triplanar coordinates, meter tiling,
rotation/offset, height blending, activation, RGBA remaps and color variation.

Unavailable Vulkan hardware or a required feature fails the executable; it
does not silently skip or report success from a CPU device. The texture table
has ten fixed entries and uses dynamically uniform indexing. This test covers
the production terrain fragment function and material-row publication. It
does not establish full renderer bindless allocation, GLB import, editor flow,
lighting, device performance or Android behavior. Control storage currently has
four weights; the 32-layer authoring schema does not imply 32 painted GPU layers.
