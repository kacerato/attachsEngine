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
// Aqui o arquivo carrega só o que o painel Qualidade expõe. O resto da política
// (cascatas, distâncias, orçamentos) continua vindo do nível escolhido, e o
// arquivo não inventa um segundo lugar para decidir aquilo.
#include "renderer/rendering_policy.h"

#include <string>
#include <string_view>

namespace ae::renderer {

inline constexpr u32 RenderingSettingsFileVersion = 1;

// Texto `chave=valor`, uma por linha, com a versão na primeira. Chave
// desconhecida é ignorada (arquivo de versão futura abre com o que se entende);
// valor ilegível numa chave conhecida recusa o arquivo inteiro.
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

} // namespace ae::renderer
