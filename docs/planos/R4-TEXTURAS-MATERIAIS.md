# R4 — texturas do projeto e texturas nos materiais (primeira fatia)

15/09/2026 · branch `codex/gameplay-runtime` · pedido: seção R4 do [relatório](RELATORIO-IMPORTACAO-TEXTURAS-MALHAS-2026-09-14.md) · contrato definido em [R3 §6](R3-DOCK-PERFIS-IMPORTACAO.md).

## 1. O que existe agora

| Antes | Agora |
|---|---|
| Textura só existia dentro da fonte importada; material do projeto só tinha fatores | Imagens embutidas de um GLB viram **recursos `Texture`** em `Texturas/<fonte>/`, com os bytes originais (sem recodificar) |
| Trocar textura era impossível; só fatores escalares por slot | Cada slot tem **4 bindings** (cor base, normal, metal/rugosidade, emissão) trocáveis por **instância** ou no **material do projeto** |
| `MaterialAsset` v1 (fatores) | `MaterialAsset` **v2**: fatores + textura de cada binding por identidade; v1 continua abrindo |
| `MeshRenderer` v3 | `MeshRenderer` **v4**: textura de cada binding por slot; v1–v3 continuam abrindo |
| Variante de pipeline fixada pelo material da fonte | A variante acompanha o binding trocado (ligar/tirar normal, MR ou emissão muda a flag) |

## 2. Contrato

- **Identidade**: um recurso `Texture` é o próprio arquivo PNG/JPEG registrado (GUID, caminho, SHA-256 do conteúdo; proveniência em `importerParameters`). Extrair de novo a mesma imagem reaproveita o registro existente pelo conteúdo.
- **Valor de binding persistido**: `-` herda, `none` sem textura, ou o GUID da textura do projeto.
- **Resolução por binding**: instância → material compartilhado do slot → material da fonte. É independente dos fatores escalares: trocar só a textura não liga a substituição escalar, e vice-versa.
- **Espaço de cor pelo uso**: cor base e emissão em sRGB, normal e metal/rugosidade em linear. A mesma imagem usada nos dois gera duas texturas publicadas.
- **Publicação**: as texturas do projeto usadas por slots e materiais entram na biblioteca publicada depois das texturas das fontes. O renderer soma a base das texturas do pacote, como faz com os materiais importados. Uma troca que usa uma textura ainda não publicada custa uma republicação da biblioteca; publicação incremental é trabalho de R2 que ainda não foi feito. Trocas entre texturas já publicadas não republicam.
- **Residência**: texturas do projeto passam pelo mesmo teto de dimensão do aparelho e pelo orçamento agregado de R2.
- **Estado de publicação não é persistido**: `MaterialParameters::textures` guarda índices resolvidos a cada extração. Componentes e materiais guardam só identidades (`withoutResolvedTextures`).
- **Reimportação**: bindings são por identidade e atravessam a reconciliação. A consolidação de peças legadas copia as texturas do slot, e textura trocada conta como dado local.

## 3. Interface

- **Arquivos → GLB selecionado**: novo botão **Texturas** ao lado de Instanciar e Reimportar. Extrai as imagens e diz quantas foram criadas, quantas já existiam e quantas ficaram de fora (KTX2 não tem arquivo próprio neste perfil).
- **Propriedades → Malha → aba Material**: as quatro linhas "Mapa: cor / normal / metal/rug. / emissão" vêm antes dos números. Cada linha mostra a textura efetiva no alcance em edição e de onde ela vem ("da fonte", "do material do projeto", "só esta instância").
- **Seletor de textura** (toque na linha): Herdar, Sem textura, ou uma textura do projeto (nome, dimensões e caminho). Vale para o alcance escolhido (Esta instância / Compartilhado). No alcance da instância, passa pelo histórico.

## 4. Estado

