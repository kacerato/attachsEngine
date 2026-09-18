# Execução do plano universal de cenários realistas — registro por bloco

Registro honesto do que foi **implementado** e do que foi **validado**, na
granularidade dos blocos G0–G6 de
[PLANO-UNIVERSAL-CENARIOS-REALISTAS-2026-09-17.md](PLANO-UNIVERSAL-CENARIOS-REALISTAS-2026-09-17.md).

Vocabulário (o mesmo da §9 do plano): **inventariado** → **especificado** →
**implementado não validado** → **validado no escopo indicado**.

**Escopo de validação desta rodada:**

1. Testes de host compilados e executados com g++ 16 (MinGW-w64): 48 testes
   novos verdes (`test_component_contracts`, `test_component_preset`,
   `test_component_recipes`, `test_import_geometry_profile`,
   `test_import_scene_impact`, `test_import_format_compat`,
   `test_import_node_exclusion`), todos registrados em `native/CMakeLists.txt`.
2. **Build nativo completo para arm64-v8a pelo Gradle/NDK, com `-Wall -Wextra
   -Wpedantic -Werror`: `BUILD SUCCESSFUL`**, APK gerado. Este é o build que
   valida o código no compilador de verdade do alvo — o host local não tem os
   cabeçalhos do Vulkan e por isso não compila o alvo inteiro.
3. **Evidência no aparelho** (Xiaomi 25053PC47G, ARM64): APK instalado, shell
   ASTRA abre, projeto abre, seleção e gizmo funcionam. Duas coisas foram vistas
   funcionando na tela: o agrupamento novo do Corpo físico aparecendo como abas
   **Corpo | Início** assim que o movimento deixa de ser estático
   (`build/astra-g2-body-dynamic.png`), e o **diff campo a campo do preset**, com
   "1 campo diferente", "Marcar tudo / Desmarcar" e a linha `Renderizar: falso →
   verdadeiro` com a marca de levar ou não (`build/astra-preset-diff.png`). Em
   ambos os casos o projeto usado foi devolvido ao estado inicial — componente
   removido, preset excluído, valor restaurado.

**O que NÃO foi validado:** a aba Perfil da importação no aparelho (exige abrir
o seletor de arquivos do sistema com um GLB de verdade), o caminho completo de
importação com uma fonte real (as normais/tangentes derivadas têm teste de host, não medição em
GLB de produção), desempenho e qualquer comparação visual controlada.

## G0 · matriz e corpus — implementado não validado em aparelho

| Entrega | Onde vive | Estado |
|---|---|---|
| Registro de capacidades do motor com estado real (implementada / limitada pelo aparelho / planejada), dono e limite | `native/core/engine_capability.h` | implementado; 32 capacidades declaradas |
| Contrato por propriedade: capacidade exigida, consumidor e invalidação, com herança do esquema do componente | `native/scene/components.h`, `native/scene/component_schema.h` | implementado nos 9 tipos registrados |
| Matriz de propriedades **gerada do código**, com padrão, domínio, unidade, grupo, consumidor, invalidação e condicionalidade | `native/scene/component_reflection.h` → [MATRIZ-PROPRIEDADES.md](../componentes/MATRIZ-PROPRIEDADES.md) | implementado; regenerar com `aether_tests --write-property-matrix docs/componentes/MATRIZ-PROPRIEDADES.md` |
| Auditoria que recusa propriedade sem identidade, identidade repetida, componente sem consumidor, capacidade não registrada e propriedade apoiada em capacidade **planejada** | `auditComponentContracts()` + `tests/native/test_component_contracts.cpp` | validado no host |
| Uma verdade só entre a tabela de luzes do renderer e o registro de capacidades | `static_assert` em `native/renderer/punctual_lights.h` | validado em tempo de compilação |
| Visibilidade condicional declarada no descritor, e não reescrita por índice de widget no inspetor | `native/editor/editor_properties.h` | implementado |
| Grupos de Inspector em Corpo físico (Corpo / Início / Amortecimento), Personagem (Cápsula / Locomoção) e Olhar (Sensibilidade / Limites); triplas de velocidade e giro inicial | `physics_body.h`, `character.h`, `camera_look.h` | implementado |
| `enabled` do Comportamento em C# passa a ter PropertyId — desligável por API, preset e animação, não só pelo dedo | `native/scene/script_behavior.h` | implementado |

