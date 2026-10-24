#include "renderer/volume_parameters.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <limits>

namespace ae::renderer {
namespace {

constexpr VolumeGroupInfo kGroups[VolumeGroupCount]{
  {"Céu","override_sky","Céu e luz","https://docs.unity3d.com/6000.0/Documentation/Manual/shader-skybox-procedural.html"},
  {"Sol e lua","override_celestial","Céu e luz","https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/create-a-physically-based-sky.html"},
  {"Atmosfera física","override_physical_atmosphere","Céu e luz","https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/physically-based-sky-volume-override-reference.html"},
  {"Luz ambiente","override_indirect","Céu e luz","https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/reference-indirect-lighting-controller.html"},
  {"Neblina","override_fog","Céu e luz","https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/fog-volume-override-reference.html"},
  {"Nuvens","override_clouds","Céu e luz","https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/cloud-layer-volume-override-reference.html"},
  {"Exposição","override_exposure","Pós-processamento","https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/Override-Exposure.html"},
  {"Tonemapping","override_post","Pós-processamento","https://docs.unity3d.com/6000.0/Documentation/Manual/urp/post-processing-tonemapping.html"},
  {"Bloom","override_bloom","Pós-processamento","https://docs.unity3d.com/6000.0/Documentation/Manual/urp/post-processing-bloom.html"},
  {"Vinheta","override_vignette","Pós-processamento","https://docs.unity3d.com/6000.0/Documentation/Manual/urp/post-processing-vignette.html"},
  {"Ajustes de cor","override_color_adjustments","Pós-processamento","https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Post-Processing-Color-Adjustments.html"},
  {"Cor","override_color_grading","Pós-processamento","https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Post-Processing-Lift-Gamma-Gain.html"},
  {"Curvas de cor","override_color_curves","Pós-processamento","https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Post-Processing-Color-Curves.html"},
  {"Grão de filme","override_film_grain","Pós-processamento","https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Post-Processing-Film-Grain.html"},
  {"Oclusão ambiente","override_ambient_occlusion","Pós-processamento","https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/reference-ambient-occlusion.html"},
  {"Profundidade de campo","override_depth_of_field","Pós-processamento","https://docs.unity3d.com/6000.0/Documentation/Manual/urp/depth-of-field-urp.html"},
  {"Desfoque de movimento","override_motion_blur","Pós-processamento","https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Post-Processing-Motion-Blur.html"},
  {"Aberração cromática","override_chromatic_aberration","Pós-processamento","https://docs.unity3d.com/6000.0/Documentation/Manual/urp/post-processing-chromatic-aberration.html"},
  {"Distorção de lente","override_lens_distortion","Pós-processamento","https://docs.unity3d.com/6000.0/Documentation/Manual/urp/Post-Processing-Lens-Distortion.html"},
};

#define AE_FIELD(member) +[](SceneEnvironment &e) -> void * { return static_cast<void *>(&e.member); }
using G = VolumeGroup;
using K = VolumeParameterKind;
// Ordem = endereço gravado. Acrescente só no fim.
const VolumeParameter kParameters[]{
  // Céu (Skybox Material / Skybox/Procedural / Panoramic)
  {"skybox", G::Sky, K::Guid, 1, AE_FIELD(skybox)},
  {"sky", G::Sky, K::Enum, 1, AE_FIELD(sky)},
  {"sun_disk", G::Sky, K::Enum, 1, AE_FIELD(sunDisk)},
  {"sun_size", G::Sky, K::Float, 1, AE_FIELD(sunSize)},
  {"sun_size_convergence", G::Sky, K::Float, 1, AE_FIELD(sunSizeConvergence)},
  {"atmosphere", G::Sky, K::Float, 1, AE_FIELD(atmosphere)},
  {"sky_zenith", G::Sky, K::Floats, 3, AE_FIELD(skyZenith)},
  {"sky_horizon", G::Sky, K::Floats, 3, AE_FIELD(skyHorizon)},
  {"ground", G::Sky, K::Floats, 3, AE_FIELD(ground)},
  {"sky_exposure", G::Sky, K::Float, 1, AE_FIELD(skyExposure)},
  {"environment_map", G::Sky, K::Guid, 1, AE_FIELD(environmentMap)},
  {"hdri_rotation_degrees", G::Sky, K::Float, 1, AE_FIELD(hdriRotationDegrees)},
  {"hdri_exposure_ev", G::Sky, K::Float, 1, AE_FIELD(hdriExposureEv)},
  // Sol e lua
  {"sun_glare", G::Celestial, K::Float, 1, AE_FIELD(sunGlare)},
  {"moon_phase", G::Celestial, K::Float, 1, AE_FIELD(moonPhase)},
  {"moon_size", G::Celestial, K::Float, 1, AE_FIELD(moonSizeDegrees)},
  {"moon_intensity", G::Celestial, K::Float, 1, AE_FIELD(moonIntensity)},
  {"star_intensity", G::Celestial, K::Float, 1, AE_FIELD(starIntensity)},
  // Atmosfera física (Physically Based Sky)
  {"physical_atmosphere_high_quality", G::PhysicalAtmosphere, K::Bool, 1, AE_FIELD(physicalAtmosphereHighQuality)},
  {"sun_disk_degrees", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(sunDiskDegrees)},
  {"sun_disk_intensity", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(sunDiskIntensity)},
  {"physical_sky_intensity", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(physicalSkyIntensity)},
  {"air_density", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(airDensity)},
  {"aerosol_density", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(aerosolDensity)},
  {"aerosol_anisotropy", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(aerosolAnisotropy)},
  {"planet_radius_km", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(planetRadiusKm)},
  {"observer_height_km", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(observerHeightKm)},
  {"rayleigh_scale_height_km", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(rayleighScaleHeightKm)},
  {"aerosol_scale_height_km", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(aerosolScaleHeightKm)},
  {"atmosphere_height_km", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(atmosphereHeightKm)},
  {"ground_albedo", G::PhysicalAtmosphere, K::Float, 1, AE_FIELD(groundAlbedo)},
  // Luz ambiente (Environment Lighting + Indirect Lighting Controller)
  {"ambient_source", G::Ambient, K::Enum, 1, AE_FIELD(ambientSource)},
  {"ambient_sky_color", G::Ambient, K::Floats, 3, AE_FIELD(ambientSkyColor)},
  {"ambient_equator_color", G::Ambient, K::Floats, 3, AE_FIELD(ambientEquatorColor)},
  {"ambient_ground_color", G::Ambient, K::Floats, 3, AE_FIELD(ambientGroundColor)},
  {"ambient_color", G::Ambient, K::Floats, 3, AE_FIELD(ambientColor)},
  {"indirect_diffuse", G::Ambient, K::Float, 1, AE_FIELD(indirectDiffuse)},
  {"indirect_specular", G::Ambient, K::Float, 1, AE_FIELD(indirectSpecular)},
  // Neblina
  {"fog", G::Fog, K::Bool, 1, AE_FIELD(fog)},
  {"fog_mode", G::Fog, K::Enum, 1, AE_FIELD(fogMode)},
  {"fog_color_mode", G::Fog, K::Enum, 1, AE_FIELD(fogColorMode)},
  {"fog_color", G::Fog, K::Floats, 3, AE_FIELD(fogColor)},
  {"fog_light_energy", G::Fog, K::Float, 1, AE_FIELD(fogLightEnergy)},
  {"fog_density", G::Fog, K::Float, 1, AE_FIELD(fogDensity)},
  {"fog_start", G::Fog, K::Float, 1, AE_FIELD(fogStart)},
  {"fog_end", G::Fog, K::Float, 1, AE_FIELD(fogEnd)},
  {"fog_base_height", G::Fog, K::Float, 1, AE_FIELD(fogBaseHeight)},
  {"fog_height_falloff", G::Fog, K::Float, 1, AE_FIELD(fogHeightFalloff)},
  // Nuvens (Cloud Layer)
  {"clouds", G::Clouds, K::Bool, 1, AE_FIELD(clouds.enabled)},
  {"cloud_shadows", G::Clouds, K::Bool, 1, AE_FIELD(clouds.castShadows)},
  {"cloud_opacity", G::Clouds, K::Float, 1, AE_FIELD(clouds.opacity)},
  {"cloud_coverage", G::Clouds, K::Float, 1, AE_FIELD(clouds.coverage)},
  {"cloud_cirrus", G::Clouds, K::Float, 1, AE_FIELD(clouds.cirrus)},
  {"cloud_altitude", G::Clouds, K::Float, 1, AE_FIELD(clouds.altitudeMeters)},
  {"cloud_size", G::Clouds, K::Float, 1, AE_FIELD(clouds.sizeMeters)},
  {"cloud_tint", G::Clouds, K::Floats, 3, AE_FIELD(clouds.tint)},
  {"cloud_exposure", G::Clouds, K::Float, 1, AE_FIELD(clouds.exposureEv)},
  {"cloud_wind_orientation", G::Clouds, K::Float, 1, AE_FIELD(clouds.windOrientationDegrees)},
  {"cloud_wind_speed", G::Clouds, K::Float, 1, AE_FIELD(clouds.windSpeedKmh)},
  {"cloud_density", G::Clouds, K::Float, 1, AE_FIELD(clouds.density)},
  {"cloud_ambient_dimmer", G::Clouds, K::Float, 1, AE_FIELD(clouds.ambientDimmer)},
  {"cloud_shadow_multiplier", G::Clouds, K::Float, 1, AE_FIELD(clouds.shadowMultiplier)},
  // Exposição (Exposure do HDRP)
  {"exposure_mode", G::Exposure, K::Enum, 1, AE_FIELD(exposureMode)},
  {"exposure_metering", G::Exposure, K::Enum, 1, AE_FIELD(exposureMetering)},
  {"exposure_adaptation", G::Exposure, K::Enum, 1, AE_FIELD(exposureAdaptation)},
  {"exposure_fixed", G::Exposure, K::Float, 1, AE_FIELD(fixedExposure)},
  {"exposure_compensation", G::Exposure, K::Float, 1, AE_FIELD(exposureCompensation)},
  {"exposure_limit_min", G::Exposure, K::Float, 1, AE_FIELD(exposureLimitMin)},
  {"exposure_limit_max", G::Exposure, K::Float, 1, AE_FIELD(exposureLimitMax)},
  {"exposure_histogram_low", G::Exposure, K::Float, 1, AE_FIELD(exposureHistogramLow)},
  {"exposure_histogram_high", G::Exposure, K::Float, 1, AE_FIELD(exposureHistogramHigh)},
  {"exposure_target_mid_grey", G::Exposure, K::Float, 1, AE_FIELD(exposureTargetMidGrey), "exposure_target_mid_grey_choice"},
  {"exposure_speed_dark_to_light", G::Exposure, K::Float, 1, AE_FIELD(exposureSpeedDarkToLight)},
  {"exposure_speed_light_to_dark", G::Exposure, K::Float, 1, AE_FIELD(exposureSpeedLightToDark)},
  // Tonemapping
  {"post", G::Tonemapping, K::Bool, 1, AE_FIELD(post)},
  {"tone_mapper", G::Tonemapping, K::Enum, 1, AE_FIELD(toneMapper)},
  // Bloom
  {"bloom_threshold", G::Bloom, K::Float, 1, AE_FIELD(bloomThreshold)},
  {"bloom_intensity", G::Bloom, K::Float, 1, AE_FIELD(bloomIntensity)},
  {"bloom_scatter", G::Bloom, K::Float, 1, AE_FIELD(bloomScatter)},
  {"bloom_tint", G::Bloom, K::Floats, 3, AE_FIELD(bloomTint)},
  {"bloom_clamp", G::Bloom, K::Float, 1, AE_FIELD(bloomClamp)},
  {"bloom_high_quality", G::Bloom, K::Bool, 1, AE_FIELD(bloomHighQualityFiltering)},
  {"bloom_dirt", G::Bloom, K::Guid, 1, AE_FIELD(bloomDirt)},
  {"bloom_dirt_intensity", G::Bloom, K::Float, 1, AE_FIELD(bloomDirtIntensity)},
  // Vinheta
  {"vignette_color", G::Vignette, K::Floats, 3, AE_FIELD(vignetteColor)},
  {"vignette_center", G::Vignette, K::Floats, 2, AE_FIELD(vignetteCenter)},
  {"vignette_intensity", G::Vignette, K::Float, 1, AE_FIELD(vignetteIntensity)},
  {"vignette_smoothness", G::Vignette, K::Float, 1, AE_FIELD(vignetteSmoothness)},
  {"vignette_rounded", G::Vignette, K::Bool, 1, AE_FIELD(vignetteRounded)},
  // Ajustes de cor (Color Adjustments)
  {"post_exposure", G::ColorAdjustments, K::Float, 1, AE_FIELD(postExposure)},
  {"contrast", G::ColorAdjustments, K::Float, 1, AE_FIELD(contrast)},
  {"color_filter", G::ColorAdjustments, K::Floats, 3, AE_FIELD(colorFilter)},
  {"hue_shift", G::ColorAdjustments, K::Float, 1, AE_FIELD(hueShift)},
  {"saturation", G::ColorAdjustments, K::Float, 1, AE_FIELD(saturation)},
  // Cor: White Balance, Lift Gamma Gain e Shadows Midtones Highlights. Cada
  // trackball é um parâmetro (cor + deslocamento), como o Vector4 da Unity.
  {"grading_temperature", G::ColorGrading, K::Float, 1, AE_FIELD(grading.temperature)},
  {"grading_tint", G::ColorGrading, K::Float, 1, AE_FIELD(grading.tint)},
  {"grading_lift", G::ColorGrading, K::Floats, 4, AE_FIELD(grading.liftColor), "grading_lift_color"},
  {"grading_gamma", G::ColorGrading, K::Floats, 4, AE_FIELD(grading.gammaColor), "grading_gamma_color"},
  {"grading_gain", G::ColorGrading, K::Floats, 4, AE_FIELD(grading.gainColor), "grading_gain_color"},
  {"grading_shadows", G::ColorGrading, K::Floats, 4, AE_FIELD(grading.shadowsColor), "grading_shadows_color"},
  {"grading_midtones", G::ColorGrading, K::Floats, 4, AE_FIELD(grading.midtonesColor), "grading_midtones_color"},
  {"grading_highlights", G::ColorGrading, K::Floats, 4, AE_FIELD(grading.highlightsColor), "grading_highlights_color"},
  {"grading_shadows_start", G::ColorGrading, K::Float, 1, AE_FIELD(grading.shadowsStart)},
  {"grading_shadows_end", G::ColorGrading, K::Float, 1, AE_FIELD(grading.shadowsEnd)},
  {"grading_highlights_start", G::ColorGrading, K::Float, 1, AE_FIELD(grading.highlightsStart)},
  {"grading_highlights_end", G::ColorGrading, K::Float, 1, AE_FIELD(grading.highlightsEnd)},
  {"grading_vibrance", G::ColorGrading, K::Float, 1, AE_FIELD(grading.vibrance)},
  // Curvas de cor
  {"curve_master", G::ColorCurves, K::Curve, 0, AE_FIELD(curves[0])},
  {"curve_red", G::ColorCurves, K::Curve, 1, AE_FIELD(curves[1])},
  {"curve_green", G::ColorCurves, K::Curve, 2, AE_FIELD(curves[2])},
  {"curve_blue", G::ColorCurves, K::Curve, 3, AE_FIELD(curves[3])},
  {"curve_hue_vs_hue", G::ColorCurves, K::Curve, 4, AE_FIELD(curves[4])},
  {"curve_hue_vs_sat", G::ColorCurves, K::Curve, 5, AE_FIELD(curves[5])},
  {"curve_sat_vs_sat", G::ColorCurves, K::Curve, 6, AE_FIELD(curves[6])},
  {"curve_lum_vs_sat", G::ColorCurves, K::Curve, 7, AE_FIELD(curves[7])},
  // Grão de filme
  {"film_grain_intensity", G::FilmGrain, K::Float, 1, AE_FIELD(filmGrainIntensity)},
  // Oclusão ambiente
  {"ambient_occlusion_intensity", G::AmbientOcclusion, K::Float, 1, AE_FIELD(ambientOcclusionIntensity)},
  {"ambient_occlusion_radius", G::AmbientOcclusion, K::Float, 1, AE_FIELD(ambientOcclusionRadius)},
  {"ambient_occlusion_power", G::AmbientOcclusion, K::Float, 1, AE_FIELD(ambientOcclusionPower)},
  {"ambient_occlusion_bias", G::AmbientOcclusion, K::Float, 1, AE_FIELD(ambientOcclusionBias)},
  // Profundidade de campo
  {"dof_mode", G::DepthOfField, K::Enum, 1, AE_FIELD(depthOfField)},
  {"dof_quality", G::DepthOfField, K::Enum, 1, AE_FIELD(depthOfFieldQuality)},
  {"dof_gaussian_start", G::DepthOfField, K::Float, 1, AE_FIELD(gaussianStart)},
  {"dof_gaussian_end", G::DepthOfField, K::Float, 1, AE_FIELD(gaussianEnd)},
  {"dof_gaussian_max_radius", G::DepthOfField, K::Float, 1, AE_FIELD(gaussianMaxRadius)},
  {"dof_gaussian_high_quality", G::DepthOfField, K::Bool, 1, AE_FIELD(gaussianHighQuality)},
  {"dof_focus_distance", G::DepthOfField, K::Float, 1, AE_FIELD(focusDistance)},
  {"dof_focal_length", G::DepthOfField, K::Float, 1, AE_FIELD(focalLength)},
  {"dof_aperture", G::DepthOfField, K::Float, 1, AE_FIELD(aperture)},
  {"dof_blade_count", G::DepthOfField, K::Float, 1, AE_FIELD(bladeCount)},
  {"dof_blade_curvature", G::DepthOfField, K::Float, 1, AE_FIELD(bladeCurvature)},
  {"dof_blade_rotation", G::DepthOfField, K::Float, 1, AE_FIELD(bladeRotation)},
  // Desfoque de movimento
  {"motion_blur_quality", G::MotionBlur, K::Enum, 1, AE_FIELD(motionBlurQuality)},
  {"motion_blur_intensity", G::MotionBlur, K::Float, 1, AE_FIELD(motionBlurIntensity)},
  {"motion_blur_clamp", G::MotionBlur, K::Float, 1, AE_FIELD(motionBlurClamp)},
  // Aberração cromática
  {"chromatic_aberration", G::ChromaticAberration, K::Float, 1, AE_FIELD(chromaticAberration)},
  // Distorção de lente
  {"lens_distortion", G::LensDistortion, K::Float, 1, AE_FIELD(lensDistortion)},
  {"lens_distortion_x_multiplier", G::LensDistortion, K::Float, 1, AE_FIELD(lensDistortionXMultiplier)},
  {"lens_distortion_y_multiplier", G::LensDistortion, K::Float, 1, AE_FIELD(lensDistortionYMultiplier)},
  {"lens_distortion_center", G::LensDistortion, K::Floats, 2, AE_FIELD(lensDistortionCenter)},
  {"lens_distortion_scale", G::LensDistortion, K::Float, 1, AE_FIELD(lensDistortionScale)},
  // G4c/G4d: espessura da camada e Volumetric Clouds (no fim: a tabela só cresce).
  {"cloud_thickness", G::Clouds, K::Float, 1, AE_FIELD(clouds.thicknessMeters)},
  {"cloud_type", G::Clouds, K::Enum, 1, AE_FIELD(clouds.type)},
  {"cloud_shape_factor", G::Clouds, K::Float, 1, AE_FIELD(clouds.shapeFactor)},
  {"cloud_erosion", G::Clouds, K::Float, 1, AE_FIELD(clouds.erosion)},
  {"cloud_steps", G::Clouds, K::Float, 1, AE_FIELD(clouds.steps)},
};
#undef AE_FIELD
static_assert(std::size(kParameters) <= VolumeParameterCapacity);

template <class T> T &at(const VolumeParameter &p, SceneEnvironment &e) { return *static_cast<T *>(p.field(e)); }
template <class T> const T &at(const VolumeParameter &p, const SceneEnvironment &e) {
  return *static_cast<const T *>(p.field(const_cast<SceneEnvironment &>(e)));
}

float mix(float from, float to, float amount) { return amount >= 1.0f ? to : from + (to - from) * amount; }

void mixCurve(u32 channel, ColorCurve &out, const ColorCurve &target, float amount) {
  if (amount >= 1.0f) {out = target;return;}
  if (out == target) return;
  // TextureCurve.Interp: média amostra a amostra das duas curvas, refeita
  // como curva suave de 16 chaves (o resultado só existe na mistura).
  ColorCurve blended;blended.count = ColorCurve::MaximumKeys;
  const bool loop = colorCurveLoops(channel);
  for (u32 i = 0; i < ColorCurve::MaximumKeys; ++i) {
    const float t = loop ? static_cast<float>(i) / ColorCurve::MaximumKeys
                         : static_cast<float>(i) / (ColorCurve::MaximumKeys - 1);
    const float value = mix(evaluateColorCurve(channel, out, t), evaluateColorCurve(channel, target, t), amount);
    blended.keys[i] = {t, std::clamp(value, -4.0f, 4.0f), 0.0f, 0.0f, 4, 4, false};
  }
  updateColorCurveTangents(blended);
  out = blended;
}

} // namespace

std::span<const VolumeGroupInfo> volumeGroups() noexcept { return kGroups; }
std::span<const VolumeParameter> volumeParameters() noexcept { return kParameters; }

VolumeGroup volumeGroupForPresentation(std::string_view presentationGroup) noexcept {
  for (u32 i = 0; i < VolumeGroupCount; ++i) if (kGroups[i].name == presentationGroup) return static_cast<VolumeGroup>(i);
  return VolumeGroup::Count;
}

u32 volumeParameterIndex(std::string_view id) noexcept {
  // Canal de trio ("fog_color.r", "vignette_center.x") endereça o trio.
  if (const auto dot = id.rfind('.'); dot != std::string_view::npos && dot + 2 == id.size()) id = id.substr(0, dot);
  for (u32 i = 0; i < std::size(kParameters); ++i)
    if (kParameters[i].id == id || (!kParameters[i].alias.empty() && kParameters[i].alias == id)) return i;
  return ~0u;
}

bool VolumeOverrides::contributes(u32 parameter) const noexcept {
  if (parameter >= std::size(kParameters) || !overridden(parameter)) return false;
  const auto group = kParameters[parameter].group;
  return hasGroup(group) && groupActive(group);
}
void VolumeOverrides::setGroupParameters(VolumeGroup group, bool value) noexcept {
  for (u32 i = 0; i < std::size(kParameters); ++i) if (kParameters[i].group == group) setOverridden(i, value);
}
void VolumeOverrides::addGroup(VolumeGroup group) noexcept {
  added |= 1u << static_cast<u32>(group);active |= 1u << static_cast<u32>(group);
}
void VolumeOverrides::removeGroup(VolumeGroup group) noexcept {
  added &= ~(1u << static_cast<u32>(group));active |= 1u << static_cast<u32>(group);
  setGroupParameters(group, false);
}
u32 VolumeOverrides::overriddenInGroup(VolumeGroup group) const noexcept {
  u32 count = 0;
  for (u32 i = 0; i < std::size(kParameters); ++i) count += kParameters[i].group == group && overridden(i);
  return count;
}
u32 VolumeOverrides::parametersInGroup(VolumeGroup group) const noexcept {
  u32 count = 0;
  for (const auto &p : kParameters) count += p.group == group;
  return count;
}
VolumeOverrides VolumeOverrides::everything() noexcept {
  VolumeOverrides result;result.added = result.active = VolumeGroupsAll;
  for (u32 i = 0; i < std::size(kParameters); ++i) result.setOverridden(i, true);
  return result;
}
bool VolumeOverrides::valid() const noexcept {
  if ((added | active) & ~VolumeGroupsAll) return false;
  for (u32 i = static_cast<u32>(std::size(kParameters)); i < VolumeParameterCapacity; ++i) if (overridden(i)) return false;
  return true;
}

VolumeOverrides volumeOverridesFromLegacyGroups(u32 legacy) noexcept {
  VolumeOverrides result;result.active = VolumeGroupsAll;
  const auto take = [&](VolumeGroup group) {result.addGroup(group);result.setGroupParameters(group, true);};
  if (legacy & EnvironmentOverrideSky) {
    take(G::Sky);take(G::Celestial);take(G::PhysicalAtmosphere);
    // O Environment Lighting (fonte e cores) acompanhava o céu.
    result.addGroup(G::Ambient);
    for (std::string_view id : {"ambient_source", "ambient_sky_color", "ambient_equator_color", "ambient_ground_color",
                                "ambient_color"}) result.setOverridden(volumeParameterIndex(id), true);
  }
  if (legacy & EnvironmentOverrideIndirect) {
    result.addGroup(G::Ambient);
    result.setOverridden(volumeParameterIndex("indirect_diffuse"), true);
    result.setOverridden(volumeParameterIndex("indirect_specular"), true);
  }
  if (legacy & EnvironmentOverrideFog) take(G::Fog);
  if (legacy & EnvironmentOverrideClouds) take(G::Clouds);
  if (legacy & EnvironmentOverrideExposure) take(G::Exposure);
  if (legacy & EnvironmentOverridePost) take(G::Tonemapping);
  if (legacy & EnvironmentOverrideBloom) take(G::Bloom);
  if (legacy & EnvironmentOverrideVignette) take(G::Vignette);
  if (legacy & EnvironmentOverrideColorAdjustments) take(G::ColorAdjustments);
  if (legacy & EnvironmentOverrideColorGrading) take(G::ColorGrading);
  if (legacy & EnvironmentOverrideFilmGrain) take(G::FilmGrain);
  if (legacy & EnvironmentOverrideAmbientOcclusion) take(G::AmbientOcclusion);
  return result;
}

void writeVolumeOverrides(std::ostream &out, const VolumeOverrides &overrides) {
  const auto flags = out.flags();
  out << ' ' << overrides.parameters.size() << std::hex;
  for (u32 word : overrides.parameters) out << ' ' << word;
  out << ' ' << overrides.added << ' ' << overrides.active;
  out.flags(flags);
}
bool readVolumeOverrides(std::istream &in, VolumeOverrides &result) {
  u32 count = 0;
  if (!(in >> count) || count == 0 || count > 64) return false;
  VolumeOverrides candidate;
  in >> std::hex;
  for (u32 i = 0; i < count; ++i) {
    u32 word = 0;
    if (!(in >> word)) {in >> std::dec;return false;}
    // Arquivo de uma versão futura com mais parâmetros: os bits que esta não
    // conhece ficam de fora (o valor deles não existe aqui).
    if (i < candidate.parameters.size()) candidate.parameters[i] = word;
  }
  const bool read = static_cast<bool>(in >> candidate.added >> candidate.active);
  in >> std::dec;
  if (!read) return false;
  for (u32 i = static_cast<u32>(std::size(kParameters)); i < VolumeParameterCapacity; ++i) candidate.setOverridden(i, false);
  candidate.added &= VolumeGroupsAll;candidate.active &= VolumeGroupsAll;
  result = candidate;
  return true;
}

void writeColorCurves(std::ostream &out, const SceneEnvironment &values) {
  for (const auto &curve : values.curves) {
    out << ' ' << curve.count;
    for (u32 i = 0; i < curve.count; ++i) {
      const auto &k = curve.keys[i];
      out << ' ' << k.time << ' ' << k.value << ' ' << k.in << ' ' << k.out << ' ' << k.left << ' ' << k.right << ' '
          << (k.broken ? 1 : 0);
    }
  }
}
bool readColorCurves(std::istream &in, SceneEnvironment &values) {
  for (auto &curve : values.curves) {
    ColorCurve candidate;
    if (!(in >> candidate.count) || candidate.count > ColorCurve::MaximumKeys) return false;
    for (u32 i = 0; i < candidate.count; ++i) {
      auto &k = candidate.keys[i];u32 broken = 0;
      if (!(in >> k.time >> k.value >> k.in >> k.out >> k.left >> k.right >> broken) || broken > 1) return false;
      k.broken = broken != 0;
    }
    if (!candidate.valid()) return false;
    curve = candidate;
  }
  return true;
}

void blendVolumeParameter(const VolumeParameter &p, SceneEnvironment &result, const SceneEnvironment &target,
                          float amount) noexcept {
  if (amount <= 0.0f) return;
  switch (p.kind) {
  case K::Float: at<float>(p, result) = mix(at<float>(p, result), at<float>(p, target), amount);break;
  case K::Floats: {
    float *out = &at<float>(p, result);const float *to = &at<float>(p, target);
    for (u32 i = 0; i < p.count; ++i) out[i] = mix(out[i], to[i], amount);
    break;
  }
  case K::Bool: at<bool>(p, result) = at<bool>(p, target);break;
  case K::Enum: at<u32>(p, result) = at<u32>(p, target);break;
  case K::Guid: at<resources::AssetGuid>(p, result) = at<resources::AssetGuid>(p, target);break;
  case K::Curve: mixCurve(p.count, at<ColorCurve>(p, result), at<ColorCurve>(p, target), amount);break;
  }
}

void copyVolumeGroup(VolumeGroup group, const SceneEnvironment &source, SceneEnvironment &target) noexcept {
  for (const auto &p : kParameters) if (p.group == group || group == VolumeGroup::Count) blendVolumeParameter(p, target, source, 1.0f);
}

bool sameVolumeParameter(const VolumeParameter &p, const SceneEnvironment &a, const SceneEnvironment &b) noexcept {
  switch (p.kind) {
  case K::Float: return at<float>(p, a) == at<float>(p, b);
  case K::Floats: return std::equal(&at<float>(p, a), &at<float>(p, a) + p.count, &at<float>(p, b));
  case K::Bool: return at<bool>(p, a) == at<bool>(p, b);
  case K::Enum: return at<u32>(p, a) == at<u32>(p, b);
  case K::Guid: return at<resources::AssetGuid>(p, a) == at<resources::AssetGuid>(p, b);
  case K::Curve: return at<ColorCurve>(p, a) == at<ColorCurve>(p, b);
  }
  return false;
}

} // namespace ae::renderer
