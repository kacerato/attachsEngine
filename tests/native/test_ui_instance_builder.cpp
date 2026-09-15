#include "harness.h"
#include "ui/ui_instance_builder.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace ae;
using namespace ae::ui;

namespace {

const UiRect kScreen{0, 0, 1600, 900};

std::vector<u8> &fontBytes() {
  static std::vector<u8> bytes = [] {
    std::vector<u8> data;
    const std::string path =
        std::string(AETHER_REPOSITORY_ROOT) + "/assets/astra-visual/ui/astra-ui-font.aeuf";
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return data;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size > 0) {
      data.resize(static_cast<usize>(size));
      if (std::fread(data.data(), 1, data.size(), file) != data.size()) data.clear();
    }
    std::fclose(file);
    return data;
  }();
  return bytes;
}

// A fonte real, e não uma sintética: o construtor de instâncias existe para
// transformar texto em quads, e testá-lo com métricas inventadas provaria só
// que ele multiplica números.
const UiFont &shippedFont() {
  static UiFont font = [] {
    UiFont result;
    result.load(fontBytes());
    return result;
  }();
  return font;
}

std::vector<u8> &iconBytes() {
  static std::vector<u8> bytes = [] {
    std::vector<u8> data;
    const std::string path =
        std::string(AETHER_REPOSITORY_ROOT) + "/assets/astra-visual/ui/astra-ui-icons.aeui";
    std::FILE *file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) return data;
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size > 0) {
      data.resize(static_cast<usize>(size));
      if (std::fread(data.data(), 1, data.size(), file) != data.size()) data.clear();
    }
    std::fclose(file);
    return data;
  }();
  return bytes;
}

const UiIconAtlas &shippedIcons() {
  static UiIconAtlas atlas = [] {
    UiIconAtlas result;
    result.load(iconBytes());
    return result;
  }();
  return atlas;
}

UiDrawList beginList() {
  UiDrawList list;
  list.begin(kScreen, shippedFont().metrics(UiFontWeight::Regular));
  return list;
}

u32 kindOf(const UiInstance &instance) {
  return static_cast<u32>(instance.params[2] + 0.5f);
}

} // namespace

AE_TEST(builder_turns_a_rect_command_into_one_instance) {
  UiDrawList list = beginList();
  list.addRect({10, 20, 300, 40}, 0xFF141414, 16.0f);
  std::vector<UiInstance> instances;
  const UiInstanceBuildResult result =
      buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
  AE_EXPECT_EQ(result.emitted, 1u, "");
  AE_EXPECT_EQ(result.dropped, 0u, "");
  AE_EXPECT_EQ(instances.size(), usize{1}, "");
  AE_EXPECT_TRUE(kindOf(instances[0]) == static_cast<u32>(UiInstanceKind::Rect), "");
  AE_EXPECT_TRUE(instances[0].bounds[0] == 10.0f && instances[0].bounds[2] == 300.0f, "");
  AE_EXPECT_TRUE(instances[0].params[0] == 16.0f, "o raio viaja na instancia");
  AE_EXPECT_EQ(instances[0].colors[0], 0xFF141414u, "");
}

AE_TEST(r4_builder_turns_a_preview_image_into_a_preview_instance_with_its_texels) {
  UiDrawList list = beginList();
  AE_EXPECT_TRUE(list.addPreviewImage({40, 50, 96, 48}, {2, 514, 200, 100}, 0xFFFFFFFF, 4.0f), "prévia aceita");
  AE_EXPECT_TRUE(!list.addPreviewImage({40, 50, 96, 48}, {}), "recorte vazio recusado");
  std::vector<UiInstance> instances;
  const UiInstanceBuildResult result = buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
  AE_EXPECT_EQ(result.emitted, 1u, "");
  AE_EXPECT_TRUE(kindOf(instances[0]) == static_cast<u32>(UiInstanceKind::Preview), "tipo prévia, não ícone");
  AE_EXPECT_TRUE(instances[0].atlas[1] == 514.0f && instances[0].atlas[2] == 200.0f, "os texels viajam do comando");
  AE_EXPECT_TRUE(instances[0].params[0] == 4.0f && instances[0].bounds[2] == 96.0f, "raio e caixa preservados");
}

AE_TEST(builder_expands_text_into_one_instance_per_drawn_glyph) {
  UiDrawList list = beginList();
  UiTypeStyle style = defaultTheme().type.body;
  // "AB C" tem quatro caracteres e tres desenhaveis: o espaco avanca e nao pinta.
  list.addText({0, 0, 400, 30}, "AB C", 0xFFFFFFFF, style);
  std::vector<UiInstance> instances;
  const UiInstanceBuildResult result =
      buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
  AE_EXPECT_EQ(result.emitted, 3u, "o espaco nao vira quad");
  for (const UiInstance &instance : instances) {
    AE_EXPECT_TRUE(kindOf(instance) == static_cast<u32>(UiInstanceKind::Glyph), "");
    AE_EXPECT_TRUE(instance.atlas[2] > 0.0f && instance.atlas[3] > 0.0f,
                   "todo glifo desenhado carrega o retangulo do atlas");
    AE_EXPECT_TRUE(instance.params[3] > 0.0f,
                   "e o alcance do campo, sem o qual o fragmento nao sabe suavizar");
  }
}

