#include "harness.h"

#include "renderer/color_grading.h"
#include "resources/environment_looks.h"
#include "resources/environment_profile.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using namespace ae;

namespace {

float srgbEncode(float linear) {
  linear = std::clamp(linear, 0.0f, 1.0f);
  return linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}
float srgbDecode(float encoded) {
  return encoded <= 0.04045f ? encoded / 12.92f : std::pow((encoded + 0.055f) / 1.055f, 2.4f);
}
// Diferença visível no monitor de 8 bits, que é o que o aceite do G2 mede.
float byteError(const float a[3], const float b[3]) {
  float worst = 0.0f;
  for (u32 c = 0; c < 3; ++c)
    worst = std::max(worst, std::abs(srgbEncode(a[c]) - srgbEncode(b[c])) * 255.0f);
  return worst;
}

std::vector<float> lutFloat(const renderer::ColorGradingLook &look) {
  std::vector<float> lut(static_cast<usize>(renderer::ColorGradingLutTexels) * 4u);
  if (!renderer::buildColorGradingLut(look, lut)) lut.clear();
  return lut;
}
// O que a GPU recebe: a LUT em meia precisão, lida de volta em float.
std::vector<float> lutFromHalf(const renderer::ColorGradingLook &look) {
  std::vector<u16> half(static_cast<usize>(renderer::ColorGradingLutTexels) * 4u);
  std::vector<float> lut;
  if (!renderer::buildColorGradingLutHalf(look, half)) return lut;
  lut.reserve(half.size());
  for (u16 value : half) lut.push_back(renderer::colorGradingHalfToFloat(value));
  return lut;
}

renderer::ColorGradingLook lookOf(resources::EnvironmentLook look) {
  renderer::SceneEnvironment environment{};
  resources::applyEnvironmentLook(environment, look);
  return renderer::colorGradingLook(environment);
}

// Mesma fórmula do caminho sem LUT (post_process_common.glsl).
void aluGrade(float contrast, float saturation, const float in[3], float out[3]) {
  const float gray = in[0] * 0.2126f + in[1] * 0.7152f + in[2] * 0.0722f;
  for (u32 c = 0; c < 3; ++c) out[c] = (gray + (in[c] - gray) * saturation - 0.5f) * contrast + 0.5f;
}

} // namespace

AE_TEST(color_grading_neutral_lut_is_identity_within_one_byte) {
  const renderer::ColorGradingLook neutral{};
  AE_EXPECT_TRUE(!renderer::colorGradingUsesLut(neutral), "neutro não liga a LUT");
  const auto lut = lutFloat(neutral);
  AE_EXPECT_TRUE(!lut.empty(), "LUT neutra montada");
  constexpr float last = static_cast<float>(renderer::ColorGradingLutSize - 1);
  for (u32 b = 0; b < renderer::ColorGradingLutSize; ++b)
    for (u32 g = 0; g < renderer::ColorGradingLutSize; ++g)
      for (u32 r = 0; r < renderer::ColorGradingLutSize; ++r) {
        const float *texel = lut.data() + ((b * 32u + g) * 32u + r) * 4u;
        const float expected[3]{(r / last) * (r / last), (g / last) * (g / last), (b / last) * (b / last)};
        for (u32 c = 0; c < 3; ++c)
          AE_EXPECT_TRUE(std::abs(texel[c] - expected[c]) <= 1.0e-6f, "célula neutra = coordenada²");
        AE_EXPECT_EQ(texel[3], 1.0f, "alfa 1");
      }
  // Todos os 256 níveis de cinza e uma grade de cores 8 bits, pela LUT em
  // meia precisão e leitura trilinear do shader: no máximo 1/255.
  const auto half = lutFromHalf(neutral);
  float worst = 0.0f;
  for (u32 level = 0; level < 256; ++level) {
    const float value = srgbDecode(level / 255.0f), in[3]{value, value, value};
    float out[3];renderer::sampleColorGradingLut(half, in, out);
    worst = std::max(worst, byteError(in, out));
  }
  for (u32 r = 0; r < 256; r += 15) for (u32 g = 0; g < 256; g += 15) for (u32 b = 0; b < 256; b += 15) {
    const float in[3]{srgbDecode(r / 255.0f), srgbDecode(g / 255.0f), srgbDecode(b / 255.0f)};
    float out[3];renderer::sampleColorGradingLut(half, in, out);
    worst = std::max(worst, byteError(in, out));
  }
  std::printf("  neutro pela LUT: erro máximo %.3f/255\n", static_cast<double>(worst));
  AE_EXPECT_TRUE(worst <= 1.0f, "LUT neutra até 1/255 (aceite: 3/255)");
}