| Parte | Implementado | Integrado ao editor | Host | Aparelho |
|---|---|---|---|---|
| Extração de imagens embutidas (T02) | sim | sim (Arquivos) | sim (`r4_textures_extract_from_source_and_resolve_per_instance_and_shared_scope`) | sim (19 PNG do Ford, bytes originais, 19 registros) |
| Identidade de textura (T01) | parcial (registro, reuso por conteúdo; sem lista de usuários) | parcial | sim | parcial (registro e seletor com nome, dimensões e caminho) |
| Textura no material, instância e compartilhado (T18) | sim | sim (aba Material + seletor) | sim (idem + `r4_material_tab_lists_texture_bindings_and_the_picker_changes_the_instance`) | parcial: instância conferida (trocar, salvar, reabrir, Herdar); compartilhado só no host |
| Formatos v2/v4 com leitura das versões antigas | sim | sim | sim (`r4_material_v2_and_mesh_renderer_v4_round_trip_and_read_older_versions`, `mesh_component_v1_archive_still_loads_and_gains_identity_on_save`) | sim (cena v4 salva e reaberta com a troca; revertida sem resíduo) |
| Variante de pipeline pelo binding trocado | sim | sim (renderer) | parcial (flags testadas; pipeline só no aparelho) | parcial (cor base conferida; normal/MR/emissão não trocados no aparelho) |

Suíte do host: 903/905 (as duas falhas antigas de R0). APK conferido: `F6EB4BB9…9638`. Evidência: `docs/validacao/evidencias/r4-texturas-20260915/`.

Medido no aparelho:
- trocar para uma textura ainda não publicada custou **uma republicação** (122 → 123 texturas); Herdar não republicou;
- reabrir um projeto cuja cena usa textura do projeto custa **uma segunda publicação** depois da cena (`cena_ms`=3242 contra 29 sem troca), como previsto na seção 2;
- as imagens do GLB do Ford não têm nome no arquivo e viram `imagem-N.png`; o seletor mostra dimensões e caminho para distinguir.

Visto no aparelho e **corrigido depois**: o cartão do vínculo com a fonte dizia "igual à fonte" com uma textura trocada nesta instância, porque a contagem de substituições do vínculo (M08.2) não olhava material nenhum. Agora `ImportOverrideMaterial` conta qualquer material local do objeto (valores, material do projeto ou textura trocada em algum slot). O menu do vínculo ganhou "Reverter material à fonte", que desfaz os três em todos os slots pelo histórico. O teste `m08e2_legacy_parts_become_slots_only_when_nothing_is_lost` passou a exigir exatamente esse bit para a peça legada que tem material local, em vez de zero. Conferido no host (`r4_textures_extract_from_source_and_resolve_per_instance_and_shared_scope`, 903/905); **ainda não conferido no aparelho**.

Estado do projeto de validação: as 19 texturas extraídas ficaram em `Texturas/` e no registro; a troca na cena foi revertida com Herdar e a cena salva não cita a textura.

## 7. Segunda fatia: reabertura, usos, oclusão, alfa e dupla face

| Parte | O que faz | Host | Aparelho |
|---|---|---|---|
| Reabertura com uma publicação | `anticipateSceneTextures` lê as texturas do projeto usadas pela cena salva antes da publicação da reabertura; elas sobem junto com as fontes | sim (`r4_reopen_publishes_scene_textures_once_and_warns_before_deleting_a_used_texture`) | — (a cena de validação foi revertida sem textura do projeto) |
| Reimportação preserva alcances | textura na instância e no material compartilhado atravessam a reimportação com fonte alterada | sim (`r4_reimport_keeps_texture_overrides_in_instance_and_shared_material`) | — |
| Usos de textura (T01) | contagem por objeto e por material; aviso antes de apagar; apagar devolve o binding à textura da fonte; status "Textura do projeto · dimensões · N usos" em Arquivos e usos no seletor | sim | — |
| Oclusão ORM (T21) | oclusão entra quando é a mesma textura e o mesmo UV do metal/rugosidade, sem transformação e com força 1 (canal R); atenua luz ambiente e reflexo do ambiente; trocar o mapa metal/rugosidade tira a oclusão da fonte; os outros casos seguem declarados na prévia com o motivo | sim (`m091_glb_textures_map_to_slots_with_color_space_uv_and_sampler`) | parcial (shader publicado sem regressão visual; efeito não isolado em captura) |
| Modo de alfa e corte (T22) | Herdar / Opaco / Recorte / Transparente e corte por instância ou material do projeto; a fila de cada desenho segue o modo efetivo | sim (`r4_alpha_mode_cutoff_and_sides_persist_resolve_and_reach_renderer_flags`, `r4_material_tab_edits_alpha_mode_cutoff_and_sides_per_instance`) | parcial (fluxo, alcance e vínculo conferidos; efeito visual não observável na carroceria opaca) |
| Dupla face real (T23) | glTF sem `doubleSided` marca `MapMaterialCullBackFaces`; o renderer descarta a face de trás por material com `VK_EXT_extended_dynamic_state` quando o aparelho confirma a feature; primitivas e pacotes antigos seguem sem culling; Herdar / Uma face / Duas faces por instância ou material; sem culling dinâmico a linha diz "sem efeito neste aparelho" e não recebe toque | sim | parcial ("culling por material: sim"; cena inteira sem peça do avesso; troca de faces sem efeito visível de fora) |

