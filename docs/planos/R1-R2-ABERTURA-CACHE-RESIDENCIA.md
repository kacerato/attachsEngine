# R1 + R2 — abertura sem tela preta, cache de derivados e orçamento agregado

14/09/2026 · branch `codex/gameplay-runtime` · base [R0](R0-BASE-CONTRATOS-CHECKLIST.md) · pedido: seções R1 e R2 do [relatório](RELATORIO-IMPORTACAO-TEXTURAS-MALHAS-2026-09-14.md).

## 1. O problema medido

Linha de base no aparelho (`docs/validacao/evidencias/r1-abertura-20260914/linha-de-base-adb.log`): com 8 fontes registradas, a janela do editor ficava **64,3 s** sem quadro. `reimportProjectSources` rodava no loop nativo antes do primeiro quadro, fonte por fonte, e cada fonte republicava a biblioteca acumulada com espera de GPU (8 reconstruções). O shell Java somava 1,8 s de espera fixa com uma barra que fingia porcentagem.

## 2. R1 — o que mudou

| Parte | Antes | Agora |
|---|---|---|
| Preparo das fontes | loop nativo, antes do primeiro quadro | `std::async` (`startProjectReopen`): lê bytes, hash, derivado ou importador |
| Publicação | 1 por fonte, acumulada | `EditorSession::reopenSources`: todas preparadas entram num candidato e publicam **uma vez**; falha volta ao estado anterior |
| Cena salva | carregada logo | `recoverProjectScene` só depois da publicação; `editorSavePath` vazio até lá mantém o autosave desligado |
| Interação durante a abertura | não havia quadro | toques bloqueados; importar recusado com aviso |
| Barra de estado | — | “Abrindo projeto · recurso N de M · etapa arquivo”, reafirmada a cada quadro (`setWorkStatus`), sem porcentagem |
| Fonte ausente ou recusada | parava a abertura | vira aviso no console; o resto abre |
| Sair durante a abertura | — | `destroyRequested` aciona o cancelamento do worker (checado entre fontes e dentro do importador) |
| Shell Java | 1,8 s fixos + barra de porcentagem inventada | pede o editor imediatamente; barra vira varredura indeterminada; splash de marca mantém 2,2 s |
| Fundo da janela | padrão do tema | `android:windowBackground=@color/astra_void` |

Medição no aparelho, APK `C436D3E4…17B7` (antes da correção da barra de estado e do shell Java): primeiro quadro do editor **1.216 ms** depois da janela; projeto aberto em 69,6 s (preparo 66,0 s, publicação única 3,5 s, cena 27 ms). O tempo total não caiu, porque o preparo ainda refazia o importador inteiro, e isso é o alvo de R2.

Defeito encontrado nas capturas dessa medição: a publicação do código escrevia “Código aplicado ao projeto” por cima da mensagem de abertura. Corrigido com a etapa reafirmada por quadro.

## 3. R2 — cache de derivados

`native/resources/import_cache.{h,cpp}`. O `GltfImport` preparado (geometria, materiais, árvore, texturas decodificadas com mips, contadores e notas da prévia) é gravado em `.astra/cache/imports/<chave>.aic`.

- **Chave**: SHA-256 de schema do arquivo, revisão do importador, versões dos codecs (Draco 1.5.7, meshoptimizer v1.2, basis_universal v2_50, stb_image 2.30), hash do conteúdo da fonte e **todos** os limites de importação, inclusive ASTC ou RGBA8. Um `.gltf` chega empacotado, então as dependências entram no hash da fonte.
- **Falha fechada**: magia no início e no fim, chave repetida no arquivo, leitura com limites, pais de nó anteriores ao filho, texturas `valid()`, índices de desenho, material e textura dentro do alcance, passo de vértice. Qualquer divergência descarta o arquivo inteiro e o importador roda.
- **Nunca é fonte de verdade**: apagar `.astra/cache/` custa só tempo. Ao fim de uma abertura não cancelada, derivados que nenhuma fonte atual usa são removidos.
- **Integração**: o worker de abertura tenta o derivado antes do importador; em falta, importa e grava (`EditorImportTransaction::write`, substituição atômica). Log por fonte: `cache=acerto|falta`, tempos de leitura, preparo e gravação; resumo `derivados_reaproveitados=N`.
- **Revisão do importador** (`ImportCacheImporterRevision`): subir quando a saída de `importGlb` mudar para os mesmos bytes e limites.

Ainda não feito: gravar o derivado já na importação interativa (hoje a primeira reabertura grava); invalidar por dependência de textura separada da malha (o critério “mudar só a textura não reconstrói todas as malhas” depende de texturas como recursos próprios, R4).

## 4. R2 — orçamento agregado de residência de texturas

`native/resources/texture_budget.{h,cpp}`. O limite por arquivo (256 MB) não enxerga o projeto. `applyTextureBudget` roda em `EditorSession::publishAndAdopt`, sobre a biblioteca inteira:

