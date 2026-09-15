# R0 — base congelada, contratos de execução e checklist dos 112 itens

14/09/2026 · branch `codex/gameplay-runtime`. Primeira etapa do [relatório de importação, texturas e malhas](RELATORIO-IMPORTACAO-TEXTURAS-MALHAS-2026-09-14.md). Este documento não entrega funcionalidade: registra de onde o trabalho parte, o que já existe e em que estado, para que R1–R10 estendam caminhos reais em vez de reiniciar codecs, submalhas ou identidade.

## 1. Base

| Item | Valor |
|---|---|
| SHA de partida | `6c7991e851110fc105291a264071eef37f088ae8` (`docs(import): conferencia da Entrega 4 no aparelho`) |
| Alterações locais na partida | Artefatos de build de `android/app/.cxx` (rastreados, preservados, fora dos commits) e o próprio relatório, não versionado |
| Último APK conferido no aparelho | `2E0117F6…3E` (antes da linha de prévia que conta texturas ASTC) |
| Suíte do host na partida | 895/897; as duas falhas antigas são `every_console_row_is_reachable_by_touch` e `the_ide_toolbar_is_icons_and_the_rest_lives_in_one_menu` |
| Aparelho de conferência | Xiaomi 25053PC47G, Android 16, projeto `M08Recursos0913k` com 8 fontes registradas |

Arquivos compartilhados de alto risco (um responsável por vez, conforme o relatório): `native/editor/editor_session.cpp`, `native/editor/editor_screen.cpp`, `native/platform/android/android_main.cpp`.

## 2. Documentação válida e reconciliação

Documentos que descrevem o estado atual: [M08.2 identidade](M08-2-IDENTIDADE-RECONCILIACAO.md), [E2 submalhas e materiais](M08-E2-SUBMESHES-MATERIAIS.md), [E3 texturas PBR](M09-E3-TEXTURAS-PBR.md), [E4 codecs, reflexão e dependências](M08-M09-E4-DEPENDENCIAS-CODECS.md), [inspetor do objeto](INSPETOR-OBJETO-ACOES.md), [plano mestre](PLANO-MESTRE-ASTRA-EDITOR-RUNTIME-ASSETS.md).

A [delegação M08/M09](DELEGACAO-M08-M09-IMPORTACAO-AUTORAL.md) foi escrita antes das entregas e continua valendo como pedido, não como retrato do código. Reconciliação por entrega:

| Delegação | O que foi entregue | O que a delegação pedia e ainda falta |
|---|---|---|
| E1 — identidade e reimportação | Mapa persistente de nós, vínculo por instância com base, reconciliação três vias, órfãos, desvincular, reverter, journal com o mapa. Validado no host e no aparelho. | Resolução de conflito por item na interface (hoje uma política global de ambiguidade); mapa ilegível ainda regenera identidades sem diagnóstico suficiente (R6). |
| E2 — submalhas e materiais | `MeshRenderer` v3 com slots, `MaterialAsset` 1, alcance instância/compartilhado, migração de filhos artificiais. Host e aparelho. | Histórico da edição compartilhada; material do projeto só tem fatores (R4). |
| E3 — texturas e PBR | PNG/JPEG, mips em espaço linear, bindless, slots base/normal/MR/emissiva, limite residente, tangentes geradas. Host e aparelho. | Oclusão; transformação de UV por textura (hoje assada quando o material concorda); referências de textura editáveis; imagens como recursos próprios na transação (R4). |
| E4 — dependências e codecs | Draco, meshopt, KTX2 (RGBA8 ou ASTC 4x4), reflexão, `.gltf` empacotado com manifesto. Host e aparelho, exceto a seleção múltipla pelo seletor do sistema. | Manifesto `.deps` fora da transação principal (R6); seleção múltipla não conferida no aparelho. |

Comentários de código que contradiziam as entregas foram corrigidos nesta etapa: o cabeçalho de `native/resources/gltf_import.h` ainda dizia “só GLB, o seletor entrega um arquivo” e “transformação de UV contada como não aplicada”.

## 3. Contrato: persistido versus transitório