Formatos: `MeshRenderer` v5 e `MaterialAsset` v3 (alfa, faces, corte), com leitura das versões anteriores. Cache de derivados: schema 2 e revisão do importador 3 (oclusão e flag de culling mudam a saída). Suíte do host: 907/909 (as duas falhas antigas). APK conferido: `A5582C59…BBEC`. Evidência: `docs/validacao/evidencias/r4-superficie-20260915/`.

Limites desta fatia, ditos explicitamente:
- o caminho indireto em lote aplica material por lote; o culling por desenho vale no caminho por desenho, que é o das cenas do editor;
- sombras seguem sem culling;
- um modo de alfa diferente do da fonte usa o pipeline genérico da família quando a variante especializada não foi criada para o pacote (mesmo resultado, sem a especialização de features).

## 8. Terceira fatia: miniaturas e visualizador de textura

| Parte | O que faz | Host | Aparelho |
|---|---|---|---|
| Atlas de prévia | atlas RGBA8 1024×1024 composto na CPU pela sessão (`TexturePreviewAtlas`): grade de 50 miniaturas de 96 px e uma região de 512×512 para o visualizador; redução por média de caixa com proporção preservada; enviado ao renderer da interface só quando muda, como terceiro sampler do descritor da interface (`VulkanUiRenderer::setPreviewAtlas`, imagem nova a cada envio) | sim (`r4_texture_preview_atlas_fits_thumbnails_and_isolates_channels`) | sim (ver conferência) |
| Tipo de instância `Preview` | `UiDrawList::addPreviewImage` leva o recorte em texels no próprio comando; o fragmento amostra o atlas de prévia com o tamanho em `outputFlags.zw`; mesma conversão sRGB dos ícones | sim (`r4_builder_turns_a_preview_image_into_a_preview_instance_with_its_texels`) | sim (ver conferência) |
| Miniaturas no seletor (T18) | cada linha do seletor de textura mostra a miniatura, nome, dimensões/usos e o botão **Ver**; as miniaturas nascem uma por atualização enquanto o seletor está aberto e só são refeitas quando o conteúdo do arquivo muda | sim (`r4_texture_thumbnails_are_generated_once_and_viewer_walks_mips_and_channels`) | sim (ver conferência) |
| Inspeção de mip (T11) | cadeia de mips gerada como na publicação (`buildMipChain`, sRGB); − / + percorrem os níveis com "Nível N de M · L×A" | sim | sim (ver conferência) |
| Canais (T14) | RGBA, Vermelho, Verde, Azul, Alfa; canal isolado em cinza opaco | sim | sim (ver conferência) |
| Zoom e fundo (T15) | zoom 1×/2×/4×/8× **no centro** da imagem (ampliação por vizinho, texel nítido); fundo xadrez, preto ou branco sob o alfa (só em RGBA) | sim | sim (ver conferência) |
| Dados | dimensões originais, níveis, MB com mips, usos, teto de residência quando a imagem passa do limite do aparelho, caminho | sim | sim (ver conferência) |

Suíte do host: 910/912 (as duas falhas antigas de R0). APK conferido: `FE2462D3…74A5`. Evidência: `docs/validacao/evidencias/r4-previa-20260915/`.

