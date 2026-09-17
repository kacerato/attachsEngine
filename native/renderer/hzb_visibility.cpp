#include "renderer/hzb_visibility.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ae::renderer {
namespace {

// Coarsest-to-finest scan budget: the total texel count tested at the chosen
// mip level never exceeds this, keeping the CPU cost of testing hundreds of
// chunks per frame negligible regardless of how the pyramid was built.
constexpr u32 MaximumScannedTexels = 64;

u32 pixelIndex(u32 x, u32 y, u32 width) { return y * width + x; }

} // namespace

bool buildHzbPyramid(const float *baseTexels, u32 baseWidth, u32 baseHeight, HzbPyramid &out) {
  out = HzbPyramid{};
  if (baseTexels == nullptr || baseWidth == 0 || baseHeight == 0) return false;
  for (usize index = 0; index < static_cast<usize>(baseWidth) * baseHeight; ++index) {
    if (!std::isfinite(baseTexels[index])) return false;
  }

  HzbPyramid pyramid;
  u32 width = baseWidth;
  u32 height = baseHeight;
  usize total = static_cast<usize>(baseWidth) * baseHeight;
  // Count levels first (down to 1x1) so the flat buffer can be sized once.
  {
    u32 w = baseWidth, h = baseHeight;
    while (w > 1 || h > 1) {
      w = (w + 1) / 2;
      h = (h + 1) / 2;
      total += static_cast<usize>(w) * h;
    }
  }
  pyramid.texels.resize(total);
  pyramid.mips.push_back({baseWidth, baseHeight, 0});
  std::copy(baseTexels, baseTexels + static_cast<usize>(baseWidth) * baseHeight, pyramid.texels.begin());

  u32 previousOffset = 0;
  while (width > 1 || height > 1) {
    const u32 nextWidth = (width + 1) / 2;
    const u32 nextHeight = (height + 1) / 2;
    const u32 nextOffset = previousOffset + width * height;
    for (u32 y = 0; y < nextHeight; ++y) {
      for (u32 x = 0; x < nextWidth; ++x) {
        const u32 x0 = x * 2;
        const u32 y0 = y * 2;
        const u32 x1 = std::min(x0 + 1, width - 1);
        const u32 y1 = std::min(y0 + 1, height - 1);
        float value = pyramid.texels[previousOffset + pixelIndex(x0, y0, width)];
        value = std::max(value, pyramid.texels[previousOffset + pixelIndex(x1, y0, width)]);
        value = std::max(value, pyramid.texels[previousOffset + pixelIndex(x0, y1, width)]);
        value = std::max(value, pyramid.texels[previousOffset + pixelIndex(x1, y1, width)]);
        pyramid.texels[nextOffset + pixelIndex(x, y, nextWidth)] = value;
      }
    }
    pyramid.mips.push_back({nextWidth, nextHeight, nextOffset});
    previousOffset = nextOffset;
    width = nextWidth;
    height = nextHeight;
  }

  pyramid.valid = true;
  out = std::move(pyramid);
  return true;
}

bool buildHzbBaseLevelBlockMax(const float *fullResDepth, u32 fullWidth, u32 fullHeight, u32 baseWidth,
                               u32 baseHeight, std::vector<float> &outBaseLevel) {
  outBaseLevel.clear();
  if (fullResDepth == nullptr || fullWidth == 0 || fullHeight == 0 || baseWidth == 0 || baseHeight == 0)
    return false;
  for (usize index = 0; index < static_cast<usize>(fullWidth) * fullHeight; ++index) {
    if (!std::isfinite(fullResDepth[index])) return false;
  }

  std::vector<float> result(static_cast<usize>(baseWidth) * baseHeight);
  for (u32 by = 0; by < baseHeight; ++by) {
    const u32 y0 = static_cast<u32>(static_cast<u64>(by) * fullHeight / baseHeight);
    const u32 y1 = std::max(y0 + 1, static_cast<u32>(static_cast<u64>(by + 1) * fullHeight / baseHeight));
    const u32 yEnd = std::min(y1, fullHeight);
    for (u32 bx = 0; bx < baseWidth; ++bx) {
      const u32 x0 = static_cast<u32>(static_cast<u64>(bx) * fullWidth / baseWidth);
      const u32 x1 = std::max(x0 + 1, static_cast<u32>(static_cast<u64>(bx + 1) * fullWidth / baseWidth));
      const u32 xEnd = std::min(x1, fullWidth);
      float value = fullResDepth[pixelIndex(x0, y0, fullWidth)];
      for (u32 y = y0; y < yEnd; ++y) {
        for (u32 x = x0; x < xEnd; ++x) {
          value = std::max(value, fullResDepth[pixelIndex(x, y, fullWidth)]);
        }
      }
      result[pixelIndex(bx, by, baseWidth)] = value;
    }
  }
  outBaseLevel = std::move(result);
  return true;
}

