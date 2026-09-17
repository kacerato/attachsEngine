// Camada pura da oclusão GPU-driven — o consumidor que faltava para o HZB
// construído em compute (ADR-016, consumidor C2).
//
// O produtor C1 já está no frame: `[HZB] produtor=compute níveis=6 base=160x346
// readback=nao`. A pirâmide fica residente na GPU e ninguém a lê — a decisão de
// oclusão continuava na CPU, que perdeu o readback e por isso registra
// `hzb_tested=0`. A cadeia de redução custa tempo de GPU medido e não remove um
// único triângulo. Este módulo existe para fechar esse laço: a decisão passa
// para um kernel compute que escreve `instanceCount` nos comandos indiretos que
// o passe opaco já consome.
//
// Regra estrutural desta camada: **ela não inventa matemática de visibilidade.**
// `cullDrawRecordReference` abaixo é o espelho exato de `draw_cull.comp` e a
// composição exata de `projectBoundsToHzbScreenRect` + `isOccludedByHzb` +
// `updateHzbHysteresis` quando a guarda de movimento é zero. Um teste tranca
// essa equivalência. Migrar a decisão para a GPU não pode ser a oportunidade de
// mudar silenciosamente o que é considerado visível.
//
// Sem Vulkan, sem Android, sem I/O: testável integralmente no host.
#pragma once

#include "core/base.h"
#include "renderer/hzb_visibility.h"

namespace ae::renderer {

// Um candidato por invocação do kernel. Layout std430 de 32 bytes, espelhado
// campo a campo em draw_cull.comp; `static_assert` no .cpp tranca o tamanho.
// O índice do registro é o índice do comando indireto correspondente: a CPU
// constrói os dois vetores na mesma ordem, de modo que o kernel não precisa
// carregar (nem poder errar) um ponteiro de destino.
struct alignas(16) GpuCullDrawRecord final {
  float boundsCenter[3]{};
  float boundsRadius = 0.0f;
  // Índice do estado de histerese persistente. É o drawIndex do pacote, não o
  // índice do comando: a histerese precisa sobreviver à reordenação
  // front-to-back e à troca de nível de LOD do mesmo objeto entre frames.
  u32 stateIndex = 0;
  u32 flags = 0;
  u32 reserved0 = 0;
  u32 reserved1 = 0;
};

// bit 0: participa do teste de oclusão. bit 1: o slot contém um comando real
// deste frame.
//
// Os dois bits existem separados porque o dispatch é gravado antes de a lista
// de comandos ser construída (ver InstancedRenderer::recordDrawCullDispatch) e
// portanto cobre a capacidade inteira da lista, não o número de comandos do
// frame. Um registro com flags zero é sobra: o kernel não escreve nada nele —
// nem o comando, nem o estado, nem telemetria. Escrever seria pior do que
// inútil, porque o `stateIndex` zerado de um slot de sobra apagaria a histerese
// do draw 0. `Present` sem `Testable` é um comando que existe mas que a
// política nunca testa; ele é publicado como visível e tem o estado zerado.
inline constexpr u32 GpuCullRecordTestable = 1u << 0;
inline constexpr u32 GpuCullRecordPresent = 1u << 1;

// Espelho do bloco de push constants de draw_cull.comp. 128 bytes: cabe nos
// 128 garantidos pelo Vulkan mesmo no aparelho mais modesto do perfil C.
struct alignas(16) GpuCullParameters final {
  float cameraPosition[3]{};
  float boundsScale = 1.0f;

  float cosineYaw = 1.0f;
  float sineYaw = 0.0f;
  float cosinePitch = 1.0f;
  float sinePitch = 0.0f;

  float tangentHalfHorizontal = 1.0f;
  float tangentHalfVertical = 1.0f;
  float nearPlane = 0.1f;
  float farPlane = 1000.0f;

  float boundsMargin = 0.0f;
  float normalizedDepthBias = 0.0f;
  // Guarda de movimento — ver buildGpuCullMotionGuard(). Os três termos abaixo
  // são as três formas de a pose ter mudado desde que a pirâmide foi
  // construída, e nenhum deles pode tornar o teste mais agressivo:
  //
  // `screenDilation`  — rotação: desloca a imagem inteira. Dilatação constante
  //                     do retângulo, em unidades normalizadas [0,1] de tela.
  // `viewDepthGuard`  — aproximação: metros subtraídos da profundidade de vista
  //                     mais próxima da esfera antes de projetar. Aumenta o
  //                     retângulo e aproxima o candidato ao mesmo tempo, que é
  //                     exatamente o efeito de andar na direção do objeto.
  // `translationDilationScale` — translação lateral: o deslocamento em tela é
  //                     inversamente proporcional à profundidade, então o termo
  //                     só pode ser resolvido por candidato, dentro do kernel.
  float screenDilation = 0.0f;
  float viewDepthGuard = 0.0f;

  // Mesma transformada 2x2 de pré-rotação que o vértice aplica antes da
  // rasterização (xx, xy, yx, yy).
  float surfaceTransform[4]{1.0f, 0.0f, 0.0f, 1.0f};

  u32 drawCount = 0;
  u32 hysteresisFrames = 0;
  u32 hzbBaseWidth = 0;
  u32 hzbBaseHeight = 0;

