# R3 — importador em Propriedades e perfis de importação (primeira fatia)

15/09/2026 · branch `codex/gameplay-runtime` · pedido: seção R3 do [relatório](RELATORIO-IMPORTACAO-TEXTURAS-MALHAS-2026-09-14.md) · base [R0](R0-BASE-CONTRATOS-CHECKLIST.md), [R1/R2](R1-R2-ABERTURA-CACHE-RESIDENCIA.md).

## 1. O que mudou

| Antes | Agora |
|---|---|
| Janela modal centralizada (`buildProjectDialogs`) cobrindo a tela com bloqueio total do toque | Contexto do painel **Propriedades** (`buildImportDock`); viewport, hierarquia e arquivos continuam respondendo |
| Um texto único (`importSummary`) paginado | Abas com dados estruturados: **Resumo**, **Estrutura** (nós, profundidade, malhas por nó), **Texturas** (dimensão, ASTC/RGBA8, cor/dados, MB, usos), **Perfil** |
| Sem opções de importação | Perfil com **escala** (I01) e **textura máxima** (T07), com efeito real no importador |
| Nada guardado sobre como a fonte foi importada | Perfil gravado com a fonte e usado na reimportação e na reabertura; padrão do projeto reutilizável (I19) |

O painel respeita o layout do IDE: ocupa o espaço de Propriedades já existente, usa as mesmas cores, raios e tipos, e não introduz ícone novo. No layout compacto e vindo do workspace de código, abrir a importação traz Propriedades à vista.

## 2. Perfil de importação

`native/resources/import_profile.{h,cpp}`, schema 1.

| Campo | Passos | Consumidor |
|---|---|---|
| `scale` | ×0,001 · ×0,01 · ×0,1 · ×0,5 · ×1 · ×2 · ×10 · ×100 · ×1000 | `GltfImportLimits::rootScale`: S·L na pose local das raízes; filhos herdam, geometria fica no espaço do nó |
| `maximumTextureDimension` | 256 · 512 · 1024 · 2048 px | `GltfImportLimits::maximumTextureDimension`, nunca acima do teto do aparelho |

- **Resolução**: `.astra/imports/<guid>.profile` (a fonte) → `.astra/import-default.profile` (projeto) → embutido (×1, 2048, igual ao comportamento anterior). Arquivo ausente ou inválido cai no próximo nível.
- **Falha fechada**: schema diferente, valor fora dos passos ou JSON inválido recusam o arquivo inteiro.
- **Cache (R2)**: escala e dimensão entram nos limites e, por eles, na chave do derivado; perfis diferentes nunca compartilham derivado.
- **Reabertura**: cada fonte reabre com os limites do próprio perfil.
- **Remoção**: apagar a fonte apaga o perfil junto com o mapa de nós.

Os passos são deliberadamente discretos: no toque, um número digitado erra fácil, e os casos reais são conversões de unidade. Conversão de eixos (I02) e hierarquia opcional (I03) ficaram fora: nenhum dos dois tem consumidor pronto (ver seção 5).

## 3. Ciclo de trabalho no painel

1. **Importar** ou **Reimportar** abre Propriedades com o perfil resolvido para aquele caminho.
2. O worker prepara com esse perfil; a prévia registra **com qual perfil** foi preparada.
3. Mudar escala ou textura máxima deixa o rascunho diferente da prévia. **Só recurso** e **Na cena** somem, e o painel diz “Perfil alterado: prepare de novo para publicar”.
4. **Preparar com este perfil** devolve os mesmos bytes ao worker (com o mesmo manifesto de dependências de `.gltf`) e chega uma prévia nova.
5. Publicar grava o perfil ao lado da fonte. **Salvar como padrão do projeto** grava o preset do projeto, sem publicar nada.
6. **Cancelar** descarta a prévia; o projeto não muda.

Ambiguidades de reimportação continuam exigindo escolha explícita (“Pela ordem” / “Como novos”) antes de publicar.

## 4. Estado