Conferência no aparelho (Ford, carroceria `Cortina:lod10_CaarPaint_0`, aba Material > Mapa: cor):
- o seletor mostra miniatura de cada uma das 19 texturas extraídas, com proporção certa (1024×2048 aparece estreita), nas páginas 1 e 2;
- **Ver** abre `imagem-1.png`: 1024×1024, 11 níveis, 5.3 MB com mips, 0 usos, caminho; xadrez visível onde o alfa é zero;
- canal **Vermelho**: amarelo vira branco e verde vira escuro, como esperado de R isolado;
- **+** cinco vezes chega a "Nível 5 de 10 · 32×32", com texel ampliado nítido;
- **−** volta ao nível 0; **Zoom 4× no centro** e **Fundo Preto** aplicados;
- **<** volta ao seletor e fecha sem alterar a cena (nenhuma troca de textura foi feita).

Limites desta fatia, ditos explicitamente:
- o zoom é sempre centrado; **não há arrastar para mover** a região ampliada;
- a prévia mostra o nível da cadeia gerada a partir do arquivo original, não a textura residente na GPU depois do teto de dimensão e do orçamento (o teto aparece escrito nos dados);
- miniaturas cobrem as 50 primeiras texturas do projeto; as seguintes aparecem sem miniatura (com o quadro vazio);
- ASTC/KTX2 da fonte não entram no visualizador: texturas do projeto são PNG/JPEG por contrato (seção 2).

## 9. Quarta fatia: canal de UV, repetição e filtro por binding

| Parte | O que faz | Host | Aparelho |
|---|---|---|---|
| Dado persistido | `MaterialSampling{uvSet, wrap, filter}` por binding (0 herda) na instância (`MeshRenderer` **v6**, token `uvf` por binding de cada slot) e no material do projeto (`MaterialAsset` **v4**); versões anteriores continuam abrindo | sim (`r4_sampling_uv_set_wrap_and_filter_resolve_publish_variants_and_travel_to_the_project_material`, `mesh_component_v1_archive_still_loads_and_gains_identity_on_save`) | sim (ver conferência) |
| Resolução | campo a campo: instância → material compartilhado → fonte, independente da textura e dos fatores | sim | sim (ver conferência) |
| Canal de UV (T19) | UV 0 / UV 1 reescreve os dois bits do binding em `textureCoordinates`, o campo que o shader já lê (`selectedUv`); vale também com a textura da fonte | sim | sim (ver conferência) |
| Repetição e filtro (T12) | Repetir / Limitar / Espelhar e Linear / Próximo viram as flags de sampler da textura do projeto publicada; a biblioteca passa a ser por (textura, espaço de cor, sampler) e só as combinações em uso sobem; trocar o sampler custa uma publicação | sim | sim (ver conferência) |
| Interface | seletor de textura do binding com três botões (UV, Rep., Filtro); herdar mostra o que vale por baixo; repetição e filtro não recebem toque quando o binding usa a textura da fonte; a linha do binding na aba Material diz "amostragem própria" | parcial (resolução e rótulos; toque só no aparelho) | sim (ver conferência) |
| Vínculo com a fonte | amostragem local conta como material local, "Reverter material à fonte" limpa; criar material do projeto leva a amostragem e limpa a da instância | sim | sim (ver conferência) |

Suíte do host: 911/913 (as duas falhas antigas de R0). Commit `7ed4a6a0`, APK conferido: `B6A005F6…492C`. Evidência: `docs/validacao/evidencias/r4-amostragem-20260915/`.

Conferência no aparelho (Ford, carroceria `Cortina:lod10_CaarPaint_0`, Material > Mapa: cor, esta instância):
- com a textura da fonte, o seletor mostra "UV: Herdar", "Rep.: Herdar" e "Filtro: Herdar"; repetição e filtro aparecem apagados e o aviso "valem só com textura do projeto" fica visível;
- **UV** trocado para UV 0: o cartão do vínculo passa de "igual à fonte" para "1 alteração local";
- escolhida a `imagem-1.png` (textura do projeto): a linha diz "só esta instância · amostragem própria", o seletor mostra "1 uso" e repetição e filtro passam a receber toque;
- **Rep.: Limitar** e **Filtro: Próximo** aplicados, sem erro de publicação;
- **Reverter material à fonte** devolveu textura, UV, repetição e filtro: "igual à fonte", carroceria original, `imagem-1.png` com 0 usos; cena salva nesse estado.

Não observado no aparelho: diferença visual de limitar/próximo na carroceria (a UV do Ford fica dentro de 0–1 e a câmera está longe demais para ver o filtro).