  u32 hzbLevelCount = 0;
  u32 maximumScannedTexels = 0;
  u32 flags = 0;
  float translationDilationScale = 0.0f;
  float orthographicHalfWidth = 0.0f;
  float orthographicHalfHeight = 0.0f;
  float reservedProjection0 = 0.0f;
  float reservedProjection1 = 0.0f;
};

// bit 0: a pirâmide desta chamada é utilizável. Zero faz o kernel escrever
// "visível" em todos os comandos e zerar a histerese — o mesmo fail-open que a
// CPU aplica quando a pirâmide não é válida, sem exigir um caminho separado no
// renderer.
inline constexpr u32 GpuCullPyramidUsable = 1u << 0;
inline constexpr u32 GpuCullOrthographic = 1u << 1;

// Tamanho local X de draw_cull.comp. Compartilhado para que o dispatch e o
// shader não possam divergir.
inline constexpr u32 GpuCullLocalSizeX = 64;

// Teto duro de varredura, idêntico ao HZB_MAXIMUM_SCAN_TEXELS do shader. Com a
// cadeia de seis níveis do runtime o nível escolhido cabe muito abaixo disto;
// existe para que uma configuração degenerada (um único nível na resolução da
// base) falhe aberto em vez de travar a GPU num laço de dezenas de milhares de
// texels. A referência de CPU aplica o mesmo teto para não divergir do kernel.
inline constexpr u32 GpuCullMaximumScanTexels = 4096;

inline constexpr u32 gpuCullGroupCount(u32 drawCount) {
  return (drawCount + GpuCullLocalSizeX - 1) / GpuCullLocalSizeX;
}

// Visão somente-leitura da pirâmide como o kernel a enxerga: dimensões da base
// e contagem de níveis, com cada nível seguinte derivado por divisão-teto — a
// mesma redução que createHzbResources planeja e que hzb_reduce_compute.comp
// executa. Os texels são fornecidos no layout plano de computeHzbLevelOffsets
// apenas para a referência de CPU; na GPU cada nível é uma imagem própria.
struct GpuCullHzbView final {
  const float *texels = nullptr;
  usize texelCount = 0;
  u32 baseWidth = 0;
  u32 baseHeight = 0;
  u32 levelCount = 0;
};

// Constrói a view a partir de uma pirâmide da camada pura. Falha (retorna
// false, deixando `out` zerado) quando a pirâmide não é válida ou suas
// dimensões não seguem a divisão-teto que o kernel assume.
bool buildGpuCullHzbView(const HzbPyramid &pyramid, GpuCullHzbView &out);


// Guarda de movimento entre a pose que produziu a pirâmide e a pose que está
// sendo testada.
//
// A pirâmide chega do frame anterior. O caminho de CPU resolvia isso desligando
// o estágio inteiro assim que a câmera se mexia (`hzb_motion_skip`), o que em
// um jogo em primeira pessoa significa nunca ocluir nada — exatamente o estado
// medido hoje. Em vez do penhasco, esta função devolve uma folga contínua:
// quanto maior o deslocamento entre as duas poses, mais o retângulo é dilatado
// e mais raso fica o candidato, até que nenhum corte seja possível. Não é uma
// prova de conservadorismo (nenhuma existe para HZB temporal com câmera livre);
// é um parâmetro explícito, monotônico e medível, e o teste tranca a
// monotonicidade: aumentar a guarda nunca aumenta o conjunto cortado.
struct GpuCullMotionGuard final {
  float screenDilation = 0.0f;
  float viewDepthGuard = 0.0f;
  float translationDilationScale = 0.0f;
  bool valid = false;
};

GpuCullMotionGuard buildGpuCullMotionGuard(const PerspectiveFrustum &frustum,
                                           const float pyramidCameraPosition[3],
                                           float pyramidYaw, float pyramidPitch);

// Preenche os parâmetros a partir do frustum já construído pelo frame e da
// transformada de superfície. Devolve false — deixando `out` zerado — para
// frustum inválido, dimensões de pirâmide degeneradas ou qualquer escalar não
// finito: a política é a mesma do resto da visibilidade, dado incerto não vira
// decisão de corte.
bool buildGpuCullParameters(const PerspectiveFrustum &frustum,
                            const HzbScreenTransform &screenTransform,
                            const GpuCullHzbView &hzb, const GpuCullMotionGuard &guard,
                            float normalizedDepthBias, u32 hysteresisFrames, u32 drawCount,
                            bool pyramidUsable, GpuCullParameters &out);

struct GpuCullOutcome final {
  // false = slot de sobra; nada é escrito e os demais campos não têm sentido.
  bool present = true;
  bool visible = true;
  bool tested = false;
  bool occludedThisFrame = false;
  // O objeto voltou a ser visível neste frame após ter acumulado oclusão.
  bool revived = false;
};

// Referência de CPU do kernel, instrução por instrução. Existe para três usos:
// provar a equivalência com a cadeia de CPU já validada, servir de oráculo para
// a validação por readback em hardware e permitir property tests sem GPU.
// `streak` é o estado persistente do objeto, atualizado no lugar.
GpuCullOutcome cullDrawRecordReference(const GpuCullParameters &parameters,
                                       const GpuCullDrawRecord &record,
                                       const GpuCullHzbView &hzb, u32 &streak);

} // namespace ae::renderer