| Parte | Implementado | Integrado ao editor | Host | Aparelho |
|---|---|---|---|---|
| Painel em Propriedades, sem bloqueio do editor | sim | sim | sim (`r3_import_lives_in_properties_and_does_not_block_the_editor`) | sim (`painel-resumo-adb.png`) |
| Abas Resumo/Estrutura/Texturas com dados estruturados | sim | sim | sim (linhas e profundidade conferidas) | sim (`painel-estrutura-adb.png`, `painel-texturas-adb.png`) |
| Perfil: escala e textura máxima | sim | sim | sim (`r3_import_profile_scales_roots_persists_per_source_and_changes_the_cache_key`) | sim (×1 → ×2: tamanho 3,46 → 6,93; `perfil-escala-x2-adb.png`) |
| Perfil por fonte e padrão do projeto | sim | sim | sim | — (nada publicado no aparelho de propósito) |
| Nova preparação com o rascunho | sim | sim (shell) | parcial (pedido e bloqueio de publicação; o worker é do shell) | sim (publicação bloqueada com aviso, volta liberada após preparar; `perfil-preparado-adb.png`) |
| Cancelar sem alterar o projeto | sim | sim | sim | sim (`cancelado-adb.png`; nenhum `.profile` criado) |
| Reabertura com o perfil de cada fonte | sim | sim (shell) | — | sim (8 fontes, 0 falhas; derivados de chave antiga podados; `abertura-com-perfis-adb.log`) |

Suíte do host: 900/902 (as duas falhas antigas de R0). APK conferido: `2D7DFE39…3F03`. Evidência: `docs/validacao/evidencias/r3-painel-20260915/`.

Defeitos vistos no aparelho e corrigidos depois (APK `962B755E…3F7F`, host 900/902), **reconferidos no aparelho** (`correcao-caminho-caixa-adb.png`, `correcao-botao-diminuir-adb.png`, `correcao-escala-meio-adb.png`: ×0,5 com tamanho 1,73; cancelado sem gravar perfil; reabertura quente com perfis: 8/8 derivados, 17,1 s):
- o botão de diminuir a escala saía vazio, porque o glifo “−” (U+2212) não existe no atlas da fonte; agora usa “-”;
- o caminho da fonte usava o estilo de rótulo em maiúsculas, que esconde a caixa do nome do arquivo; agora usa o estilo de legenda.

A tentativa de reconferir abriu outro projeto: a ordem da lista de projetos mudou e o toque cego caiu em `InteriorG2`, que abriu `project.json` no editor de código sem edição (data de modificação do arquivo conferida: 11/09/2026, intacta). As capturas dessa tentativa foram descartadas. Daqui em diante, cada toque no aparelho é precedido por uma captura da tela.

## 5. Fora desta fatia, dito explicitamente

- **I02 conversão de eixos**: exige tratar poses, normais, tangentes e winding juntos. Uma opção sem isso seria um controle falso.
- **I03 / I04 hierarquia e inclusão por nó**: exigem filtro na IR e política para descendentes e referências (R3/R6).
- **I18 política por categoria** e **I24 conflitos por item**: dependem da reconciliação granular (R6).
- **W02 navegação de subassets**, **W03 layout persistente do painel** e **W09 console contextual**: próximas fatias do R3.
- **Prévia 3D** do modelo no painel: não existe ainda; as abas mostram saídas reais em texto.
- **Tamanho aproximado**: calculado pelas esferas dos desenhos levadas ao mundo, então é maior que a caixa real. Por isso aparece como “aprox.”.

## 6. Contrato de R4 definido junto (texturas e materiais como recursos)

Para que os controles de R3 não precisem ser descartados em R4:

1. **Identidade de textura (T01)**: cada imagem da fonte ganha GUID próprio no registro, com chave estável `fonte + índice de imagem + uso (cor/dados)`. O perfil de textura (dimensão, e depois compressão) passa a poder existir **por textura**, com o perfil da fonte como padrão herdado.
2. **Derivado de textura separado do de malha**: a chave de cache de uma textura é conteúdo da imagem + perfil de textura + codecs. Mudar só uma textura não reconstrói as malhas da fonte, que é o critério do relatório para R2/R4.
3. **Material do projeto referencia textura por GUID** (T18), nunca por índice do arquivo. O índice do arquivo fica só dentro do derivado da fonte.
4. **O painel de R3 é o lugar das texturas**: a aba Texturas de hoje (lista somente leitura) vira a navegação para o inspetor de textura (W02) sem mudar de lugar.
5. **Alcance**: edição de textura segue o mesmo modelo instância/compartilhado do material (E2), passando pelo histórico (W04).