AE_TEST(builder_advances_glyphs_left_to_right) {
  UiDrawList list = beginList();
  list.addText({0, 0, 400, 30}, "III", 0xFFFFFFFF, defaultTheme().type.body);
  std::vector<UiInstance> instances;
  buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
  AE_EXPECT_EQ(instances.size(), usize{3}, "");
  AE_EXPECT_TRUE(instances[1].bounds[0] > instances[0].bounds[0], "");
  AE_EXPECT_TRUE(instances[2].bounds[0] > instances[1].bounds[0], "");
}

AE_TEST(builder_honours_horizontal_text_alignment) {
  // O valor numerico do Inspector e alinhado a direita e o rotulo a esquerda,
  // na mesma linha. Errar isto desalinha a coluna X/Y/Z inteira.
  const UiTypeStyle style = defaultTheme().type.body;
  const auto firstGlyphX = [&](UiAlign align) {
    UiDrawList list = beginList();
    list.addText({100, 0, 400, 30}, "1.250", 0xFFFFFFFF, style, align);
    std::vector<UiInstance> instances;
    buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
    return instances.empty() ? 0.0f : instances[0].bounds[0];
  };
  const float start = firstGlyphX(UiAlign::Start);
  const float centre = firstGlyphX(UiAlign::Center);
  const float end = firstGlyphX(UiAlign::End);
  AE_EXPECT_TRUE(start < centre && centre < end, "");
  AE_EXPECT_TRUE(std::fabs(start - 100.0f) < 2.0f, "alinhado ao inicio comeca na caixa");
}

AE_TEST(builder_centres_two_sizes_on_the_same_optical_middle) {
  // Dois corpos diferentes na mesma linha do Inspector -- rotulo de 11 e valor
  // de 22 -- nao podem ter a mesma LINHA DE BASE se cada um for centrado: o
  // maior desce mais, geometricamente. O que eles compartilham e o centro da
  // caixa alta, que e o que o olho compara.
  UiTypeStyle small = defaultTheme().type.body;
  small.size = 11.0f;
  UiTypeStyle large = defaultTheme().type.body;
  large.size = 22.0f;

  const UiFontMetrics metrics = shippedFont().metrics(UiFontWeight::Regular);
  const auto capCentre = [&](const UiTypeStyle &style) {
    UiDrawList list = beginList();
    list.addText({0, 100, 200, 60}, "H", 0xFFFFFFFF, style, UiAlign::Start, UiAlign::Center);
    std::vector<UiInstance> instances;
    buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
    if (instances.empty()) return 0.0f;
    // A linha de base fica no fundo do glifo menos a margem do campo, e o centro
    // da caixa alta, meia altura acima dela.
    const float margin = shippedFont().spreadPixels() /
                         static_cast<float>(shippedFont().emPixels()) * style.size;
    const float baseline = instances[0].bounds[1] + instances[0].bounds[3] - margin;
    return baseline - metrics.capHeight * style.size * 0.5f;
  };

  AE_EXPECT_TRUE(std::fabs(capCentre(small) - 130.0f) < 1.0f,
                 "o miolo optico cai no meio da linha");
  AE_EXPECT_TRUE(std::fabs(capCentre(small) - capCentre(large)) < 1.0f,
                 "e os dois corpos compartilham esse miolo");
}

AE_TEST(builder_does_not_leak_box_decoration_into_glyphs) {
  // Um rotulo dentro de um botao arredondado com degrade e contorno tem de sair
  // com a propria cor e sem cantos.
  UiDrawList list = beginList();
  UiDrawCommand probe{};
  list.addText({0, 0, 200, 40}, "A", 0xFFCAFB04, defaultTheme().type.body);
  std::vector<UiInstance> instances;
  buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
  AE_EXPECT_EQ(instances.size(), usize{1}, "");
  AE_EXPECT_TRUE(instances[0].params[0] == 0.0f, "glifo nao herda raio");
  AE_EXPECT_TRUE(instances[0].params[1] == 0.0f, "glifo nao herda contorno");
  AE_EXPECT_EQ(instances[0].colors[0], instances[0].colors[1],
               "glifo nao herda degrade: as duas paradas sao a cor do texto");
  (void)probe;
}

