# Spectral water runtime

`renderer/water_fft` is the numerical reference for the spectral-water provider.
It is backend-neutral and does not allocate Vulkan resources or assume a scene.

`WaterFftPlan::initialize` accepts powers of two from 2 through 2048, builds
bit-reversal and twiddle tables, and preserves the existing plan on invalid
input. Transform calls require exact buffer dimensions. Forward transform uses
negative phase; inverse uses positive phase and normalization 1/N per axis.
Execution is allocation-free. Independent buffers can share a plan across
threads; initialization requires exclusive ownership.

`WaterSpectralField` owns initial amplitudes, dispersion frequencies and spatial
scratch. Initialization receives unshifted complex amplitudes, patch length and
depth in metres. Evolution uses finite-depth gravity dispersion and conjugate
opposite frequencies, followed by normalized inverse FFT. Returned samples are
row-major (X contiguous), in the amplitude units supplied by the caller. No
implicit FFT shift is used. Calls on one field must be serialized; sample views
are invalidated by reinitialization and overwritten by update.

Tests compare against a direct DFT, impulse response, inverse normalization,
invalid authoring, and a closed-form travelling wave at multiple times. This
provides independent CPU evidence for later GPU readback comparisons.

## Wind authoring

`WaterSpectrumSettings` initializes a deterministic TMA/JONSWAP spectrum with
wind speed (m/s), direction (radians from +X), fetch (metres), finite depth,
swell, directional spread, short-wave damping, wavelength band and seed.
Resolution is a power of two from 8 to 2048. Zero wind produces a flat field.
Invalid settings preserve the destination amplitudes. Generation allocates
only during initialization, not evolution; it is not a per-frame operation.
Amplitudes compensate the normalized inverse FFT, so resolution is not an
artistic wave-height multiplier. DC and Nyquist lines are excluded. Band limits
are inclusive; callers must avoid overlapping bands when composing cascades.
The implementation uses a specified integer PRNG and Box-Muller transform;
repeatability is tested within a platform, not promised bitwise across libm
implementations. Directional spectra follow the mathematical approach described
by [GodotOceanWaves](https://github.com/2Retr0/GodotOceanWaves/).

Tests also cover seeded generation, calm water, invalid input preservation,
wavelength filtering, directional energy and real-valued temporal evolution.

## Still required

Deterministic pass timing, project-resource serialization, editable structural
domains/resolutions, flow and spray remain required. The provider does not yet
establish parity with GodotOceanWaves; capability checks and analytical fallback
remain mandatory even after the successful Adreno validation.

## Surface channels

The reference now evolves height plus seven channels: horizontal displacement
X/Z, height slope X/Z, and horizontal derivatives XX/XZ/ZZ. Positive choppiness
compresses crests. Horizontal outputs use unit choppiness; consumers must scale
both displacement and its derivatives consistently. `jacobian` computes the
determinant of the complete horizontal mapping, including the mixed derivative;
values below one indicate compression, not automatically persistent foam.
Odd derivative symbols vanish at Nyquist. Invalid channel IDs return an empty
view; invalid Jacobian samples return the neutral determinant one.

The oblique travelling-wave regression checks displacement, slopes, all three
horizontal derivatives and mixed Jacobian against closed-form expressions.
Storage is allocated at initialization. This CPU oracle uses eight inverse
transforms per update and is NOT a mobile performance fallback: the production
GPU implementation needs packed transforms and explicit cascade budgets.

## Vulkan compute provider

`rhi/VulkanWaterSpectralCompute` owns one cascade's initial-mode buffer and
surface buffer plus evolution and inverse-transform kernels. It accepts
power-of-two sizes 8 through 256; the CPU oracle supports larger sizes but the
GPU provider explicitly rejects those. Creation checks compute workgroup limits,
8 KiB shared memory and storage-buffer range. Resources consume 52*N*N bytes
before allocator alignment, or 3.25 MiB at 256, including foam. Initial upload occurs once.

Eight real fields are packed into four complex transforms. One workgroup owns
each complete row or column; two inverse dispatches follow evolution. Shared
memory barriers separate butterflies, and buffer barriers order evolution,
row transform, column transform, graphics consumption and the next overwrite.
The output is two vec4 per sample, in the order defined by
`WaterSpectralSample`, followed by N*N floats of persistent foam;
the opt-in vertex variant samples this buffer directly.

The owner must record outside render passes, on a queue supporting graphics
and compute, serialize use of each instance, and complete all submissions
before reinitialization/destruction. Queue-family transfer and async compute
are not provided implicitly. No frame allocations, queue-idle waits or CPU
transforms are introduced by recording. Time is float seconds; long-session
phase precision still needs a measured strategy before production enablement.

Android launch option `aether.water_fft_probe=true` runs an isolated diagnostic
device/queue on a worker, with explicit fence and readback invalidation. It
compares all eight spatial channels plus foam to the CPU oracle at sizes 8, 128, 256 and times
0, 1, 60 seconds, reporting maximum absolute error and pass/fail under
`Aether.WaterProbe`. It does not run normally or load the other scene's assets.
The diagnostic ran on Adreno on September 5: all nine channels passed for
8/128/256 resolutions at all three times. Maximum errors respectively were
0.00000035, 0.00001417 and 0.00001925, below the 0.001 tolerance. This is
numerical evidence for the tested corpus, not a general visual quality guarantee.
Host reflection tests and SPIR-V validation only prove shader/layout contracts.

## Cascade authoring and generation

`renderer/water_cascades` generates one through four independent spectral bands,
each with wind settings, patch length, resolution, displacement scale and
choppiness. Bands are ordered by wavelength. Overlapping bands are rejected;
intentional gaps remain legal. A shared boundary belongs only to the longer-wave
band, avoiding duplicate energy at exact mode wavelengths. Settings remain
unchanged, and failed generation preserves the previous output.

The caller supplies an explicit GPU buffer budget. Generation validates the
current provider's 256 resolution ceiling and charges 52*N*N bytes per cascade;
allocator alignment and future textures require additional budgets at
resource creation. The test partitions a seeded spectrum and compares the
combined amplitudes against the unpartitioned realization at every mode.
No resolution/choppiness preset is forced onto all scenes.

## Render integration and capability fallback

`aether.water_fft=true` enables the provider for materials marked Water, not
for a particular scene name. `setWaterCascades` accepts validated authoring
before renderer initialization. Live mode reconfiguration now updates amplitudes
and angular frequencies in the existing mapped buffers after `vkDeviceWaitIdle`;
it does not recreate device, descriptors or pipelines. Persistent editor-resource
properties are still pending. Without explicit configuration, three 128²
bands cover 2–8, 8–32 and 32–2048 metres with patches 32, 128 and 2048 metres.
They use 2.4375 MiB of initial/output/history buffers before allocator alignment.

The Ocean Lab and Boat On Water templates now select this generic flag by
default. Existing projects retain the choice through their template id; the
runtime still falls back to the analytical provider when capabilities or
allocation reject FFT. This is a template policy, not a material-name or
scene-name branch in the renderer.

Only scenes containing water allocate these resources. Unsupported compute,
storage or memory creation falls back to the analytical provider with a log.
The graphics queue records evolution and both inverse passes for every active
cascade before rendering. `water_spectral.vert` uses the shared map vertex
implementation with separate storage bindings; the forest vertex shader has
no spectral bindings. Bilinear sampling wraps continuously in world XZ,
combines height/displacement/derivatives, and builds normals from both deformed
tangents. Existing local impulses still add to the surface. Geometric waves
fade by the shortest wavelength of each cascade when the mesh cannot resolve
them; filter-gradient normals and fragment-scale spectral detail remain work.

Finite water culling expands by an all-phase triangle-inequality displacement
bound computed at initialization. Camera-relative grids retain their existing
conservative submission policy. Resources are destroyed after device work
completes, together with the owning renderer epoch. Material optics controls
continue using the common water profile.

The latest captured ocean has near-field spectral detail and moving FFT-driven
bodies. The authored HDRI still has a bright horizon band. Ocean Lab and Boat On
Water launch spectral by default; capability failure remains analytical.

## Persistent spectral foam

Each cascade owns one scalar foam-history value per sample, appended after the
spatial output so FFT writes cannot erase it. A fourth compute dispatch follows
the inverse transforms and evaluates the displacement Jacobian at the same
scale/choppiness used by geometry. Source strength is growth multiplied by
clamped compression below a configurable threshold. The exact constant-source
solution of `dF/dt = source*(1-F)-decay*F` keeps history bounded and independent
of update rate; the CPU regression compares 30/60/120 updates over one second.

The first dispatch and time rewind reset history. Zero elapsed time preserves
it, and no-source regions decay exponentially. `record` advances its history
clock: recorded command buffers must be submitted in order, not discarded or
replayed as independent simulation updates. Reinitialization resets history.
The Android readback probe also checks foam against this CPU recurrence.

The spectral vertex variant bilinearly samples history; the fragment variant
uses it instead of instantaneous slope foam, while retaining depth-intersection
shore foam. Cascades combine coverage with max; this is per-band compression,
not the Jacobian of the combined spectrum. History is attached to parameter
space, without flow advection, spray coupling or foam mip filtering yet. These
limitations and the still-missing device validation prevent production parity.

## Live spectral controls

`WaterSpectralControls` provides backend-neutral displacement/choppiness gains,
time scale, orientation and optional global foam override. The renderer accepts
validated updates without reallocating spectra or descriptors. Authored
per-cascade foam remains active unless the override is explicitly selected.
The Android overlay publishes these values in its coherent JNI snapshot. Height,
speed, crest and direction sliders affect either provider; an FFT launch adds
foam compression/growth/decay sliders.

`WaterSpectrumAuthoringSettings` adds real spectrum regeneration for main wind,
fetch, depth, swell organization, directional spread and short-wave damping; a
second independent wave train has wind, relative direction, fetch, swell,
spread and energy. Displacement and choppiness are independent for the 2–8,
8–32 and 32–2048 metre bands. UI changes are deferred until touch release so a
drag cannot stall the render loop with repeated idle/rebuild cycles. Validation
is transactional; rejection preserves the previous modes. A successful update
increments `waterSpectrumRevision`, and `OceanValidation` replaces its complete
CPU mirror at the same simulation time before the next physics query.

Direction rotates sampling coordinates and transforms displacements, slopes
and the full derivative tensor back to world space. It rotates the existing
realization; the separate physical spectrum controls regenerate wind/fetch.
Speed integrates elapsed time
instead of multiplying the absolute clock, so slider edits do not jump phase.
Speed zero freezes spectral evolution and its foam clock; local impulses keep
their existing clock. Finite-water culling reserves the complete accepted live
gain envelope (height 0–3, choppiness gain 0–2) at initialization.

The panel queries an atomic runtime provider status when opened, showing
inactive/loading, analytical, spectral or fallback and disabling FFT-only
controls outside spectral mode. Device validation changed cross-wind live,
observed GPU revision 3 and the physics mirror adopting revision 3 immediately.
Per-band structural resolution/domain, persistence and background job scheduling
remain separate pending controls.

## Performance evidence and attribution

Two September 5 windows of 600 frames each reported 120.08/120.12 present FPS
and GPU means 6.95/6.98 ms, but visibility telemetry showed render scale 0.5.
This is not evidence of the requested visual quality at stable high FPS.
Schema 7 incorrectly charged pre-graphics FFT work to the first Shadow marker;
its approximately 0.93 ms shadow figure in that capture is not shadow cost.
Schema 8 adds `gpu_water_simulation_ms` before Shadow, bracketing all cascade
evolution/IFFT/foam work. Compute classes are excluded from the raster tile
attribution-collapse heuristic. Existing metric names remain unchanged.

Schema 8 was built, installed and measured: PID 17069, epoch 4, window 2
reported simulation mean 0.9191 ms / p95 0.9806 ms; Shadow was 0.0008 ms.
Post mean was 2.3059 ms. Render scale remained exactly 0.5 throughout the
window, and thermal pressure was none. Main raster work appeared in Opaque
(3.7203 ms), with other in-pass timestamps near zero; these tile-deferred
subpass timings must not be read as proof that water shading costs zero.

## Sub-grid normal detail

The spectral material now evaluates the spectral slopes excluded by the
geometry filter when those wavelengths remain resolvable by screen pixels.
Undisplaced world XZ and mesh spacing are interpolated from the vertex stage;
fragment derivatives set a separate pixel-footprint fade. The complement of
the geometric weight prevents adding the same slope twice. Orientation is
applied consistently with vertex sampling. This fixes the structural loss of
the 2–8 m band on the approximately 3 m near grid without increasing triangles.
It adds storage-buffer reads in fragment shading and still needs device
performance/visual validation; hardware-filtered mip textures are a potential
replacement if those reads exceed the mobile budget. Horizontal small-wave
folding and accumulated slope variance for specular filtering remain pending.

Device validation of the first fragment implementation (PID 31004, epoch 4,
window 2) showed 76.31 FPS / GPU mean 11.6084 ms at render scale 0.5. Raster
cost rose to 7.2315 ms; simulation was 1.1212 ms and post 3.2474 ms. Clocks
differed from earlier runs, so this is not a controlled isolated delta, but
the implementation fails the overall budget. The screenshot
`build/android-validation/water-pixel-detail.png` shows added ripples but the
pale body and horizon band persist. This is not an accepted production result.

The fragment accessor now reads only the two slope scalars from each of four
neighbors rather than eight surface floats per neighbor. Vertex sampling keeps
its original complete data layout. This reduces requested values without
changing bilinear interpolation, but cache-line traffic and compiler behavior
must be measured; no performance recovery is claimed yet. A sampled normal
texture with hardware filtering remains the likely next step if needed.

The scalar accessor build was installed and measured (PID 25261, epoch 4,
window 3, 600 frames): 111.443 FPS, GPU mean 7.7812 ms / p95 8.3164 ms,
simulation mean 0.9229 ms, raster aggregate 4.5265 ms, post 2.3260 ms. Scale
stayed at 0.5 and thermal pressure was none. Different simulation/post timings
indicate clock differences versus PID 31004, so this is not a controlled
speedup attribution. It still misses the 120 FPS GPU budget and quality target.
The next performance step should replace fragment buffer interpolation with
hardware-filtered slope textures, not discard the restored near-field detail.

## Hardware slope filtering implementation

The provider now packs slopes into an RGBA32F storage/sampled image per cascade,
then transitions it for fragment sampling. Linear repeat sampling plus a
half-texel offset reproduces the existing grid-node bilinear convention.
Fragment shaders use static sampler bindings 11–14; vertex shaders retain
buffer bindings 7–10. Format storage/sampling/linear-filter support and sampler
limits are checked before enabling this provider; otherwise analytical fallback
is retained. No extended storage-format feature is assumed.

The current resource estimate is 68*N*N bytes including images (3.1875 MiB for
three 128² cascades), superseding the earlier 52-byte estimate above. Images
add 768 KiB for those cascades, plus alignment. Each frame adds a packing
dispatch and image barriers; actual performance and image equivalence need
device validation. Mip filtering is not implemented yet; pixel-footprint fade
remains active. The historical buffer-budget API now accounts for these images
as required resources too.
