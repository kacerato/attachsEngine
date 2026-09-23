#pragma once
// Configurações de renderização do PROJETO, no disco e na tela do editor.
//
// A Unity guarda isso em Project Settings > Quality (os níveis, com Render
// Scale, Upscaling Filter e Anti-aliasing no URP Asset de cada nível); a Godot
// em Project Settings > Rendering (Scaling 3D Scale/Mode, MSAA/TAA/FXAA, Max
// FPS). As duas fazem o autor escolher, no editor, o nível de qualidade e a
// resolução interna — e as duas renderizam o viewport do EDITOR em resolução
// nativa, sem escala dinâmica, porque é ali que se julga a imagem.
//
// O arquivo v2 preserva o nível e todos os overrides autorais que a política
// consome. Assim o painel pode começar pelo preset e aprofundar por eixo sem
// criar um segundo contrato ou perder os ajustes ao reabrir o projeto.
#include "renderer/rendering_policy.h"

#include <string>
#include <string_view>

namespace ae::renderer {

inline constexpr u32 RenderingSettingsFileVersion = 2;

// Texto `chave=valor`, uma por linha, com a versão na primeira. Chave
// desconhecida é ignorada nas versões suportadas; versão futura ou valor
// ilegível numa chave conhecida recusa o arquivo inteiro, sem alterar a saída.
std::string writeRenderingSettings(const ProjectRenderingSettings &settings);
bool readRenderingSettings(std::string_view text, ProjectRenderingSettings &out);

// O que o editor usa quando o projeto não decidiu: resolução nativa, sem escala
// dinâmica e alvo de 60 Hz. A escala dinâmica é ferramenta de jogo em
// execução; no editor ela troca a imagem que o autor está julgando por uma
// mais borrada assim que a GPU aperta — foi o que aconteceu no aparelho, com o
// viewport caindo para 50% em um segundo e não voltando.
ProjectRenderingSettings withEditorDefaults(ProjectRenderingSettings settings);

// Nomes do painel, no vocabulário dos níveis de qualidade da Unity.
const char *qualityLevelLabel(QualityPreset preset);
const char *antiAliasingLabel(AntiAliasingMode mode);
const char *upscalingFilterLabel(UpscalingFilter filter);
const char *shadowQualityLabel(ShadowQuality quality);
const char *ambientQualityLabel(AmbientQuality quality);
const char *postQualityLabel(PostQuality quality);
const char *textureQualityLabel(TextureQuality quality);
const char *featureOverrideLabel(FeatureOverride value);

} // namespace ae::renderer