| Persistido no projeto (fonte de verdade autoral) | Onde | Regenerável? |
|---|---|---|
| Fonte importada (GLB; `.gltf` chega empacotado) | `Fontes/<nome>.glb` | Não: é a origem |
| Manifesto de dependências do `.gltf` | `Fontes/<nome>.glb.deps` | Não (proveniência) |
| Registro de recursos (GUID, caminho, hash, versão do importador) | `.astra/assets.astra` | Não |
| Mapa de nós por fonte (identidade M08.2) | `.astra/imports/<guid>.nodes` | Não: perder regenera identidades |
| Material do projeto | `Materiais/<nome>.material` | Não |
| Cena autoral (objetos, vínculos, overrides) | `scenes/editor.aescene` | Não |
| Journal da transação de importação | `.astra/import-transaction/` | Temporário, só durante a transação |

| Transitório (reconstruído a cada abertura) | Dono | Observação |
|---|---|---|
| `GltfImport` (geometria, texturas decodificadas, mips) | worker de importação/reabertura | R2: derivado regenerável em `.astra/cache/imports/<chave>.aic`; apagar custa só tempo |
| Relatório de residência de texturas | `EditorSession::textureResidency` | R2: recalculado a cada publicação |
| Biblioteca achatada (`ImportedLibrary`) e pacote adotado | `EditorSession` / `EditorMapScene` | Refeita na publicação |
| Imagens, buffers e samplers Vulkan; slots bindless | `InstancedRenderer` / `DirtRoadResources` | Perdidos com a surface; republicados |
| Cópias de picking | `EditorMapScene` | Derivadas do pacote publicado |
| Estado da prévia de importação e do worker de reabertura | shell Android | Descartado ao trocar de projeto ou cancelar |

Regra para R2 em diante: nada da segunda tabela pode ser exigido para abrir um projeto; um derivado em cache só vale com chave de conteúdo das dependências, perfil de importação e versões de importador, codecs e schema.

## 4. Checklist dos 112 itens

Estados do relatório: **E** existente a preservar · **P** parcial · **N** lacuna · **A** exige auditoria de subsistema. As quatro colunas seguintes são independentes: um item implementado não está automaticamente integrado ao editor, nem validado. **sim** = comprovado; **parcial** = parte do contrato; **—** = não.

### 4.1 Importação (24)

| ID | Função | Rel. | Implementado | Integrado ao editor | Host | Aparelho | Pacote |
|---|---|---|---|---|---|---|---|
| I01 | Escala de importação | P | parcial (R3: escala uniforme por passos nas raízes; sem unidade declarada) | sim (aba Perfil) | sim | — | R3 |
| I02 | Conversão de eixos | P | — (convenção glTF preservada, sem opção) | — | — | — | R3/R7 |
| I03 | Preservar hierarquia | E/P | sim (árvore e pivôs) | sim | sim | sim | R3 |
| I04 | Inclusão por nó | N | — | — | — | — | R3/R6 |
| I05 | Tipo e nome da raiz | P | parcial (grupo para várias raízes) | parcial | sim | sim | R3/R6 |
| I06 | Câmeras e luzes | N | — (contadas como omitidas) | — | — | — | R7 |
| I07 | Normais | P | parcial (geradas se ausentes) | — | sim | — | R5/R7 |
| I08 | Tangentes | P | parcial (geradas com mapa normal, não MikkTSpace) | — | sim | sim | R4/R7 |
| I09 | Soldagem | N | — | — | — | — | R7 |
| I10 | Organização de buffers | P/A | — | — | — | — | R7 |
| I11 | Precisão de armazenamento | N | — | — | — | — | R7 |
| I12 | Dados CPU editáveis | P | parcial (cópias de picking) | — | — | — | R2/R7 |
| I13 | LOD automático | N | — | — | — | — | R7 |
| I14 | Geometria de sombra | A | — | — | — | — | R7/R9 |
| I15 | UV de iluminação | A | — | — | — | — | R9 |
| I16 | Colisão na importação | P | parcial (ajuste de colisor no editor, slot 0) | parcial | — | — | R7 |
| I17 | Extração/remapeamento de material | P | parcial (material do projeto a partir do slot) | sim | sim | sim | R4/R6 |
| I18 | Política por categoria | P | — (reconciliação única) | — | — | — | R3/R6 |
| I19 | Perfil reutilizável | N | parcial (R3: perfil por fonte + padrão do projeto; sem presets nomeados) | sim | sim | — | R3 |
| I20 | Pós-processamento | A | — | — | — | — | R6/R9 |
| I21 | Clips de animação | N | — (contados como omitidos) | — | — | — | R8 |
| I22 | Esqueleto | N | — (contado como omitido) | — | — | — | R8 |
| I23 | Inspeção de saídas | P | parcial (R3: abas Estrutura e Texturas com dados estruturados; sem prévia 3D) | sim | sim | parcial (contadores E4) | R3 |
| I24 | Prévia de conflitos | P | parcial (política global de ambiguidade) | parcial | sim | parcial | R6 |