**O que o bloco NÃO fecha:** o corpus de três fontes representativas com
proveniência e referência visual (§9 do plano) continua pendente; nada aqui
substitui a avaliação de qualidade de fonte.

## G1 · composição e presets — parcial

| Entrega | Onde vive | Estado |
|---|---|---|
| Recursos declarados como reflexão do componente (`ComponentResourceBinding`), com slots, herança e ausência declarada distintas | `native/scene/components.h`, `native/scene/mesh_renderer.h` | implementado |
| Grafo de impacto, reparo de recurso e substituição local passam a percorrer os bindings declarados em vez do caminho especial de MeshRenderer | `native/editor/editor_component_impact.h` | implementado |
| Diff campo a campo entre valor atual e candidato, com aplicável / não aplicável e motivo | `native/scene/component_preset.h` | validado no host |
| Aplicação **por campo** em uma transação: nada entra se a combinação deixar o componente inválido | `applyComponentFields()` | validado no host |
| Dependências de recurso de um valor e remapeamento por identidade entre projetos | `componentResourceDependencies()`, `remapComponentResources()`, `missingResourceDependencies()` | validado no host |
| Receita multi componente: captura do objeto inteiro, formato de biblioteca v2 com leitura da v1, aplicação em uma transação com Add resolvendo exigências | `native/editor/editor_component_presets.h`, `EditorSession::applyComponentRecipe` | biblioteca validada no host; aplicação implementada, sem teste de sessão |
| Painel de presets com diff interativo (marcar campo a campo, marcar tudo, desmarcar), preview de receita com o que é adicionado, atualizado e exigido | `native/editor/editor_screen.cpp`, `editor_session.cpp` | **validado no aparelho** (`build/astra-preset-diff.png`) |

**Lacuna fechada em seguida (ver G3 abaixo):** amostragem, canais, superfície e
fatores por slot eram dados do MeshRenderer **sem PropertyId**, o que deixava a
aplicação seletiva sem alcançá-los. Passaram a ter identidade endereçável por
slot, e o painel deixou de precisar do aviso.

## G2 · importação autorável — parcial (geometria derivada e diagnóstico de UV)

| Entrega | Onde vive | Estado |
|---|---|---|
| Perfil de importação v2 com **Normais** (Importar / Calcular), **Modo das normais** (por área / por ângulo) e **Tangentes** (Importar / Calcular), com os nomes do Model Import Settings da Unity | `native/resources/import_profile.h/.cpp` | validado no host |
| Consumidor real: geração de normais quando a fonte não traz NORMAL — o glTF permite e este renderer não — e recálculo quando o perfil pede; geração de tangente forçada por perfil | `native/resources/gltf_import.cpp` | validado no host |
| Campo ausente ou modo desconhecido no arquivo de perfil falha fechado, em vez de assumir padrão | `parseImportProfile` | validado no host |
| Chave do cache de derivados inclui a geometria derivada: um perfil novo não reusa o derivado do antigo | `native/resources/import_cache.cpp` | validado no host |
| Diagnóstico **sem UV com textura declarada**: a fonte declara textura e a primitiva não tem TEXCOORD | `gltf_import.cpp` → relatório de importação | validado no host |
| Diagnóstico **densidade de texel desigual**: razão entre a maior e a menor densidade dentro da primitiva, com a pior razão do arquivo. É a medida que não depende da resolução da textura — e por isso a que explica por que trocar a imagem por uma maior não conserta estiramento | `gltf_import.cpp` → relatório de importação | validado no host |
| **Importar câmeras** (Import Cameras da Unity): a câmera do arquivo vira componente `Camera` no objeto do nó, com lente (perspectiva/ortográfica), planos e a meia volta que converte o −Z do glTF no +Z desta engine | `gltf_import.cpp`, `import_node_map.*`, `editor_import_reconcile.cpp` | validado no host |
| Câmera em nó com geometria ou filhos é **recusada com motivo**, em vez de girar a geometria do autor para acertar o enquadramento | `gltf_import.cpp` | validado no host |
| **Diff da cena aberta antes de publicar**: quantos objetos desta cena estão presos à fonte, quais saem, quais ficam órfãos (com os nomes) e quantos nós novos entram em cada instância | `importSceneImpact()` em `editor_import_reconcile.*` | validado no host |
| **Perfil por nó — excluir nó da importação**: o nó (e a subárvore) não vem para a cena, mas **continua no mapa com a identidade dele**. Para a cena ele sai pela mesma regra de nó removido (sem edição local sai, com edição fica órfão); reincluir traz de volta o **mesmo** nó, reintroduzido na revisão nova para não ser confundido com "apagado pelo autor". Mudar só a exclusão, com os mesmos bytes, avança a revisão do mapa | `import_node_map.*` (`excluded`, `markExcludedNodes`), `import_profile.*` (schema 4), `editor_import_reconcile.cpp` | validado no host, inclusive a reconciliação que remove e traz de volta |
| Interruptor por nó na aba **Estrutura**, com a herança do pai visível (filho de excluído aparece apagado e sem toque); o impacto na cena é refeito a cada toque sem reler o arquivo, e excluir não pede nova preparação | `editor_screen.cpp`, `editor_session.cpp` | implementado, compila para o alvo; sem evidência em aparelho |
| Reabrir o projeto usa a exclusão com que a fonte foi publicada | `EditorSession::reopenSources` | implementado |
| Controles no painel de importação (aba Perfil) e as linhas novas no relatório | `native/editor/editor_screen.cpp`, `editor_session.cpp` | implementado, compila para o alvo; sem evidência em aparelho |