- soma as cadeias de mips, contando uma vez a textura que aparece mais de uma vez;
- enquanto passar do teto, remove o nível mais alto da maior textura redutível (mais de um nível e menor lado/2 ≥ piso de 256 px);
- as reduzidas são **cópias**: as fontes e o derivado em cache guardam as cadeias completas, então subir o teto e republicar devolve a resolução sem reimportar;
- relatório (`textureResidency()`): pedidos, residentes, teto, texturas reduzidas, níveis removidos, se coube. Mudança com efeito vai para o console (aviso quando nem o piso cabe) e o shell registra `[Residencia]` ao abrir.

Teto padrão: 1 GiB (`setImportTextureBudget`). Não medido ainda contra a memória real do aparelho; o número é um ponto de partida documentado, não uma calibração.

## 5. Estado

| Parte | Implementado | Integrado ao editor | Host | Aparelho |
|---|---|---|---|---|
| Worker de abertura + publicação única | sim | sim | sim (`r1_project_reopen_publishes_all_sources_once_and_matches_the_sequential_path`) | sim (1.216 ms até o primeiro quadro) |
| Etapa real na barra de estado | sim | sim | — (shell) | sim (`fria-8s-etapa-real-adb.png`: “recurso 2 de 8 · gravando derivado …”) |
| Shell Java sem espera fixa nem porcentagem | sim | sim | — | sim (toque → `AetherActivity` exibida em ~4 s, antes ~6 s com a espera fixa) |
| Cache de derivados | sim | sim (abertura) | sim (`r2_import_cache_round_trips_the_prepared_model_and_refuses_stale_or_corrupt_files`) | sim (fria × quente abaixo) |
| Orçamento agregado | sim | sim (publicação) | sim (`r2_texture_budget_reduces_residency_across_sources_without_touching_originals`, `m091_publication_offsets_texture_indices_per_source`) | parcial: relatório conferido (339 MB pedidos, teto 1 GiB, nenhuma redução necessária); a redução em si só foi exercitada no host |

Suíte do host: 898/900 (as duas falhas antigas de R0). APK com R2: `53A52678…9FCA`.

## 6. Medição no aparelho: fria × quente

15/09/2026, mesmo projeto `M08Recursos0913k` e as mesmas 8 fontes. Evidência: `docs/validacao/evidencias/r2-cache-20260915/`.

| | Sem cache (R1, 14/09) | Fria: grava derivados | Quente: lê derivados |
|---|---|---|---|
| Primeiro quadro do editor | 1.216 ms | 1.178 ms | 1.155 ms |
| Preparo das 8 fontes | 66,0 s | 96,3 s | **14,9 s** |
| Publicação única | 3,5 s | 3,1 s | 2,9 s |
| Projeto aberto | 69,6 s | 99,4 s | **17,8 s** |
| Derivados reaproveitados | — | 0 | 8 |
| Porsche (90 MB) | 44,7 s | 54,9 s + 10,8 s gravando | 8,9 s |

- A cena recuperada é a mesma nas duas aberturas (`fria-aberto-adb.png` e `quente-aberto-adb.png`: mesmo conteúdo e o mesmo tamanho de PNG).
- A abertura fria custa ~30 s a mais que sem cache: ~19,5 s gravando derivados, e o restante é preparo mais lento no mesmo aparelho. Ela só acontece na primeira abertura depois de mudar fonte, limites ou versão do importador.
- **Disco:** os 8 derivados ocupam ~485 MB (Porsche: 188 MB para uma fonte de 90 MB, porque as texturas vão descomprimidas com mips). É o custo do cache e precisa aparecer para o usuário (limpeza e tamanho do cache no dock, R3).
- Na quente, parte do tempo por fonte é ler a fonte inteira e calcular o SHA-256 dela. Nesta medição o log ainda somava isso em `leitura_derivado_ms`; o log agora separa `leitura_fonte_hash_ms`. Validar o conteúdo sem reler a fonte (tamanho e data com o hash do registro) é a próxima redução possível, e exige cuidado para não aceitar uma fonte trocada.

Fora deste bloco, e dito explicitamente: carregamento sob demanda por cena, separação entre handles autorais e posições de buffer, publicação incremental com épocas, cache de derivados GPU e contabilização de geometria e staging no orçamento continuam como trabalho de R2 que não foi feito.

**Atualização (15/09/2026, R4 §14):** a publicação da biblioteca de autoria ficou incremental para texturas e geometria. Uma textura que chega como o mesmo objeto compartilhado mantém a imagem na GPU, e vértices e índices com os mesmos bytes mantêm os buffers. No aparelho, mudar o perfil de uma textura sem usos republicou com "122 reaproveitadas, 0 enviadas; geometria reaproveitada", contra "122 enviadas" na abertura. Continuam de fora:
- buffers de instância, filas de desenho e descritores, ainda recriados a cada publicação;
- texturas reduzidas pelo orçamento, que viram cópia nova e sobem de novo;
- épocas explícitas;
- carregamento sob demanda;
- cache de derivados GPU;
- geometria e staging no orçamento.