AE_TEST(color_grading_lut_carries_contrast_and_saturation_like_the_alu_path) {
  // Com a LUT ligada o shader pula contraste e saturação; a LUT os aplica na
  // mesma ordem. Ligada por um matiz mínimo, o resto tem de bater com a ULA.
  renderer::ColorGradingLook look{};
  look.contrast = 1.12f;look.saturation = 0.7f;
  for (u32 level = 0; level <= 20; ++level) for (u32 hue = 0; hue < 6; ++hue) {
    const float v = level / 20.0f;
    const float in[3]{v, v * (hue & 1 ? 0.3f : 1.0f), v * (hue & 2 ? 0.5f : 0.1f)};
    float expected[3], actual[3];
    aluGrade(look.contrast, look.saturation, in, expected);
    renderer::gradeDisplayColor(look, in, actual);
    for (u32 c = 0; c < 3; ++c) AE_EXPECT_TRUE(std::abs(expected[c] - actual[c]) <= 1.0e-6f, "mesma ULA");
  }
}

AE_TEST(color_grading_lut_matches_the_function_for_every_look) {
  for (const auto &preset : resources::environmentLookPresets()) {
    const auto look = lookOf(preset.look);
    const auto half = lutFromHalf(look);
    AE_EXPECT_TRUE(!half.empty(), "LUT montada");
    float worst = 0.0f;
    for (u32 r = 0; r < 256; r += 17) for (u32 g = 0; g < 256; g += 17) for (u32 b = 0; b < 256; b += 17) {
      const float in[3]{srgbDecode(r / 255.0f), srgbDecode(g / 255.0f), srgbDecode(b / 255.0f)};
      float direct[3], sampled[3];
      renderer::gradeDisplayColor(look, in, direct);
      renderer::sampleColorGradingLut(half, in, sampled);
      worst = std::max(worst, byteError(direct, sampled));
    }
    std::printf("  %s: LUT x função, erro máximo %.3f/255\n", std::string(preset.key).c_str(),
                static_cast<double>(worst));
    // O pior caso é cor saturada com um canal quase zero: erro linear ~0,001
    // ampliado pela inclinação 12,92 do sRGB perto do preto. O aceite de
    // 3/255 do G2 é o do Neutro (teste acima); aqui é a fidelidade da tabela.
    AE_EXPECT_TRUE(worst <= 4.0f, "32³ reproduz a função até 4/255");
  }
}

AE_TEST(color_grading_looks_are_monotonic_on_gray_and_primaries) {
  // Mais luz na entrada nunca dá menos luz na saída: nem no cinza, nem
  // subindo um canal sozinho. Uma inversão vira faixa/solarização na tela.
  for (const auto &preset : resources::environmentLookPresets()) {
    const auto look = lookOf(preset.look);
    const auto lut = lutFromHalf(look);
    for (u32 axis = 0; axis < 4; ++axis) {
      float previous = -std::numeric_limits<float>::infinity();
      for (u32 step = 0; step <= 255; ++step) {
        const float v = srgbDecode(step / 255.0f);
        const float in[3]{axis == 3 || axis == 0 ? v : 0.2f, axis == 3 || axis == 1 ? v : 0.2f,
                          axis == 3 || axis == 2 ? v : 0.2f};
        float out[3];renderer::sampleColorGradingLut(lut, in, out);
        const float y = out[0] * 0.2126f + out[1] * 0.7152f + out[2] * 0.0722f;
        AE_EXPECT_TRUE(y >= previous - 1.0e-4f, "luminância monotônica");
        previous = y;
      }
    }
  }
}

AE_TEST(color_grading_limits_stay_finite_and_invalid_values_are_rejected) {
  renderer::ColorGradingLook extreme{};
  extreme.contrast = 2.0f;extreme.saturation = 2.0f;
  auto &g = extreme.grading;
  g.temperature = 100.0f;g.tint = -100.0f;
  g.lift = 1.0f;g.gamma = -1.0f;g.gain = 1.0f;
  g.shadows = 1.0f;g.midtones = -1.0f;g.highlights = 1.0f;g.vibrance = 1.0f;
  for (float *color : {g.liftColor, g.gammaColor, g.gainColor, g.shadowsColor, g.midtonesColor, g.highlightsColor})
    color[0] = 0.0f, color[1] = 1.0f, color[2] = 0.0f;
  g.shadowsStart = g.shadowsEnd = 0.5f; // faixa degenerada vira degrau
  g.highlightsStart = 1.0f;g.highlightsEnd = 0.0f;
  AE_EXPECT_TRUE(g.valid(), "extremos válidos");
  for (float temperature : {-100.0f, 100.0f}) {
    g.temperature = temperature;
    const auto lut = lutFloat(extreme);
    AE_EXPECT_TRUE(!lut.empty(), "extremos montam LUT finita");
    for (float value : lut) AE_EXPECT_TRUE(std::isfinite(value), "célula finita");
  }
  // Entrada fora de [0,1] (sem tonemap, bloom forte) fica na borda da LUT.
  const auto neutral = lutFloat({});
  const float above[3]{4.0f, 1.0f, -2.0f};float out[3];
  renderer::sampleColorGradingLut(neutral, above, out);
  AE_EXPECT_TRUE(std::abs(out[0] - 1.0f) < 1.0e-6f && std::abs(out[2]) < 1.0e-6f, "borda limita");

  renderer::ColorGrading invalid{};
  invalid.temperature = 101.0f;AE_EXPECT_TRUE(!invalid.valid(), "temperatura fora");
  invalid = {};invalid.lift = std::numeric_limits<float>::quiet_NaN();AE_EXPECT_TRUE(!invalid.valid(), "NaN");
  invalid = {};invalid.gainColor[1] = 1.5f;AE_EXPECT_TRUE(!invalid.valid(), "cor > 1");
  invalid = {};invalid.highlightsEnd = -0.1f;AE_EXPECT_TRUE(!invalid.valid(), "faixa < 0");
  renderer::SceneEnvironment environment{};environment.grading.vibrance = -1.5f;
  AE_EXPECT_TRUE(!environment.valid(), "ambiente recusa gradação inválida");
  std::vector<float> wrongSize(16);
  AE_EXPECT_TRUE(!renderer::buildColorGradingLut({}, wrongSize), "tamanho errado recusado");
}

