# Ocean environment asset pipeline

The ocean sample uses Kloppenheim 03 (Pure Sky), by Greg Zaal and Jarod Guest,
from https://polyhaven.com/a/kloppenheim_03_puresky (CC0 1.0).
The retained Hausdorf source is not the active environment.

Run `python tools/build-ocean-demo.py` from a Python environment with NumPy and
Pillow installed and FFmpeg/FFprobe available on PATH. This generates the mesh,
bathymetry, normal mip chains, sky, lighting metadata, GGX reflection probe and
BRDF lookup, then records output and source hashes in the sample manifest.
The source HDR is checked in; generation does not require network access.
Failure of the sky cooker fails the generator instead of selecting another sky.

The generic sky cooker accepts `--width` of 512, 1024, 2048 or 4096; height is
half the width. Default remains 1024 for existing callers. Ocean selects 2048.
Its RGBA8 full mip chain occupies approximately 10.67 MiB, versus 2.67 MiB at
1024. The 256-square RGBA16F reflection probe remains independently sized.
Sky and reflections originate from the same HDR and sun direction is detected
from that source. Runtime does not decode HDR files.

Validation: `python -m unittest discover -s tests/tools -p test_ocean_assets.py`
checks manifest hashes, source provenance, sky dimensions and full mip payload,
normal texture payloads, and bounded sample mesh density.

This is a static image-based environment, not a dynamic atmospheric simulation.
The visible sky is RGBA8 sRGB; the specular probe preserves HDR radiance.
Increasing panorama resolution alone does not address water optical composition,
wave simulation complexity, or prove sustained 120 FPS.

## Water optical composition

The tile-local depth path converts axial depth separation to distance along the
camera ray before Beer-Lambert attenuation. `surfaceOpacity` now scales optical
density rather than putting a fixed ceiling on coverage. At zero density the
body transmits the background; Fresnel reflection can still remain. At positive
density a sufficiently deep column approaches full opacity. This intentionally
changes the previous deep-water appearance without changing serialized fields.

The current blend uses scalar luminance transmittance; it cannot accurately
filter the background separately in R/G/B without reading scene color. Full HDR
refraction/composition remains separate work.

## Camera-relative graded surface

The sample now opts into `MapMaterialWaterCameraGrid` alongside `MapMaterialWater`.
The offline `water_geometry.py` helper generates a single graded grid. UV1 stores
incident-edge spacing and authoring half-extent; the loader validates this data.
The GPU scales offsets to the profile's maximum distance and anchors XZ to the
camera. This mode is world-horizontal and does not use the instance's XZ
translation/rotation/scale; finite meshes remain transformed as before.

At the sample defaults, central spacing remains 3 metres, with an 8 km half-extent
and 131,072 water triangles. Camera far distance is 12 km. This is finite coverage
following the camera, not mathematical infinity. Choose camera far distance to
cover the desired horizon. The CPU clipmap planner remains separate and unused.
Large distant cells smoothly suppress undersampled geometric spectrum bands;
normal detail remains mip-filtered. No per-frame geometry allocation/upload is
introduced. Conservative submission bypasses static bounds only for this explicit
camera-relative material contract.