Limites desta fatia, ditos explicitamente:
- **transformação de UV por binding** (escala, deslocamento, rotação) ficou para a fatia seguinte e foi feita na **seção 10**;
- repetição e filtro de uma **textura da fonte** não são editáveis (o sampler dela sobe com o pacote); para trocar, extraia a textura e use a do projeto;
- trocar o canal de UV do mapa normal não recalcula tangentes: elas seguem o conjunto de UV em que foram geradas na importação;
- duas amostragens da mesma imagem em uso ao mesmo tempo duplicam a cadeia de mips na CPU e na GPU.

## 10. Quinta fatia: transformação de UV por binding

| Parte | O que faz | Host | Aparelho |
|---|---|---|---|
| Dado persistido | `MaterialSampling` ganha deslocamento (U, V), escala (U, V) e rotação em graus, com a identidade herdando; instância em `MeshRenderer` **v7** (cinco números por binding de cada slot) e material do projeto em `MaterialAsset` **v5**; versões anteriores continuam abrindo | sim (`r4_uv_transform_per_binding_resolves_reaches_the_shader_entry_and_persists`, `mesh_component_v1_archive_still_loads_and_gains_identity_on_save`) | sim (ver conferência) |
| Resolução | a transformação é uma unidade: a da instância inteira, senão a do material compartilhado; resolvida em linhas 2x3 T·R·S (as do KHR_texture_transform) | sim | sim (ver conferência) |
| Caminho até a GPU | o push constant e o `GpuMeshInstance` seguem nos 128 bytes: a tabela de transformações é um storage buffer em set=1, binding=16 (sempre presente, 1024 entradas, só desenhos que transformam ocupam entrada) e o índice+1 vai nos bits 1..31 de `materialFlags.z` (o bit 0 continua sendo a saída sRGB); atualizada ao publicar a cena e no caminho de Play | sim (entrada da tabela) | sim (ver conferência) |
| Shader | `selectedUv` e a UV da cobertura aplicam a transformação (recorte e cor no mesmo texel); o mapa normal gira o detalhe pela rotação transposta, sem a escala | shaders validados e reproduzíveis | sim (ver conferência) |
| Interface | no seletor de textura do binding, abaixo de Herdar e Sem textura: passos de ± para deslocamento U/V (0,05), escala U/V (0,1) e rotação (15°), e "Zerar transformação"; vale com qualquer textura, inclusive a da fonte | sim (a opção Sem textura continua alcançável: `r4_material_tab_lists_texture_bindings_and_the_picker_changes_the_instance`) | sim (ver conferência) |
| Vínculo com a fonte | transformação local conta como material local; reverter e criar material do projeto seguem a amostragem | sim | sim (ver conferência) |

Suíte do host: 912/914 (as duas falhas antigas de R0). Commit `3b8791be`, APK conferido: `615B171D…F85B`. Evidência: `docs/validacao/evidencias/r4-transformacao-uv-20260915/`.

Conferência no aparelho (Ford, carroceria `Cortina:lod10_CaarPaint_0`, Material > Mapa: cor, esta instância):
- o projeto abre e a cena desenha normalmente com o binding 16 novo no conjunto de descritores do mapa (sem erro de pipeline);
- o seletor mostra Herdar e Sem textura no topo e, abaixo, UV/Rep./Filtro e os passos "Desl. U/V", "Esc. U/V", "Rot." e "Zerar transformação";
- escala U e V em 2.00 e rotação 90° pelos passos: os rótulos seguem e o vínculo passa a "1 alteração local"; com a textura da fonte (quase branca) não há diferença visível;
- com a `imagem-1.png` na cor base, a mesma câmera mostra outro desenho na carroceria do que sem transformação (faixas amarelas, verdes e brancas ao longo da lateral em vez do fundo preto com o logotipo na traseira): **a transformação chega ao shader**;
- "Reverter material à fonte" volta a "igual à fonte" e à carroceria original; cena salva nesse estado.

Não conferido no aparelho: o giro do detalhe do mapa normal (a normal da fonte não mostra relevo legível a essa distância) e o recorte alfa transformado (a carroceria é opaca).