AE_TEST(color_grading_half_conversion_round_trips) {
  for (float value : {0.0f, 1.0f, -1.0f, 0.5f, 1.0e-5f, 6.1e-5f, 65504.0f, 0.333333f, 2.0f}) {
    const float back = renderer::colorGradingHalfToFloat(renderer::colorGradingFloatToHalf(value));
    AE_EXPECT_TRUE(std::abs(back - value) <= std::abs(value) * 1.0e-3f + 6.0e-8f, "meia precisão");
  }
  AE_EXPECT_TRUE(std::isinf(renderer::colorGradingHalfToFloat(renderer::colorGradingFloatToHalf(1.0e6f))), "estoura em inf");
}

AE_TEST(environment_looks_apply_only_the_post_group) {
  const auto presets = resources::environmentLookPresets();
  AE_EXPECT_EQ(presets.size(), usize{7}, "sete perfis");
  const renderer::SceneEnvironment defaults{};
  AE_EXPECT_TRUE(resources::matchEnvironmentLook(defaults) == resources::EnvironmentLook::Neutral,
                 "cena padrão/antiga = Neutro");
  AE_EXPECT_TRUE(!renderer::colorGradingUsesLut(renderer::colorGradingLook(defaults)), "Neutro sem LUT");
  for (const auto &preset : presets) {
    renderer::SceneEnvironment scene{};
    scene.fog = true;scene.fogDensity = 0.03f;scene.exposureEv = 1.5f;scene.ambientOcclusion = true;
    scene.sky = renderer::SkyModel::PhysicalAtmosphere;scene.post = false;
    const auto before = scene;
    resources::applyEnvironmentLook(scene, preset.look);
    AE_EXPECT_TRUE(scene.valid(), "perfil válido");
    AE_EXPECT_TRUE(scene.post, "aplicar liga o pós");
    AE_EXPECT_TRUE(scene.fog == before.fog && scene.fogDensity == before.fogDensity &&
                   scene.exposureEv == before.exposureEv && scene.ambientOcclusion == before.ambientOcclusion &&
                   scene.sky == before.sky, "céu, neblina, exposição e AO são da cena");
    AE_EXPECT_TRUE(resources::matchEnvironmentLook(scene) == preset.look, "cartão reconhece o perfil aplicado");
    AE_EXPECT_EQ(renderer::colorGradingUsesLut(renderer::colorGradingLook(scene)), preset.look != resources::EnvironmentLook::Neutral,
                 "só o Neutro dispensa a LUT");
    scene.grading.vibrance += 0.01f;
    AE_EXPECT_TRUE(!resources::matchEnvironmentLook(scene), "ajuste à mão vira Personalizado");
    const auto profile = resources::environmentLookProfile(preset.look);
    AE_EXPECT_TRUE(profile.valid() && profile.name == preset.name, "perfil embutido válido");
    AE_EXPECT_TRUE(profile.guid == resources::environmentLookProfile(preset.look).guid, "GUID estável");
    resources::EnvironmentProfile parsed;
    AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(profile.serialize(), parsed), "perfil v9 relido");
    AE_EXPECT_TRUE(parsed.values.grading == profile.values.grading && resources::sameEnvironmentAppearance(parsed.values, profile),
                   "gradação atravessa o arquivo");
    AE_EXPECT_TRUE(resources::findEnvironmentLook(preset.key) == &preset, "chave estável");
  }
}