void computeHzbLevelOffsets(const HzbLevelDims *dims, u32 levelCount, std::vector<HzbMipLevel> &outMips) {
  outMips.clear();
  if (dims == nullptr) return;
  outMips.reserve(levelCount);
  u32 offset = 0;
  for (u32 level = 0; level < levelCount; ++level) {
    outMips.push_back({dims[level].width, dims[level].height, offset});
    offset += dims[level].width * dims[level].height;
  }
}

bool hzbPyramidFromLevels(const float *floatTexels, usize floatCount, const HzbLevelDims *dims,
                          u32 levelCount, HzbPyramid &out) {
  out.valid = false;
  if (floatTexels == nullptr || dims == nullptr || levelCount == 0) return false;

  usize total = 0;
  for (u32 level = 0; level < levelCount; ++level) {
    if (dims[level].width == 0 || dims[level].height == 0) return false;
    const usize levelTexels = static_cast<usize>(dims[level].width) * dims[level].height;
    if (levelTexels > std::numeric_limits<usize>::max() - total) return false;
    total += levelTexels;
  }
  if (total != floatCount) return false;
  for (usize index = 0; index < total; ++index) {
    if (!std::isfinite(floatTexels[index])) return false;
  }

  // Reuse capacity on the per-frame GPU readback path. The previous version
  // constructed/moved two fresh vectors every frame, violating the render
  // loop's zero-allocation contract even though the pyramid dimensions never
  // change between swapchain recreations.
  out.mips.resize(levelCount);
  u32 offset = 0;
  for (u32 level = 0; level < levelCount; ++level) {
    out.mips[level] = {dims[level].width, dims[level].height, offset};
    offset += dims[level].width * dims[level].height;
  }
  out.texels.assign(floatTexels, floatTexels + total);
  out.valid = true;
  return true;
}

bool validateHzbMaxReductionChain(const HzbPyramid &pyramid, float tolerance) {
  if (!pyramid.valid || pyramid.mips.empty() || !std::isfinite(tolerance) || tolerance < 0.0f)
    return false;
  for (usize level = 0; level < pyramid.mips.size(); ++level) {
    const HzbMipLevel &mip = pyramid.mips[level];
    const usize texelCount = static_cast<usize>(mip.width) * mip.height;
    if (mip.width == 0 || mip.height == 0 || mip.offset > pyramid.texels.size() ||
        texelCount > pyramid.texels.size() - mip.offset) return false;
    for (usize texel = 0; texel < texelCount; ++texel)
      if (!std::isfinite(pyramid.texels[mip.offset + texel])) return false;
    if (level == 0) continue;
    const HzbMipLevel &previous = pyramid.mips[level - 1];
    if (mip.width != (previous.width + 1) / 2 || mip.height != (previous.height + 1) / 2)
      return false;
    for (u32 y = 0; y < mip.height; ++y) {
      for (u32 x = 0; x < mip.width; ++x) {
        const u32 x0 = x * 2;
        const u32 y0 = y * 2;
        const u32 x1 = std::min(x0 + 1, previous.width - 1);
        const u32 y1 = std::min(y0 + 1, previous.height - 1);
        float expected = pyramid.texels[previous.offset + pixelIndex(x0, y0, previous.width)];
        expected = std::max(expected, pyramid.texels[previous.offset + pixelIndex(x1, y0, previous.width)]);
        expected = std::max(expected, pyramid.texels[previous.offset + pixelIndex(x0, y1, previous.width)]);
        expected = std::max(expected, pyramid.texels[previous.offset + pixelIndex(x1, y1, previous.width)]);
        const float actual = pyramid.texels[mip.offset + pixelIndex(x, y, mip.width)];
        if (std::abs(actual - expected) > tolerance) return false;
      }
    }
  }
  return true;
}