AE_TEST(builder_resolves_an_icon_to_its_atlas_rectangle) {
  UiDrawList list = beginList();
  list.addImage({0, 0, 32, 32}, static_cast<UiImageId>(UiIcon::EditorMove));
  std::vector<UiInstance> instances;
  buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
  AE_EXPECT_EQ(instances.size(), usize{1}, "");
  AE_EXPECT_TRUE(kindOf(instances[0]) == static_cast<u32>(UiInstanceKind::Icon), "");
  const UiRect expected = shippedIcons().rectOf(UiIcon::EditorMove);
  AE_EXPECT_TRUE(instances[0].atlas[0] == expected.x && instances[0].atlas[1] == expected.y, "");
  AE_EXPECT_TRUE(instances[0].atlas[2] == expected.width, "");
}

AE_TEST(builder_skips_an_icon_the_atlas_does_not_have) {
  UiDrawList list = beginList();
  list.addImage({0, 0, 32, 32}, 999999u);
  std::vector<UiInstance> instances;
  const UiInstanceBuildResult result =
      buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
  AE_EXPECT_EQ(result.emitted, 0u, "");
  AE_EXPECT_TRUE(instances.empty(), "melhor nenhum icone do que o icone errado");
}

AE_TEST(builder_draws_panels_even_without_a_font) {
  // Uma interface sem rotulo ainda mostra a forma dos paineis, o que e muito
  // melhor do que uma tela preta enquanto o atlas nao chega.
  UiFont empty;
  UiDrawList list;
  list.begin(kScreen, fallbackFontMetrics());
  list.addRect({0, 0, 100, 100}, 0xFF0F0F0F, 16.0f);
  list.addText({0, 0, 100, 100}, "Hierarchy", 0xFFFFFFFF, defaultTheme().type.label);
  std::vector<UiInstance> instances;
  const UiInstanceBuildResult result = buildUiInstances(list, empty, shippedIcons(), 64, instances);
  AE_EXPECT_EQ(result.emitted, 1u, "o retangulo saiu");
  AE_EXPECT_EQ(result.missingGlyphRuns, 1u, "e a ausencia do texto foi contada, nao silenciada");
}

AE_TEST(builder_counts_what_did_not_fit_in_the_budget) {
  UiDrawList list = beginList();
  for (u32 index = 0; index < 20; ++index)
    list.addRect({static_cast<float>(index) * 10.0f, 0, 8, 8}, 0xFFFFFFFF);
  std::vector<UiInstance> instances;
  const UiInstanceBuildResult result =
      buildUiInstances(list, shippedFont(), shippedIcons(), 5, instances);
  AE_EXPECT_EQ(result.emitted, 5u, "");
  AE_EXPECT_EQ(result.dropped, 15u,
               "estouro de orcamento e falha a ser reportada, nao a ser silenciada");
  AE_EXPECT_EQ(instances.size(), usize{5}, "e nunca passa da capacidade do buffer");
}

AE_TEST(builder_carries_the_clip_into_every_instance_of_a_run) {
  UiDrawList list = beginList();
  list.pushClip({50, 60, 200, 100});
  list.addText({50, 60, 200, 30}, "Water", 0xFFFFFFFF, defaultTheme().type.body);
  list.popClip();
  std::vector<UiInstance> instances;
  buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
  AE_EXPECT_TRUE(!instances.empty(), "");
  for (const UiInstance &instance : instances) {
    AE_EXPECT_TRUE(instance.clip[0] == 50.0f && instance.clip[1] == 60.0f, "");
    AE_EXPECT_TRUE(instance.clip[2] == 200.0f && instance.clip[3] == 100.0f,
                   "cada glifo carrega o recorte: nao ha scissor por painel");
  }
}

AE_TEST(builder_preserves_command_order) {
  // A ordem da lista e a profundidade. Reordenar por textura para economizar
  // troca de estado desenharia o painel por cima do rotulo dele.
  UiDrawList list = beginList();
  list.addRect({0, 0, 100, 100}, 0xFF0F0F0F);
  list.addText({0, 0, 100, 100}, "A", 0xFFFFFFFF, defaultTheme().type.body);
  list.addRect({0, 0, 100, 100}, 0x40000000);
  std::vector<UiInstance> instances;
  buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
  AE_EXPECT_EQ(instances.size(), usize{3}, "");
  AE_EXPECT_TRUE(kindOf(instances[0]) == static_cast<u32>(UiInstanceKind::Rect), "");
  AE_EXPECT_TRUE(kindOf(instances[1]) == static_cast<u32>(UiInstanceKind::Glyph), "");
  AE_EXPECT_TRUE(kindOf(instances[2]) == static_cast<u32>(UiInstanceKind::Rect), "");
}

AE_TEST(builder_appends_without_clearing_the_destination) {
  // O chamador acumula a interface e o HUD no mesmo buffer antes de submeter.
  UiDrawList list = beginList();
  list.addRect({0, 0, 10, 10}, 0xFFFFFFFF);
  std::vector<UiInstance> instances;
  instances.push_back(UiInstance{});
  buildUiInstances(list, shippedFont(), shippedIcons(), 64, instances);
  AE_EXPECT_EQ(instances.size(), usize{2}, "");
}
