#pragma once

// GT (DESEMPENHO-GRAFICOS-VIVOS-2026-10-17): registro de desempenho enviado pelos
// testadores que aceitam a telemetria. Uma janela agregada por vez (nunca quadro
// a quadro), no esquema "astra.telemetry.window/1". O nativo descreve o que mediu;
// o envelope do aparelho (modelo, GPU, versão do app) e o envio ficam no Java.
//
// Nada pessoal entra aqui: só números do quadro, o nível de qualidade resolvido e
// um rótulo de cena que o chamador já reduziu a hash ou a id de bancada.

#include "core/base.h"
#include "profiler/frame_statistics.h"

#include <string>
#include <string_view>

namespace ae::profiler {

struct TelemetryWindowContext {
  u64 sequence = 0;            // contador da sessão; o servidor detecta lacunas
  std::string_view scene;      // hash da cena do usuário ou id de bancada ("vila-astra")
  std::string_view mode;       // "play" | "edit"
  std::string_view tier;       // nível resolvido: "c" | "b" | "a" | "s"
  u32 renderWidth = 0, renderHeight = 0;
  float renderScale = 1.0f;
  i32 thermalStatus = -1;      // AThermal status; -1 = indisponível
  float thermalHeadroom = -1.0f;
  u32 targetFps = 0;
  u32 drawCalls = 0;
  u64 triangles = 0;
};

inline constexpr std::string_view TelemetryWindowSchema = "astra.telemetry.window/1";

// Um objeto JSON numa linha. Métricas sem amostras válidas (GPU sem timer) saem
// como null em vez de zero, para o servidor não confundir "não medido" com "grátis".
// Devolve vazio quando o resumo não tem amostras.
std::string telemetryWindowJson(const FrameProfileSummary &summary, const TelemetryWindowContext &context);

// Escapa texto para dentro de aspas JSON (controle, aspas e barra invertida).
std::string telemetryJsonString(std::string_view text);

} // namespace ae::profiler