HzbScreenRect projectBoundsToHzbScreenRect(const PerspectiveFrustum &frustum, const float center[3],
                                           float radius,
                                           const HzbScreenTransform &screenTransform) {
  HzbScreenRect result{};
  if (!frustum.valid || center == nullptr || !std::isfinite(radius) || radius < 0.0f) return result;
  for (u32 axis = 0; axis < 3; ++axis) if (!std::isfinite(center[axis])) return result;
  const float transformValues[] = {screenTransform.xx, screenTransform.xy,
                                   screenTransform.yx, screenTransform.yy};
  for (float value : transformValues) if (!std::isfinite(value)) return result;

  const float deltaX = center[0] - frustum.cameraPosition[0];
  const float deltaY = center[1] - frustum.cameraPosition[1];
  const float deltaZ = center[2] - frustum.cameraPosition[2];
  const float cosineYaw = std::cos(frustum.yaw);
  const float sineYaw = std::sin(frustum.yaw);
  const float cosinePitch = std::cos(frustum.pitch);
  const float sinePitch = std::sin(frustum.pitch);

  // Same view-space transform as frustum_visibility.cpp's isSphereVisible --
  // exact CPU counterpart of transpose(dirtRoadCameraRotation()).
  const float yawX = cosineYaw * deltaX - sineYaw * deltaZ;
  const float yawZ = sineYaw * deltaX + cosineYaw * deltaZ;
  const float unrolledY=cosinePitch * deltaY + sinePitch * yawZ;
  const float viewX = std::cos(frustum.roll)*yawX+std::sin(frustum.roll)*unrolledY;
  const float viewY = -std::sin(frustum.roll)*yawX+std::cos(frustum.roll)*unrolledY;
  const float viewZ = -sinePitch * deltaY + cosinePitch * yawZ;

  const float expandedRadius = radius * frustum.boundsScale + frustum.boundsMargin;
  const float nearDepth = viewZ - expandedRadius;
  // A sphere that straddles or sits behind the near plane cannot be safely
  // bounded on screen (the projection denominator approaches/crosses zero).
  // Refuse to test it rather than risk an undersized rectangle -- frustum
  // culling upstream already owns the "is this even in view" decision.
  if (!std::isfinite(nearDepth) || nearDepth < frustum.nearPlane) return result;

  const float invHorizontal = 1.0f / (projectionDivisor(frustum,nearDepth) * projectionHalfWidth(frustum));
  const float invVertical = 1.0f / (projectionDivisor(frustum,nearDepth) * projectionHalfHeight(frustum));
  float ndcMinX = (viewX - expandedRadius) * invHorizontal;
  float ndcMaxX = (viewX + expandedRadius) * invHorizontal;
  float ndcMinY = (viewY - expandedRadius) * invVertical;
  float ndcMaxY = (viewY + expandedRadius) * invVertical;
  if (!std::isfinite(ndcMinX) || !std::isfinite(ndcMaxX) || !std::isfinite(ndcMinY) ||
      !std::isfinite(ndcMaxY)) return result;
  // Transform all four corners, then rebuild the conservative AABB in the
  // actual depth attachment's NDC. This handles 90/180/270-degree pre-rotation
  // and mirrors without teaching the backend-independent frustum about
  // VkSurfaceTransformFlagBitsKHR.
  float surfaceMinX = std::numeric_limits<float>::max();
  float surfaceMinY = std::numeric_limits<float>::max();
  float surfaceMaxX = -surfaceMinX;
  float surfaceMaxY = -surfaceMinY;
  for (float x : {ndcMinX, ndcMaxX}) {
    for (float y : {ndcMinY, ndcMaxY}) {
      const float transformedX = screenTransform.xx * x + screenTransform.xy * y;
      const float transformedY = screenTransform.yx * x + screenTransform.yy * y;
      surfaceMinX = std::min(surfaceMinX, transformedX);
      surfaceMaxX = std::max(surfaceMaxX, transformedX);
      surfaceMinY = std::min(surfaceMinY, transformedY);
      surfaceMaxY = std::max(surfaceMaxY, transformedY);
    }
  }
  surfaceMinX = std::clamp(surfaceMinX, -1.0f, 1.0f);
  surfaceMaxX = std::clamp(surfaceMaxX, -1.0f, 1.0f);
  surfaceMinY = std::clamp(surfaceMinY, -1.0f, 1.0f);
  surfaceMaxY = std::clamp(surfaceMaxY, -1.0f, 1.0f);
  if (surfaceMinX >= surfaceMaxX || surfaceMinY >= surfaceMaxY) return result;

  result.minU = surfaceMinX * 0.5f + 0.5f;
  result.maxU = surfaceMaxX * 0.5f + 0.5f;
  result.minV = surfaceMinY * 0.5f + 0.5f;
  result.maxV = surfaceMaxY * 0.5f + 0.5f;
  // Exact CPU counterpart of dirt_road.vert's perspective projection:
  // clipZ=(far*z-near*far)/(far-near), clipW=z. The HZB stores clipZ/clipW,
  // not view-space z. Expanded bounds already make this the nearest possible
  // point of the sphere, so the conversion preserves conservatism.
  const float normalizedDepth = cameraNormalizedDepth(frustum,nearDepth);
  if (!std::isfinite(normalizedDepth)) return HzbScreenRect{};
  result.nearDepth = std::clamp(normalizedDepth, 0.0f, 1.0f);
  result.valid = true;
  return result;
}

