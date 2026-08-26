# ADR-11 — Menu radial como mecanismo primário de comando

- **Estado:** aceita como decisão de UX; validada em protótipo, **não implementada no runtime de produto**
- **Data:** 26/08/2026
- **Item do plano:** 0.4.1, 3.2.4, 8.5
- **Decisores:** arquitetura, UX/editor

## Contexto

O editor mobile não tem o equivalente natural de atalhos de teclado de
desktop — não há tecla física para memorizar. A alternativa óbvia (barras de
ferramentas, menus hierárquicos) exige mirar precisamente num alvo pequeno
com o dedo, repetidamente, para a mesma ação — lento e sem construir memória
muscular.

## Decisão

Menu radial (pie menu) acionado por toque-e-segurar é o mecanismo primário
de comando do editor. Memória muscular direcional (sempre arrastar "para
cima-esquerda" para a mesma ação, independente de onde o menu abriu) é o
substituto touch dos atalhos de teclado de desktop.

## Alternativas descartadas

1. **Barras de ferramentas.** Exige mirar num alvo fixo pequeno,
   repetidamente — sem ganho de velocidade com a prática, ao contrário de
   memória muscular direcional.
2. **Menus hierárquicos (nested).** Multiplica o número de toques por
   comando e não aproveita a vantagem do toque-e-arrasta em relação ao
   clique-preciso.

## Evidência (protótipo, não produto)

- `prototype/editor.html`: implementação completa em JavaScript/Canvas —
  elemento `#radial`/`#radialSvg`, lógica de abertura por toque-e-segurar
  com detecção de ~320 ms de imobilidade, estado
  `radialOpen`/`radialCenter`/`radialPick`.
- Screenshot de evidência: `prototype/shot-radial.png`.
- `docs/MATRIZ-MARCOS.md`, item 3.2.4: classificado como "PoC" —
  "`prototype/editor.html` prova o conceito de UX; não é o runtime de
  produto".

## Estado de implementação

**Não existe em `managed/` (produto/runtime).** Busca por
"radial"/"RadialMenu" no código C# não encontra nenhum resultado. O
protótipo HTML prova a interação (câmera, seleção, gizmos, menu radial e
Inspector em landscape), mas explicitamente **não** prova desempenho gráfico
real, integração com a engine, nem a implementação em runtime de produto
(`docs/ESTADO.md`, tabela "O que é protótipo ou prova de conceito").

## Consequências

- Quando a Etapa 3.2/8.5 (menu radial de produto) começar, o protótipo HTML
  é a referência de interação a replicar/melhorar, não um ponto de partida
  de código a portar diretamente — é JavaScript/Canvas, sem relação com a
  stack C#/Vulkan real.
- Qualquer UI de comando alternativa introduzida antes do menu radial de
  produto (ex.: um menu de contexto simples para destravar um fluxo cedo)
  deve ser tratada como provisória, não como substituição desta decisão.
