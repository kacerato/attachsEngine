# Registro de Decisões Arquiteturais (ADRs)

Item 0.4.1 do plano principal (`docs/PLANO-ENGINE-MOBILE.md`, Apêndice A).
Cada ADR documenta uma decisão de arquitetura já tomada, com evidência real
do código quando implementada, ou registrada preventivamente quando a
implementação ainda não começou. ADRs não substituem `docs/ESTADO.md`/
`docs/MATRIZ-MARCOS.md` (o estado vivo item a item do plano) — registram o
**porquê** de uma decisão, não o progresso corrente.

| # | Decisão | Estado |
|---|---|---|
| [ADR-01](ADR-01-LINGUAGENS.md) | C# para engine/editor, C++ para núcleo | ✅ implementada (divergência: sem IDL `.aidl`) |
| [ADR-02](ADR-02-ECS-ARQUETIPOS.md) | ECS por arquétipos com fachada de nós | ✅ implementada |
| [ADR-03](ADR-03-VULKAN.md) | Vulkan 1.3 como único backend gráfico | 🟡 parcial (1 GPU validada) |
| [ADR-04](ADR-04-RENDER-GRAPH.md) | Render graph com aliasing e fusão de subpasses | 🟡 implementada, só headless |
| [ADR-05](ADR-05-JOLT-PHYSICS.md) | Jolt Physics em vez de solver próprio | ✅ implementada |
| [ADR-06](ADR-06-AST-UNICA-AETHERFLOW.md) | AST única com três representações (AetherFlow) | 🟡 fundação pronta, UIs não iniciadas |
| [ADR-07](ADR-07-FORMATO-DUAL.md) | Formato dual texto+binário | ✅ implementada |
| [ADR-08](ADR-08-PROCESSO-PLAY-SEPARADO.md) | Processo separado para o Play | ⬜ não implementada |
| [ADR-09](ADR-09-BUILD-NUVEM.md) | Build nativo em nuvem, dados no dispositivo | ⬜ não implementada |
| [ADR-10](ADR-10-PIPELINE-UNICO.md) | Um único pipeline de renderização escalável | ⬜ não implementada |
| [ADR-11](ADR-11-MENU-RADIAL.md) | Menu radial como mecanismo primário de comando | 🟡 validada em protótipo, não em produto |
| [ADR-12](ADR-12-WAL-EDICAO.md) | WAL de edição com recuperação total | ✅ implementada |
| [ADR-013](ADR-013-PHYSICS-2D-BACKEND.md) | Backend de física 2D (Jolt restrito vs. Box2D) | 🟡 proposta, aguardando perfis Android B/C |
| [ADR-014](ADR-014-POLITICA-GLOBAL-RENDERIZACAO.md) | Política global de renderização orientada por budgets | 🟡 proposta, implementação não iniciada |
| [ADR-015](ADR-015-AEMAP-V2-VERTICES-COMPACTOS.md) | AEMAP v2 com vértices compactos e leitura legada | 🟡 integrada em Adreno; matriz pendente |
| [ADR-016](ADR-016-COMPUTE-RHI-RENDER-GRAPH.md) | Compute como capacidade do RHI e do Render Graph | ✅ fundação implementada e validada em Adreno |

**Legenda:** ✅ implementada e testada · 🟡 parcial ou pendente de evidência ·
⬜ decisão registrada, sem código ainda.

Numeração ADR-01 a ADR-12 corresponde à tabela original do Apêndice A do
plano principal. ADR-013 foi aberta posteriormente para uma decisão que não
constava naquela lista original (por isso o padrão de dígitos difere). ADR-014
registra a política global de rendering/perfis antes da implementação. ADR-015
versiona a primeira mudança persistente de layout guiada por essa política.
ADR-016 registra a fundação compute transversal, posterior à lista original.