bool isOccludedByHzb(const HzbPyramid &pyramid, const HzbScreenRect &rect,
                     float normalizedDepthBias) {
  if (!pyramid.valid || pyramid.mips.empty() || !rect.valid ||
      !std::isfinite(normalizedDepthBias) || normalizedDepthBias < 0.0f) return false;

  // Pick the coarsest level whose texel footprint for this rect still fits
  // the scan budget, preferring the finest (most precise) level that does.
  // Any level works correctness-wise (MAX-reduction is valid at every level);
  // this only bounds how many texels get compared per test.
  usize chosenLevel = pyramid.mips.size() - 1;
  u32 minX = 0, maxX = 0, minY = 0, maxY = 0;
  for (usize level = 0; level < pyramid.mips.size(); ++level) {
    const HzbMipLevel &mip = pyramid.mips[level];
    // Index of the last texel the rect can touch on each axis: ceil(edge *
    // extent) - 1, i.e. the texel whose half-open span [i, i+1) contains the
    // edge. Rounding this way (rather than floor) stays conservative under
    // floating-point jitter -- worst case it scans one extra texel, never
    // one too few, which can only raise the tested MAX (safer).
    const u32 levelMinX = std::min(static_cast<u32>(rect.minU * mip.width), mip.width - 1);
    const u32 levelMaxX = std::min(
        static_cast<u32>(std::max(std::ceil(rect.maxU * mip.width) - 1.0f, 0.0f)), mip.width - 1);
    const u32 levelMinY = std::min(static_cast<u32>(rect.minV * mip.height), mip.height - 1);
    const u32 levelMaxY = std::min(
        static_cast<u32>(std::max(std::ceil(rect.maxV * mip.height) - 1.0f, 0.0f)), mip.height - 1);
    const u32 texelCount = (levelMaxX - levelMinX + 1) * (levelMaxY - levelMinY + 1);
    minX = levelMinX; maxX = levelMaxX; minY = levelMinY; maxY = levelMaxY;
    chosenLevel = level;
    if (texelCount <= MaximumScannedTexels) break;
  }

  const HzbMipLevel &mip = pyramid.mips[chosenLevel];
  float farthestNearestDepth = pyramid.texels[mip.offset + pixelIndex(minX, minY, mip.width)];
  for (u32 y = minY; y <= maxY; ++y) {
    for (u32 x = minX; x <= maxX; ++x) {
      farthestNearestDepth =
          std::max(farthestNearestDepth, pyramid.texels[mip.offset + pixelIndex(x, y, mip.width)]);
    }
  }
  return rect.nearDepth > farthestNearestDepth + normalizedDepthBias;
}

bool shouldRunHzb(u32 candidateDraws, u32 minimumCandidateDraws) {
  return candidateDraws > 0 && candidateDraws >= minimumCandidateDraws;
}

bool updateHzbHysteresis(HzbHysteresisState &state, bool occludedThisFrame, u32 hysteresisFrames) {
  if (!occludedThisFrame) {
    state.occludedStreak = 0;
    return true;
  }
  if (state.occludedStreak != std::numeric_limits<u32>::max()) ++state.occludedStreak;
  return state.occludedStreak < hysteresisFrames;
}

} // namespace ae::renderer
