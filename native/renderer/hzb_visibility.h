#pragma once

#include "core/base.h"
#include "renderer/frustum_visibility.h"

#include <vector>

namespace ae::renderer {

// Hierarchical-Z (Hi-Z) conservative occlusion culling, CPU-side decision
// layer. The pyramid itself may be populated two ways: buildHzbPyramid()
// below (a pure CPU reference, used by tests and available as a fallback/
// validation oracle) or, at runtime, from a small GPU-built mip chain read
// back one frame late -- see PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md and the
// Vulkan integration in InstancedRenderer for that half, which this header
// intentionally knows nothing about (no VkImage/VkBuffer dependency here).
//
// Mip convention (standard Hi-Z, e.g. Greene/Kass/Zorin): the base level
// stores, per texel, the depth of the NEAREST surface actually drawn there
// (ordinary depth-buffer semantics: smaller = closer to camera). Each coarser
// mip stores the MAX (farthest) of its 2x2 children. A candidate is occluded
// only when its own nearest possible depth is farther than EVERY texel in the
// footprint it covers -- i.e. farther than the MAX of that footprint. This is
// what makes the test conservative: a single unoccluded texel in the
// footprint is enough to keep the candidate visible.

struct HzbMipLevel final {
  u32 width = 0;
  u32 height = 0;
  u32 offset = 0; // Index into HzbPyramid::texels where this level begins.
};

struct HzbPyramid final {
  std::vector<float> texels; // Flat storage, base level (mips[0]) first.
  std::vector<HzbMipLevel> mips;
  bool valid = false;
};

// Pure CPU reference builder: reduces a base level into a full chain down to
// 1x1 via exact 2x2 max, no filtering. Used by tests and as a fallback/
// validation oracle -- the runtime path may instead receive an already-built,
// shorter chain from the GPU (see hzbPyramidFromReadback in the Vulkan
// integration, not part of this pure layer).
bool buildHzbPyramid(const float *baseTexels, u32 baseWidth, u32 baseHeight, HzbPyramid &out);

// CPU-testable reference for the GPU's first reduction pass: maps the real
// (arbitrary, non-power-of-two) depth resolution down to a smaller base level
// by taking the MAX depth within each output texel's exact pixel footprint.
// Used to validate hzb_reduce_first.frag once hardware is available; also
// exercised directly by native tests today.
bool buildHzbBaseLevelBlockMax(const float *fullResDepth, u32 fullWidth, u32 fullHeight,
                               u32 baseWidth, u32 baseHeight, std::vector<float> &outBaseLevel);

struct HzbLevelDims final {
  u32 width = 0;
  u32 height = 0;
};

// Tightly-packed, finest-to-coarsest flat layout for a caller-chosen list of
// level dimensions. Shared by the Vulkan integration (which needs this to
// plan each level's vkCmdCopyImageToBuffer destination offset while building
// the reduction chain) and hzbPyramidFromLevels below (which reconstructs the
// CPU pyramid from readback bytes written using this exact layout) so the two
// sides can never disagree about where a level lives in the buffer.
void computeHzbLevelOffsets(const HzbLevelDims *dims, u32 levelCount, std::vector<HzbMipLevel> &outMips);

// Reconstructs a pyramid from GPU-readback float texels for an explicit list
// of level dimensions (finest first), using the exact layout
// computeHzbLevelOffsets defines. Unlike buildHzbPyramid (which always
// derives a full chain down to 1x1 from ITS OWN reduction), this accepts
// whatever shorter chain the GPU actually built -- see
// InstancedRenderer's HZB reduction pass, not part of this pure layer.
// Fails closed (matching decodeCameraRoute/decodeMapPackage discipline: this
// is input driving a culling decision, not visibility data that should fail
// open) on a float count mismatch or any non-finite texel.
bool hzbPyramidFromLevels(const float *floatTexels, usize floatCount, const HzbLevelDims *dims,
                          u32 levelCount, HzbPyramid &out);

// Validates the invariant produced by both raster and compute HZB paths:
// every texel after level zero must be the exact maximum of its clamped 2x2
// footprint in the previous level. This deliberately does not validate level
// zero (its arbitrary-resolution depth footprint is validated separately by
// buildHzbBaseLevelBlockMax). Used by hardware validation readback only; the
// production GPU-only path never maps the pyramid to the CPU.
bool validateHzbMaxReductionChain(const HzbPyramid &pyramid, float tolerance = 0.0f);

struct HzbScreenRect final {
  // Normalized [0,1] screen-space rectangle (0,0 = top-left of the frustum's
  // projected view), already clamped to the visible range.
  float minU = 0.0f;
  float maxU = 0.0f;
  float minV = 0.0f;
  float maxV = 0.0f;
  // Conservative nearest depth of the tested bounds in the SAME normalized
  // Vulkan [0,1] depth space stored by the depth attachment/HZB (standard
  // convention: smaller = closer to camera). Never store view-space metres
  // here: comparing those directly with sampled depth would cull virtually
  // every object beyond one metre.
  float nearDepth = 0.0f;
  // false => degenerate/unsafe input (near-plane straddle, invalid frustum,
  // non-finite bounds). Callers must treat an invalid rect as visible,
  // matching frustum_visibility's fail-open discipline for uncertain data.
  bool valid = false;
};

// Maps camera NDC into the actual depth attachment orientation. Vulkan
// Android swapchains commonly use a pre-rotation, so the HZB image can be
// portrait-shaped while the visible display is landscape. This is the same
// 2x2 transform consumed by dirt_road.vert before rasterization.
struct HzbScreenTransform final {
  float xx = 1.0f;
  float xy = 0.0f;
  float yx = 0.0f;
  float yy = 1.0f;
  // Physical normalized viewport AFTER surface rotation. The pyramid spans
  // the full depth attachment, including clear pixels outside this view.
  float viewportX = 0, viewportY = 0, viewportWidth = 1, viewportHeight = 1;
};

// Projects a conservative (frustum.boundsScale/boundsMargin-expanded) world
// bounding sphere into the HZB's screen space. Deliberately conservative, not
// exact: a sphere straddling or closer than the near plane is reported
// invalid rather than risk an undersized rectangle, and the rectangle itself
// encloses both depth faces of its view-space bounding box. Projecting only
// the nearest face underestimates bounds entirely on one side of the axis.
HzbScreenRect projectBoundsToHzbScreenRect(const PerspectiveFrustum &frustum, const float center[3],
                                           float radius,
                                           const HzbScreenTransform &screenTransform = {});

// Conservative occlusion test: true only when every texel covering rect in
// the selected mip level is farther than rect.nearDepth. Fails open (returns
// false, i.e. "not occluded") for any invalid/empty pyramid or rect.
bool isOccludedByHzb(const HzbPyramid &pyramid, const HzbScreenRect &rect,
                     float normalizedDepthBias = 0.0f);

// Fixed-cost guard for the current CPU-readback implementation. A zero
// minimum is useful for validation runs; production policy should choose a
// measured threshold. Invalid/empty workloads never run the stage.
bool shouldRunHzb(u32 candidateDraws, u32 minimumCandidateDraws);

struct HzbHysteresisState final {
  u32 occludedStreak = 0;
};

// Advances one object's hysteresis state by one frame's occlusion test and
// returns whether the object should be treated as visible THIS frame.
// - Any non-occluded test resets the streak and reports visible immediately
//   (no delay reviving a newly-visible object -- never a one-frame pop).
// - An occluded test only reports "cull" (false) once occludedStreak reaches
//   hysteresisFrames; until then the object stays visible (grace period).
// - A hysteresisFrames of 0 means no grace period (cull on the first
//   occluded test); the shipped default budget is expected to be >0.
// - An object never tested keeps occludedStreak==0 and therefore starts
//   visible, matching the fail-open discipline used throughout visibility.
bool updateHzbHysteresis(HzbHysteresisState &state, bool occludedThisFrame, u32 hysteresisFrames);

} // namespace ae::renderer
