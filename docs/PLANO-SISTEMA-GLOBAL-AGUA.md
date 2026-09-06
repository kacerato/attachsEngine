# Sistema global de água: execução e critérios

## Objetivo

Água como recurso e superfície da engine, não um efeito condicionado ao nome da
cena. Preservar `WaterProfile`, consultas de gameplay e malhas finitas existentes.
O demo valida o sistema; não define regras dentro do renderer.

## Sequência

1. **Superfície e distância (integrada; validação de movimento pendente).** Grade contínua graduada,
   camera-relative, opt-in por contrato de material. Um draw, buffers imutáveis,
   densidade próxima preservada e frequências geométricas filtradas pela célula.
   Critérios: sem emendas entre patches, sem culling pelo bounds original,
   sem crescimento de triângulos por frame, teste de densidade e captura Android.
2. **Óptica coerente.** Separar absorção volumétrica, espalhamento, Fresnel,
   rugosidade e espuma. Substituir o lóbulo solar não normalizado; usar BRDF do
   ambiente. Composição HDR antes do tone mapping e scene color amostrável são
   necessários para transmissão RGB e refração correta. Fallback depth-only
   explícito; não chamar transmissão sem distorção de refração física.
3. **Espectro e interação.** Ondas analíticas e consultas CPU continuam como
   fallback. FFT somente após provedor, tempo, bounds, sincronização e budgets
   estarem definidos. Interações locais com domínio limitado, timestep fixo,
   amortecimento e entrada genérica para toque/corpos/chuva. Não ligar compute
   apenas porque o hardware suporta: o passe precisa existir e ser validado.
4. **Costa e volume.** Espuma por contato/quebra, máscaras de exclusão, detecção
   de câmera submersa, transmissão e caustics. Cada módulo tem enable, distância,
   resolução e custo próprios. Sem simular volume com uma cor sobre toda a tela.
5. **Authoring global.** Registro de propriedades compartilhado entre runtime,
   serialização e futura UI/Inspector/NoCode. Várias superfícies/perfis precisam
   de ownership explícito; a UBO global atual não equivale a suporte multiágua.
6. **Iluminação e validação.** Céu/reflexos da mesma fonte, exposição única,
   sombras e AA sem descontinuidades. Capturas rasa/profunda, horizonte, câmera
   em movimento e troca de cenas. Soak com CPU/GPU p50/p95, escala efetiva,
   memória e estado térmico. Meta 120 Hz não equivale a 120 FPS em resolução
   nativa; publicar a escala utilizada e nunca prometer frequência de clock.

## Decisão da primeira etapa

Usar grade contínua graduada em vez de ativar imediatamente o planner de clipmap
existente. Esse planner não tem draw path nem costura validada. Uma grade única
evita T-junctions e permite validar cobertura distante sem introduzir vários
draws ou novos buffers por frame. A cobertura é limitada pelo perfil/câmera,
não um oceano matematicamente infinito. Lagos continuam usando sua própria malha.

O novo bit de material documenta o contrato de vértices. UV1 guarda espaçamento
local e extensão de authoring; não reutilizar malhas arbitrárias nesse modo.
O build offline deve gerar esses dados e os testes devem verificá-los.

## Não concluído

Este documento não declara FFT, SSR, planar reflections, underwater, Inspector
ou clipmap operacional. Cada etapa só muda de estado após integração e prova.

## Evidência da primeira integração — 2026-09-05

- 348/348 testes nativos e 7/7 testes Python de geometria/assets passaram.
- Shaders recompilados e validados como SPIR-V; build Android Release passou.
- Grade camera-relative instalada e capturada em `build/android-validation/grid-check.png`.
- Água mantém 131.072 triângulos; nenhuma alocação/upload de malha por frame.
- Distribuição GGX/visibilidade Smith extraídas para include compartilhado com
  materiais opacos. Água passa a usar Fresnel solar e BRDF integrada do ambiente
  quando disponível. Materiais opacos mantêm as mesmas equações.
- A faixa clara no horizonte continua visível: investigar/compor céu e reflexos
  no mesmo domínio HDR, sem esconder diferença com névoa ou cor específica da cena.
- Movimento prolongado, cena finita de regressão e soak de performance ainda
  pendentes. Não há comprovação nova de 120 FPS sustentados.