**Limites declarados:**

- não há a opção "None" de normais da Unity (aqui ela só produziria superfície
  preta), nem **Smoothing Angle** — ângulo de suavização exige duplicar vértices
  na borda dura, que é mudança de topologia e não de atributo;
- **Import Lights não entra aqui.** O glTF usa unidades fotométricas (lux para
  direcional, candela para pontual/spot) e a escala de intensidade desta engine
  ainda não é calibrada — importar os valores do arquivo produziria cenas
  estouradas. A capacidade `render.light.photometric` está declarada como
  *planejada* no registro do motor, e a importação de luzes vai junto com ela no
  bloco de iluminação (G4);
- a câmera importada entra **desligada** (`enabled = false`): trazer o
  enquadramento do editor 3D não pode sequestrar a câmera do Play.

## G3 · materiais e malhas — parcial (endereço por slot)

Fecha a lacuna declarada no G1 e atende a §5.4 do plano ("Material: UV set e
transform por binding", "canais", "superfície") na granularidade que ela pede.

| Entrega | Onde vive | Estado |
|---|---|---|
| `ComponentSlotNumber` / `ComponentSlotEnum`: propriedade que existe uma vez **por slot**, com identidade persistente e escrita que nunca cria slot | `native/scene/components.h` | validado no host |
| MeshRenderer declara por slot: tipo de superfície, faces, corte do alfa; canais de rugosidade/metálico/oclusão, origem e força da oclusão, inversão Y da normal, origem do alfa; conjunto de UV, repetição, filtro, deslocamento, escala e rotação; os 11 fatores PBR; e o próprio interruptor de override | `native/scene/mesh_renderer.h` | validado no host |
| Matriz, auditoria, diff e aplicação seletiva passam a endereçar slot; a auditoria recusa propriedade por slot sem escrita | `component_reflection.h`, `component_preset.h` | validado no host |
| Rótulos no vocabulário do URP Lit (Surface Type, Alpha Clipping, Render Face, Tiling, Offset) | `mesh_renderer.h` | implementado |

**Limite declarado:** as propriedades de amostragem escrevem o valor em **todos
os bindings do slot** e leem o binding de cor base. Endereçar (slot × binding)
por propriedade multiplicaria a lista por cinco; a escolha por binding continua
no editor de material, que endereça os dois índices. Também permanece: aplicar
valores nunca cria slot — trocar a quantidade de slots é mudar a geometria do
objeto, e só a substituição completa do componente faz isso. O painel diz isso
quando (e só quando) o preset tem outra quantidade de slots.

## G4–G6

Ainda **inventariados/especificados** pelo plano; nada implementado nesta rodada.
