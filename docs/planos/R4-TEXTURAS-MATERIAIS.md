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
| Extração de imagens embutidas (T02) | sim | sim (Arquivos) | sim (`r4_textures_extract_from_source_and_resolve_per_instance_and_shared_scope`) | pendente |
| Identidade de textura (T01) | parcial (registro, reuso por conteúdo; sem lista de usuários) | parcial | sim | pendente |
| Textura no material, instância e compartilhado (T18) | sim | sim (aba Material + seletor) | sim (idem + `r4_material_tab_lists_texture_bindings_and_the_picker_changes_the_instance`) | pendente |
| Formatos v2/v4 com leitura das versões antigas | sim | sim | sim (`r4_material_v2_and_mesh_renderer_v4_round_trip_and_read_older_versions`, `mesh_component_v1_archive_still_loads_and_gains_identity_on_save`) | pendente |
| Variante de pipeline pelo binding trocado | sim | sim (renderer) | parcial (flags testadas; pipeline só no aparelho) | pendente |

Suíte do host: 903/905 (as duas falhas antigas de R0).

## 5. Fora desta fatia, dito explicitamente

- **Miniaturas** no seletor (hoje nome, dimensões e caminho), **visualizador de canais e mips** (T11/T14/T15) e **zoom/fundo**.
- **Sampler, canal de UV e transformação por binding** (T12/T19), **alpha e dupla face** (T22/T23), **oclusão** (T21): ainda não são campos do material.
- **Caminho indireto em lote do renderer**: aplica o material por lote, não por desenho; valia antes para os fatores e vale igual para as texturas. As cenas do editor usam o caminho por desenho.
- **Passes de cobertura e sombra** usam a textura de cor da fonte para o recorte alfa; trocar a cor base não muda o recorte.
- **Sem descritores bindless**: texturas importadas e do projeto não aparecem (limitação anterior, com aviso no log).
- **Lista de usuários da textura** e **apagar/substituir textura** com prévia de impacto (W05/W07): R6.
- **Histórico da edição compartilhada** (W04): o material do projeto continua fora do Desfazer, como em E2.