### 4.2 Texturas e materiais (26)

| ID | Função | Rel. | Implementado | Integrado ao editor | Host | Aparelho | Pacote |
|---|---|---|---|---|---|---|---|
| T01 | Identidade de textura | N | parcial (R4: recurso Texture com GUID e reuso por conteúdo; sem lista de usuários) | parcial (seletor) | sim | parcial | R4 |
| T02 | Extração de embutidas | N | sim (R4: PNG/JPEG com bytes originais; KTX2 fica de fora) | sim (Arquivos > Texturas) | sim | sim | R4/R6 |
| T03 | Cor/dados | E/P | sim (pelo uso) | — | sim | sim | R4 |
| T04 | Origem do alpha | P | parcial (alpha da cor base) | — | sim | — | R4 |
| T05 | Bordas transparentes | N | — | — | — | — | R4 |
| T06 | Convenção de normal | P | parcial (escala; sem inversão Y) | — | sim | — | R4 |
| T07 | Resolução por recurso | P | parcial (limite automático + R3: textura máxima no perfil da fonte; ainda não por textura) | sim (aba Perfil) | sim | parcial (limite automático conferido na E3) | R2/R4 |
| T08 | Formato de plataforma | P | parcial (ASTC 4x4 para KTX2 com mips) | parcial (prévia) | sim | sim | R2/R4 |
| T09 | Compressão e qualidade | P | — | — | — | — | R4/R9 |
| T10 | Mipmaps | E/P | sim (gerados ou do KTX2) | — | sim | sim | R4 |
| T11 | Inspeção de mip | N | sim (R4 §8: cadeia gerada do arquivo, nível e dimensões) | sim (seletor > Ver) | sim | sim (nível 5 de 10 · 32×32) | R4 |
| T12 | Filtro e repetição | E/P | sim (sampler do arquivo; R4 §9: repetir/limitar/espelhar e linear/próximo por binding para textura do projeto) | sim (seletor de textura) | sim | parcial (limitar/próximo aplicados e revertidos; efeito visual não isolado) | R4 |
| T13 | Anisotropia | A | parcial (política global do renderer) | — | — | — | R4/R9 |
| T14 | Canais RGBA | N | sim (R4 §8: RGBA, R, G, B, A) | sim (seletor > Ver) | sim | sim (Vermelho) | R4 |
| T15 | Zoom e fundo | N | parcial (R4 §8: zoom 1–8× só no centro, sem arrastar; fundo xadrez/preto/branco) | sim (seletor > Ver) | sim | sim (4×, preto) | R4 |
| T16 | Memória e resolução efetiva | P | parcial (MB por importação; R2: pedidos × residentes do projeto) | parcial (console e log) | sim | parcial (log `[Residencia]` 339 MB) | R2/R4 |
| T17 | Streaming por orçamento | A | parcial (R2: teto agregado reduz mips na publicação; sem streaming por uso) | parcial | sim | — (projeto medido cabe no teto; redução não exercitada) | R2/R9 |
| T18 | Textura no material | N | parcial (R4: 4 bindings por instância e compartilhado; miniaturas no seletor; sem sampler/UV por binding) | sim (aba Material + seletor) | sim | parcial (instância) | R4 |
| T19 | Transformação por uso | P | sim (assada na importação quando o material concorda; R4 §9–10: canal de UV e deslocamento/escala/rotação por binding, lidos no shader) | sim (seletor de textura) | sim | sim (importação); sim (canal de UV); sim (transformação por binding: escala 2 e rotação 90° visíveis na carroceria, revertido; giro do mapa normal não isolado) | R4 |
| T20 | Empacotamento de canais | P | parcial (MR importado) | — | sim | — | R4 |
| T21 | Oclusão | N | parcial (R4: ORM no canal R do metal/rugosidade; textura própria segue declarada) | sim (prévia diz aplicada/não aplicada) | sim | parcial | R4 |
| T22 | Corte alfa e transparência | P | sim (R4: modo e corte por instância e material; fila segue o modo efetivo) | sim (aba Material) | sim | parcial (fluxo; efeito não observável no modelo) | R4 |
| T23 | Dupla face | P | sim (R4: culling real por material com estado dinâmico; sem ele, declarado) | sim (aba Material) | sim | parcial (culling ativo sem regressão; troca sem efeito visível no modelo) | R4 |
| T24 | Verniz | N | — | — | — | — | R9 |
| T25 | Cubemap, array e HDR | N | — (recusados) | — | sim (recusa) | — | R9 |
| T26 | Substituição em lote | N | — | — | — | — | R6 |