AE_TEST(environment_profile_v8_reads_with_neutral_grading) {
  auto profile = resources::environmentLookProfile(resources::EnvironmentLook::Neutral);
  profile.values.contrast = 1.2f;
  auto text = profile.serialize();
  // Um arquivo v8 é o atual sem os 31 números da gradação e os 5 dos corpos
  // celestes (v10) e os 17 das nuvens (v11, v12), que são os últimos.
  text.replace(text.find(" 12 "), 4, " 8 ");
  auto cut = text.size() - 1;
  for (u32 removed = 0; removed < 53; ++removed) cut = text.rfind(' ', cut - 1);
  text = text.substr(0, cut) + "\n";
  resources::EnvironmentProfile parsed;
  AE_EXPECT_TRUE(resources::EnvironmentProfile::deserialize(text, parsed), "v8 lido");
  AE_EXPECT_TRUE(parsed.values.grading.neutral() && parsed.values.contrast == 1.2f, "v8 = gradação neutra, resto igual");
}

AE_TEST(environment_volume_blend_reaches_the_exact_target_grading) {
  renderer::SceneEnvironmentVolume base{}, look{};
  base.environment.active = look.environment.active = true;
  resources::applyEnvironmentLook(look.environment, resources::EnvironmentLook::Sunset);
  look.environment.priority = 1.0f;base.stableId = 1;look.stableId = 2;
  const renderer::SceneEnvironmentVolume volumes[]{base, look};
  const float camera[3]{};
  const auto full = renderer::resolveSceneEnvironment(volumes, camera);
  AE_EXPECT_TRUE(full.grading == look.environment.grading, "peso 1 = alvo exato");
  renderer::SceneEnvironmentVolume halfLook = look;halfLook.weight = 0.5f;
  const renderer::SceneEnvironmentVolume halfVolumes[]{base, halfLook};
  const auto half = renderer::resolveSceneEnvironment(halfVolumes, camera);
  AE_EXPECT_TRUE(half.grading.temperature > 0.0f && half.grading.temperature < look.environment.grading.temperature,
                 "meio peso mistura a gradação");
  renderer::SceneEnvironmentVolume fogOnly = look;fogOnly.overrides = renderer::EnvironmentOverrideFog;
  const renderer::SceneEnvironmentVolume fogVolumes[]{base, fogOnly};
  AE_EXPECT_TRUE(renderer::resolveSceneEnvironment(fogVolumes, camera).grading.neutral(), "gradação é do grupo Pós");
}

#include "scene/environment.h"
#include <sstream>
#include <string>

// Linha v15 a partir da atual: sem os 3 interruptores das nuvens (depois da
// exposição automática, palavras 20..22) e sem os 15 números delas no fim
// (14 da v16 e a espessura da v17).
inline std::string withoutClouds(const std::string &text) {
  std::istringstream in(text);std::vector<std::string> words;std::string word;
  while (in >> word) words.push_back(word);
  std::string out;
  for (usize i = 0; i + 15 < words.size(); ++i) {
    if (i >= 20 && i <= 22) continue;
    if (!out.empty()) out += ' ';
    out += words[i];
  }
  return out;
}

AE_TEST(environment_component_v12_archive_reads_neutral_and_v13_round_trips) {
  scene::Environment authored;
  resources::applyEnvironmentLook(authored.values, resources::EnvironmentLook::Fantasy);
  authored.values.grading.shadowsEnd = 0.4f;
  std::ostringstream written;written.imbue(std::locale::classic());
  written << std::setprecision(9);
  authored.write(written);
  scene::Environment reread;
  std::istringstream in(written.str());in.imbue(std::locale::classic());
  AE_EXPECT_TRUE(reread.read(in, scene::Environment::descriptor.version), "v13 relido");
  AE_EXPECT_TRUE(reread.values.grading == authored.values.grading && reread.values.contrast == authored.values.contrast,
                 "gradação atravessa a cena");
  // v12: a mesma linha sem os 31 números da gradação e os 5 dos corpos
  // celestes (v14), que são os últimos, depois de tirar as nuvens (v16).
  auto text = withoutClouds(written.str());
  auto cut = text.size();
  for (u32 removed = 0; removed < 36; ++removed) cut = text.rfind(' ', cut - 1);
  scene::Environment old;
  std::istringstream oldIn(text.substr(0, cut));oldIn.imbue(std::locale::classic());
  AE_EXPECT_TRUE(old.read(oldIn, 12), "v12 lido");
  AE_EXPECT_TRUE(old.values.grading.neutral() && old.values.saturation == authored.values.saturation,
                 "cena v12 fica neutra e mantém o resto");
  usize grading = 0;
  for (const auto &number : scene::Environment::descriptor.numbers) grading += number.id.starts_with("grading_");
  AE_EXPECT_EQ(grading, usize{31}, "31 propriedades de gradação no Inspector/API");
}