Limites desta fatia, ditos explicitamente:
- **caminho indireto em lote** não recebe a transformação (como o resto do material por instância); as cenas do editor usam o caminho por desenho;
- **sombras** usam o shader próprio e ignoram a transformação: o recorte alfa da sombra continua na UV da fonte;
- acima de **1024 desenhos** com transformação, os excedentes ficam sem ela, com um aviso único no log;
- rotação com escala não uniforme aproxima o referencial do mapa normal (a rotação é correta; a escala desigual inclina a base).

## 11. Auditoria contra o relatório e sexta fatia: extensão de material por desenho

Uma auditoria linha a linha contra a seção 7.2 e a seção 8 do relatório mostrou que o R4 ainda não estava completo. Esta fatia fecha T04, T06, T20, T21 e o tratamento de sombras de T22.

| Parte | O que faz | Host | Aparelho |
|---|---|---|---|
| Oclusão (T21) | origem por slot: da fonte / sem / canal do metal-rugosidade / **textura própria do projeto**; **força** 0–1 e canal; a textura própria é dado linear e usa o conjunto de UV e a amostragem do metal/rugosidade | sim (`r4_occlusion_channels_normal_and_alpha_source_resolve_reach_the_extension_and_persist`) | pendente |
| Canais (T20) | de qual canal da imagem vêm rugosidade, metal e oclusão (R, G, B, A) | sim | pendente |
| Normal (T06) | inversão Y (convenção DirectX), além da intensidade que já existia | sim (`r4_material_tab_isolates_a_channel_and_flips_normal_y_per_instance`) | pendente |
| Origem do alfa (T04) | alfa da cor base, ignorar (opaco) ou luminância da cor base; vale no sombreamento, na cobertura e na sombra | sim | pendente |
| Isolar na prévia (T21) | mostra só oclusão, rugosidade, metal, alfa ou normal no slot em edição; transitório, nunca salvo, nunca em Play | sim | pendente |
| Sombra pelo material efetivo (T22) | a sombra recorta pela textura, corte, conjunto de UV, transformação de UV e origem do alfa do material trocado (antes: sempre da fonte); `ShadowPushConstants` em 128 bytes | shaders validados | pendente |
| GPU | a entrada da tabela por desenho (set=1, binding=16) passou de 8 para 12 linhas vec4; o padrão da fonte é resolvido na CPU; a textura de oclusão própria só existe com descritores bindless | sim (entrada) | pendente |
| Formatos | `MeshRenderer` **v8** e `MaterialAsset` **v6** (canais, oclusão, textura de oclusão, normal, alfa); versões anteriores abrem | sim | pendente |
| Interface | aba Material com sete linhas novas (Oclusão, Textura de oclusão, Força, Canais, Mapa normal, Origem do alfa, Isolar na prévia); o seletor de textura serve à oclusão | sim | pendente |

Limites: a oclusão própria não existe no caminho sem bindless (quatro samplers fixos por material); o caminho indireto em lote não recebe a extensão.

## 12. Sétima fatia: perfil por textura e anisotropia aplicada

| Parte | O que faz | Host | Aparelho |
|---|---|---|---|
| Perfil por textura | `.astra/textures/<guid>.profile`: interpretação (pelo uso / cor sRGB / dado linear, T03), tamanho máximo (T07, nunca acima do teto do projeto), mipmaps sim/não (T10), bordas sem halo (T05: dilatação da cor sob alfa zero antes dos mips, sem mudar o alfa) e anisotropia desligável (T13) | sim (`r4_texture_profile_round_trips_fails_closed_and_dilates_edges`, `r4_texture_profile_changes_interpretation_mips_anisotropy_and_is_read_back_on_reopen`) | sim (Mipmaps: não aplicado e revertido na `imagem-1.png`) |
| Aplicar | grava o perfil, invalida a decodificação daquela textura e republica; lido de novo na reabertura | sim | sim |
| Residência por textura (T08/T16) | "Na GPU: L×A · níveis · MB" depois do perfil e do orçamento, por combinação de uso | sim | sim ("Na GPU: não usada" numa textura sem usos) |
| **Anisotropia (T13) — defeito encontrado** | a política calculava a anisotropia e a registrava no log, mas **nenhum sampler a usava** e, pior, o dispositivo Vulkan era criado **sem pedir a feature `samplerAnisotropy`**: o limite ficava 1 e o log dizia "reduzido por capability" em qualquer aparelho. Agora a feature é pedida quando a GPU suporta e a anisotropia chega aos samplers do pacote e da autoria, salvo quando o perfil da textura a desliga | sim | antes da correção: "aniso=1.0 … reduzido por capability"; depois (APK `1AD975F7…FCEA`): "aniso=8.0" na política e "anisotropia de material=8.0" nos samplers |

