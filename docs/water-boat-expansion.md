# Water / floating asset expansion

## Execution order

1. Asset pipeline: inspect FBX and material slots; normalize to metres; preserve
   PBR maps; cook ahead of time; merge with explicit material/index remapping.
   Record supplied archive hash without inventing a redistribution licence.
2. Floating render instance: retain every primitive's local transform and bounds;
   move all hull/rope/sail primitives with one body pose. Keep baked collision
   separate. Start with an explicitly documented box proxy, not a claim of
   triangle-accurate naval hydrodynamics.
3. Wave authoring: independent swell length and directional spread, using one
   validated profile for GPU displacement and CPU water queries. Preserve old
   profile serialization. Expose controls in the mobile test panel.
4. Optical refinement: directional illumination of in-scattering, filtered foam
   breakup and contact transition; do not hide tone-mapping/refraction limits.
5. Host regression tests, release build, then Android install and visual review.

## Following stages, not completed by the initial boat integration

- Multi-section hull sampling with mass/centre-of-mass authoring and waterline
  exclusion shared by physics/rendering; rigid body propulsion and rudder forces.
- Asynchronous spectral surface queries with bounded latency and CPU/GPU clock
  agreement before attaching buoyancy to FFT rendering.
- Velocity-driven wake, persistent foam advection, spray events and underwater
  camera transitions. Each must have explicit capacity and quality controls.
- Linear-HDR scene-colour refraction and unified sky/ocean exposure, then spatial
  clipmaps with seam tests. No claim that existing LDR blending is full refraction.

References: https://github.com/2Retr0/GodotOceanWaves and
https://github.com/krautdev/GodotOceanWaves (conceptual reference, no source copied).

## Implemented laboratory controls

- Boat render model: 20 m long, uniform scale 2 relative to the normalized asset;
  box proxy 4.4 x 2.4 x 16 m, not the canopy's full visual bounding box.
- Analytic wave gain 0..12; FFT remains 0..3 within its validated resource budget.
- Still-water level -20..40 m, shared by renderer and physics queries.
- Optional long-wave amplitude 0..20 m and wavelength 120..2000 m, finite-depth
  dispersion at the test's 30 m reference depth. It occupies one wave slot;
  when full, the laboratory explicitly drops the last crossing-swell component.
- The ocean test reserves 128 m visibility displacement, including water-level
  offset. The general renderer rejects profiles outside its resource budget.

High-amplitude tests exceed linear wave theory's physical accuracy. They test
numerical/visual robustness, not tsunami forecasting, coastal run-up or flooding.
Underwater camera rendering and triangular hull hydrodynamics remain incomplete.

## Android validation, 2026-09-05

Release installed and SHA256 verified against device base.apk:
`9b34a2d06e20f6aebfd4c1d7faf583307ad3fb23c7dd6f1f303e4b4131ba0d87`.
412 native tests and 14 targeted Python tests passed. GPU shader binaries were
regenerated/validated. Device log reported scene ready and scale=1/dynamic=0.
Panel-tested long-wave amplitude 7.68 m, wavelength 870.12 m, water level 5.74 m;
no FATAL/VUID/invalid-water messages in the captured process log. This is a
smoke test, not validation of every extreme combination or long-term stability.

Captures: `build/android-validation/ocean-boat-stress.png` and
`build/android-validation/ocean-level-verified.png`. Strong waves can capsize the
box-proxy boat; high troughs reveal the authored shallow floor. The horizon
exposure seam remains visible. No claim of production-ready naval physics or
fully realistic water shading is made by this milestone.
