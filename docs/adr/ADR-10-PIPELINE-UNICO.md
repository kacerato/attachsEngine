# ADR-10 — Um único pipeline de renderização escalável

- **Estado:** aceita como decisão preventiva; **nenhum pipeline direto implementado ainda (nem único, nem múltiplo)**
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1, 2.4
- **Decisores:** arquitetura, renderer

## Contexto

Fragmentar o pipeline de renderização em variantes incompatíveis (como
URP/HDRP na Unity) força o usuário a escolher cedo demais, entre opções que
divergem em recursos e compatibilidade de asset — e fragmenta o
conhecimento da comunidade entre "quem usa URP" e "quem usa HDRP". O plano
do Aether cita isso explicitamente como "o maior erro estratégico da
Unity".

## Decisão

Um único pipeline de renderização, escalável de "modo bateria" a "modo
showcase" através de capabilities detectadas em runtime (ver ADR-03,
`DeviceProfile`), nunca trocando de renderizador ou de conjunto de
assets/materiais incompatíveis entre perfis de hardware.

## Alternativas descartadas

1. **Pipelines separados por perfil (URP/HDRP-like).** Fragmenta assets e
   conhecimento da comunidade — o mesmo material/cena teria que ser refeito
   ou portado entre pipelines, e tutoriais/conteúdo de terceiros
   fragmentariam por "qual pipeline você usa".

## Estado de implementação

**Nenhum pipeline de renderização direto existe ainda — nem único, nem
múltiplo.** O que existe hoje é fundação de baixo nível: RHI básico parcial
(`native/rhi/device.cpp`, perfis S/A/B/C em `device_profile.h/.cpp` — ver
ADR-03) e Render Graph headless (ver ADR-04). Os dois renderers de prova de
conceito no shell Android
(`native/platform/android/android_triangle_renderer.*`,
`instanced_renderer.*`) são hardcoded, sem depth buffer, sem material real e
sem passar pelo Render Graph — não são o pipeline de produto, são PoCs de
interop.

`docs/MATRIZ-MARCOS.md` (Etapa 2.4 — Pipeline de renderização direta) marca
todos os 6 itens (depth prepass/Forward+, PBR, sombras, IBL,
transparência/partículas, pós-processamento) como "não iniciado". As Etapas
2.5 (culling/batching) e 2.6 (2D) também estão inteiramente não iniciadas.

Como nenhum pipeline foi construído ainda, esta decisão não tem como ser
observada em código — nem confirmada nem contradita. É registrada aqui
**preventivamente**, para que a Etapa 2.4 comece já com a restrição
arquitetural definida, em vez de essa pergunta ficar em aberto quando o
trabalho começar.

## Consequências

- Quando a Etapa 2.4 começar, todo recurso de renderização deve ser
  projetado para escalar via `DeviceProfile` (perfis S/A/B/C já definidos
  em `native/rhi/device_profile.h` — ver ADR-03), nunca via um segundo
  conjunto de shaders/pipeline paralelo e incompatível.
- Um material ou cena criado num perfil deve continuar carregando e
  renderizando (com qualidade degradada aceitável) em qualquer outro
  perfil, nunca falhar por incompatibilidade de pipeline.