### 4.3 Malha e componente de renderização (32)

| ID | Função | Rel. | Implementado | Integrado ao editor | Host | Aparelho | Pacote |
|---|---|---|---|---|---|---|---|
| M01 | Referência de geometria | P | parcial (“Malha N”) | parcial | sim | sim | R5 |
| M02 | Reutilização de malha | E/P | sim | sim | sim | sim | R5 |
| M03 | Variante editável | N | — | — | — | — | R5/R7 |
| M04 | Prévia de malha | N | — | — | — | — | R5 |
| M05 | Estatísticas | P | parcial (contadores de importação) | parcial | sim | sim | R5 |
| M06 | Submalhas | E/P | sim (slots) | parcial | sim | sim | R5 |
| M07 | Materiais por superfície | E/P | sim | parcial | sim | sim | R4/R5 |
| M08 | Override de material | E/P | sim (instância/compartilhado) | parcial | sim | sim | R4/R5 |
| M09 | Camada de material | A | — | — | — | — | R9 |
| M10 | Wireframe | A | — | — | — | — | R5 |
| M11 | Normais/tangentes | N | — | — | — | — | R5 |
| M12 | UV por canal | P | — (dado existe, sem visualizador) | — | — | — | R5 |
| M13 | Cor de vértice | P | parcial (COLOR_0 importado) | — | — | — | R5 |
| M14 | Bounds | P | parcial | — | sim | — | R5/R7 |
| M15 | Margem de culling | A | — | — | — | — | R7/R8 |
| M16 | Modos de sombra | P | parcial (projetar sombra liga/desliga) | sim | sim | sim | R7/R9 |
| M17 | Recepção de sombra | A | — | — | — | — | R9 |
| M18 | Participação em GI | A | — | — | — | — | R9 |
| M19 | Lightmap e densidade | A | — | — | — | — | R9 |
| M20 | Probes | A | — | — | — | — | R9 |
| M21 | Vetores de movimento | A | — | — | — | — | R8/R9 |
| M22 | Camadas de renderização | A | parcial (camada do objeto; consumo gráfico a auditar) | parcial | sim | sim | R7/R9 |
| M23 | Oclusão por instância | P/A | parcial (culling HZB global) | — | — | — | R7 |
| M24 | LOD atribuído | P | parcial (seleção runtime do pacote de mapa) | — | sim | — | R7 |
| M25 | Transição de LOD | P/A | parcial (histerese do pacote) | — | — | — | R7 |
| M26 | Faixa de visibilidade | A | — | — | — | — | R7/R9 |
| M27 | Colisão visível | P | parcial | — | — | — | R5/R7 |
| M28 | Pontos de anexação | A | — | — | — | — | R7 |
| M29 | Esqueleto associado | N | — | — | — | — | R8 |
| M30 | Pesos de deformação | N | — | — | — | — | R8 |
| M31 | Morph targets | N | — | — | — | — | R8 |
| M32 | Bake de pose | N | — | — | — | — | R8 |