Limite: texturas embutidas nas fontes seguem o perfil de importação da fonte (R3); para escolher por textura, extraia a imagem.

## 13. Oitava fatia: textura em Propriedades e gerenciador de texturas

| Parte | O que faz | Host | Aparelho |
|---|---|---|---|
| Textura em Propriedades (T01/T11/T14/T15/T18) | tocar numa textura em Arquivos mostra em Propriedades o visualizador (canal, mip, zoom, fundo), o perfil, a residência e **os usuários** (objetos da cena, selecionáveis, e materiais do projeto); mudar a seleção da cena devolve Propriedades ao objeto | sim (`r4_texture_manager_filters_searches_and_the_inspector_lists_users`) | sim ("Usuários (0)", perfil e visualizador da `imagem-1.png`) |
| Gerenciador (seção 6 do relatório) | tocar na pasta Texturas (ou numa subpasta) abre a grade com miniaturas paginada | sim | sim (19 texturas, 3 páginas) |
| Busca | por nome ou pasta, sem diferenciar maiúsculas, pelo teclado do aparelho ("Buscar textura") | sim | pendente |
| Filtros | Todas, Não usadas, Ausentes (arquivo ilegível), Alteradas (conteúdo diferente do hash registrado), Sem alfa (nenhum texel com alfa abaixo de 255, analisado pela miniatura) e Acima do teto | sim | sim (Não usadas: 19; Sem alfa: 16, sem a `imagem-1.png`, que tem transparência) |

Limites: "Sem alfa" só conhece as texturas cuja miniatura já foi gerada (50 primeiras); substituir fonte, relocalizar e duplicar variante são W05/W07 do R6.

## 14. Nona fatia: publicação incremental da biblioteca (critério do R2 e do R4)

| Parte | O que faz | Host | Aparelho |
|---|---|---|---|
| Texturas | uma textura que chega como o mesmo objeto compartilhado da publicação anterior mantém a imagem (e o sampler, se a anisotropia não mudou) na GPU; só as novas sobem | sim (`r2_r4_authoring_library_reuses_only_the_same_shared_textures`) | sim: abertura "122 publicadas (0 reaproveitadas, 122 enviadas); geometria enviada"; mudar o perfil de uma textura sem usos: "**122 reaproveitadas, 0 enviadas; geometria reaproveitada**" |
| Geometria | vértices e índices com os mesmos bytes mantêm os buffers | — (Vulkan) | sim (idem) |
| Falha fechada | nada da biblioteca antiga muda de dono antes de todas as partes novas existirem | — | — |

Limites: buffers de instância, filas de desenho e descritores ainda são recriados a cada publicação (custo de CPU e de alocação pequena); uma textura reduzida pelo orçamento agregado vira cópia nova e sobe de novo.

## 5. Fora desta fatia, dito explicitamente

- ~~Miniaturas, visualizador de canais e mips, zoom/fundo~~: feitos na seção 8 (zoom só no centro).
- **Sampler, canal de UV e transformação por binding** (T12/T19): ainda não são campos do material. O push constant do material já ocupa os 128 bytes garantidos pelo Vulkan; trocar UV e transformação por binding pede um buffer de materiais por desenho, que é trabalho de renderer ainda não feito.
- **Oclusão em textura própria** (fora do canal R do metal/rugosidade) ou com força diferente de 1: continua declarada como não aplicada (ver seção 7).
- **Caminho indireto em lote do renderer**: aplica o material por lote, não por desenho; valia antes para os fatores e vale igual para as texturas. As cenas do editor usam o caminho por desenho.
- **Passes de cobertura e sombra** usam a textura de cor da fonte para o recorte alfa; trocar a cor base não muda o recorte.
- **Sem descritores bindless**: texturas importadas e do projeto não aparecem (limitação anterior, com aviso no log).
- **Lista de usuários da textura** e **apagar/substituir textura** com prévia de impacto (W05/W07): R6.
- **Histórico da edição compartilhada** (W04): o material do projeto continua fora do Desfazer, como em E2.