### 4.4 Sistemas dependentes de malhas (18)

| ID | Função | Rel. | Implementado | Integrado ao editor | Host | Aparelho | Pacote |
|---|---|---|---|---|---|---|---|
| D01 | Ajuste de primitiva física | P | parcial (slot 0) | parcial | — | — | R7 |
| D02 | Casco convexo | A | — | — | — | — | R7 |
| D03 | Decomposição convexa | A | — | — | — | — | R7 |
| D04 | Colisão triangular | P/A | parcial (colisão estática do pacote de mapa) | — | sim | — | R7 |
| D05 | Preparação de colisão | A | — | — | — | — | R7 |
| D06 | Navmesh | A | — | — | — | — | R7/R9 |
| D07 | Occluder derivado | A | — | — | — | — | R7/R9 |
| D08 | Instanciamento em massa | P/A | parcial (renderer instanciado) | — | — | — | R7 |
| D09 | Partículas | A | — | — | — | — | R9 |
| D10 | Geometria por arrays | A | — | — | — | — | R7/R9 |
| D11 | Edição topológica | A | — | — | — | — | R7/R9 |
| D12 | Construção por superfície | A | — | — | — | — | R7/R9 |
| D13 | Geometria imediata | A | — | — | — | — | R9 |
| D14 | CSG | A | — | — | — | — | R9 |
| D15 | Tecido/deformação física | A | — | — | — | — | R9 |
| D16 | Simplificação | N | — (meshopt vendorizado só decodifica) | — | — | — | R7 |
| D17 | UV derivada | A | — | — | — | — | R9 |
| D18 | Exportação de derivados | A | — | — | — | — | R10 |

### 4.5 Gerenciamento e experiência (12)

| ID | Função | Rel. | Implementado | Integrado ao editor | Host | Aparelho | Pacote |
|---|---|---|---|---|---|---|---|
| W01 | Importador no dock | N | sim (R3: contexto de Propriedades, janela modal removida) | sim | sim | — | R3 |
| W02 | Navegação de subassets | P | parcial (vínculo → recurso; sem breadcrumbs) | parcial | — | — | R3/R5 |
| W03 | Layout persistente | P/A | parcial (preferências do IDE) | parcial | — | — | R3 |
| W04 | Histórico de recurso | N | — (material compartilhado fora do Desfazer) | — | — | — | R4/R6 |
| W05 | Dependências e usuários | P | parcial (contagem de usuários antes de apagar) | parcial | sim | — | R6 |
| W06 | Relocalizar fonte | P | parcial (diagnóstico de fonte ausente) | — | — | — | R6 |
| W07 | Excluir/substituir | P | parcial | parcial | sim | — | R6 |
| W08 | Biblioteca visual | P | parcial (lista de arquivos) | parcial | — | sim | R3/R6 |
| W09 | Console contextual | P | parcial | parcial | sim | sim | R3/R6 |
| W10 | Cancelamento e retomada | P | parcial (importação entre etapas; R1: abertura em worker cancelável, publicação única com reversão) | parcial | sim | parcial (abertura medida; cancelamento no meio não conferido) | R1/R3 |
| W11 | Customização por capacidade | P/A | parcial | parcial | — | — | R3/R9 |
| W12 | Integridade transacional | P | parcial (journal; `.deps` fora) | parcial | sim | sim | R6 |

Contagem: 24 + 26 + 32 + 18 + 12 = **112**.

## 5. Como este checklist é mantido

Cada pacote que tocar um ID atualiza a linha no mesmo commit, com o documento do pacote como evidência. Uma linha só passa a **sim** em “Aparelho” com captura ou log do aparelho referenciados. Nenhuma coluna é deduzida das outras.
