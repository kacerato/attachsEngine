# Retomada — 16/10/2026

Estado mais recente21/10: APK111 `.20261021.3` compilado/auditado e instalado,
SHA256 `e6f4114746d082a43c996485299864b14f47ecf246c2b56f5246181633888c7b`.
Editor30FPS e Play/Player50FPS implementados; ganho de navegação ainda não medido.
Aparelho conectado, porém bloqueado/apagado; pedido de desbloqueio pendente.
Build Android incremental final passou em1m22s após corrigir namespace da declaração.
Suíte host final de terreno está recompilando em um único job. Água, desempenho
em movimento e Player exportado continuam sem aceite. Engine já publicada em
`955458a6` nesta branch; correção subsequente de namespace em fechamento. As notas
abaixo são checkpoints históricos, não anulam este estado. Docs públicas seguintes
ainda não publicadas; autorização ADB excepcional segue aberta.

Checkpoint local21/10: [separação Viewport/Play/Player e desempenho](VIEWPORT-PLAY-PLAYER-2026-10-21.md) complementa [correntes, lava, chuva e brilho por altura](FLUXOS-LAVA-CHUVA-2026-10-20.md). **239/239 terreno/editor,46/46 política gráfica,312/312 GPU e44 shaders** passaram na revisão local; os112 Core/2 assets/106 capturas anteriores continuam históricos. APK111 `.20261021.1` instalado para QA privada, SHA256 `c16bf1d44e63842c51c14d9f7e1900a4a17b254222f0014f2c218cba9f6301e6`; ainda apresentou cerca de5 FPS em movimento. **Não aprovado em desempenho/água visual.** Candidato intermediário `.20261021.2` compilado, sem instalação; revisão seguinte em preparação com presets editoriais próprios, relógio de prévia opcional, exclusão de preferências da exportação, prova de camadas vazias e shader que acumula um material por vez. Não há instalação do candidato novo, push ou próximas Docs confirmados neste checkpoint. ADB sem aparelho na última consulta. Play/Player agora têm atualização de UI enxuta, com toque em Canvas real verificado e arquivo autoral intacto; presets do editor e gráficos do próximo Play/APK têm escopos explícitos. Captura executável revelou navegação sobrepondo o popover; ordem corrigida e testada em655×300/1280×720, recaptura em andamento. A/B móvel e exportação executável pendentes. A passagem ADB excepcional desta entrega segue aberta; depois das próximas Docs continuar sem ADB até terminar o plano. Originais intactos; catálogo82/156, atlas646;71 itens/28 linhas preservados.

Ajuste subsequente solicitado: autoria30FPS; Play/Player50FPS, sem seletor por enquanto.
53/53 política/cadência,6/6 retry/fence e10/10 seleção de pipelines passaram; shader real
312 modos com comparações extras de especialização/fallback passou em238,25s,44 famílias
SPIR-V válidas. Candidato privado `.20261021.3` em compilação com frame gate não bloqueante,
seleção conservadora de recursos e logs de idade do input. Ainda sem instalação/medição.
ADB foi reconectado ao endpoint pareado; testes só com foco da Astra. Não aceitarFPS ou
água antes do teste real. As contagens239/239 de editor acima são do checkpoint anterior
à última mudança de cadência/título; suíte/capturas finais e exportação seguem pendentes.

Publicação M13 confirmada em20/10: engine `78ff1825` na branch
`codex/terrain-voxel-generator`, AstraDocs `ce156ff9`, Vercel
`dpl_5YJ5ukrWPTuPkzqt1icC3nSbGoCf` READY/produção; HTML, Markdown,
feed e Atualizações HTTP200 com conteúdo novo em `astraengine.com.br`.
Evidência local `build/terrain-evidence/terrain-fx-domain-verification.json`.
O parágrafo seguinte registra o aceite anterior à publicação. Continuação
M14/chuva em integração local,310 cenários GPU aprovados em overlay;
não declarar UI/runtime/aparelho aceitos antes dos respectivos testes.
Usuário liberou ADB somente para esta próxima entrega antes das próximas docs;
depois continuar sem ADB até terminar o plano. Plano71 permanece aberto.

Aceite local mais recente: [M13 estático, pintura FX, umidade e poças](FX-ESTATICO-2026-10-20.md), com [revisão da UI](UI-FX-2026-10-20.md). Recipe9/SurfaceV4, pincel RGBA independente, barras/precisão, modos exclusivos, cursor sobre o relevo, preview vivo, Wetness/Puddle/porosidade/Snow clearing, cache/histórico e atlas640. **213/213 editor,103/103 Core,252 GPU,11/11 pipeline incluindo2048² íntegro,26/26 Android sintático+1/1 visual final,44 shaders e42 capturas finais**. Corrigidos rolagem/cancelamento, reabertura, gizmo/câmera, rodapé aplicado e estado Prévia. Sem commit/push/Atualizações próprios confirmados ainda; sem APK/ADB. M13 integral/primeiro bloco/71 itens continuam abertos. Streams/Lava têm overlay renderer com284 GPU aprovados, assets originais e UI congelados em staging; chuva/specularFade têm302 GPU aprovados em overlay separado. Dinâmica/scene/controller/altura/renderer por instância seguem em staging, com aceite completo e ligação Android pendentes. CSV conserva71/28 linhas, todas as propriedades e colunas históricas. Os checkpoints seguintes são históricos.

Continuação mais recente: [M11–M12, neve, qualidade autoral e busca](NEVE-QUALIDADE-BUSCA-2026-10-20.md). Off/Simple/Rich, fontes originais, RGB/RG/GA, iluminação/transmissão fina, barras, filtro por finalidade e miniaturas reais integrados. **158/158 editor,72/72 Core,238 modos GPU,36/36 publicação,11/11 residência focada**,21/21 Android sintático+4/4 na revisão final,44 shaders,4+1 testes de assets;130 capturas base+28 refinadas. Catálogo82/152 e atlas636, originais intactos. **Publicação confirmada: engine `a3d27687`, Docs `4b5264a9`, Vercel READY e domínio HTML/Markdown/feed/Atualizações HTTP200; sem APK/ADB.** Os71 itens e M11/M12 continuam abertos para autoridades/poças/tracks/adjacentes e aceite Android real. Próxima dependência: [M13–M15](PROXIMO-M13-M15-2026-10-20.md), com arquitetura/UI versionadas e código apenas staging: fundação9/9 e SurfaceV4 4/4 isolados passaram; Recipe9/persistência/UI/renderer ainda não aceitos. Os registros abaixo são históricos.

Continuação mais recente: [M09–M10, estratos, curvas assinadas e mapas globais](ESTRATOS-GLOBAIS-2026-10-20.md). Modelo/serialização/GLB/cache/shader/editor integrados; domínio real, fontes originais e ícones executáveis. **132/132 editor**, **64/64 Core**, **199 modos GPU**, **33/33 regressões de curvas existentes**, **17/17 Android sintático**, 44 shaders e quatro testes Python de assets/preparação passaram. **82 capturas executáveis** de 41 estados, com revisão de área útil, fontes vazias e curva vazia/desligada. Navegação antiga Ruído/PBR/variantes também compactada; paginação materiais/texturas corrigida. Catálogo: 82 materiais/148 texturas, originais intactos. Publicação confirmada: engine `03b8f10a` na branch `codex/terrain-voxel-generator`; AstraDocs `2236df2`, Vercel READY (`dpl_Akh1U17TxzJMkBgghKToWYKgw34i`), guia HTML/Markdown/feed/Atualizações HTTP200 com conteúdo novo no domínio. Build Docs: 2439 páginas e 595395 links sem erro; revisão desktop/390 px sem overflow. Sem APK/ADB. Próximas dependências: [M11–M12 neve](PROXIMO-M11-M12-2026-10-20.md) e [qualidade/residência/busca](PROXIMO-QUALIDADE-CATALOGO-2026-10-20.md), ambas ainda propostas/staging. M09/M10 conservam capacidades adjacentes pendentes; primeiro bloco e plano dos 71 continuam abertos. Os registros abaixo são históricos.

Continuação mais recente: [M05–M08, ruído, anti-tile, resampling e variantes](RUIDO-ANTI-TILE-2026-10-20.md). Consumidores/UI integrados com fontes reais, barras e valores precisos; três normais globais/locais, variantes de sete mapas, qualidade1/2/3 contribuições, identidade estável, gradientes explícitos e transições por distância/altura. **113/113 host**, **116 modos GPU**, **16/16 Android sintático**, shaders validados e16 capturas executáveis. Novo ícone integrado sem alterar IDs; geradores evitam regravações idênticas. Sem APK/ADB. Publicação confirmada: engine `2750745c` na branch `codex/terrain-voxel-generator`; AstraDocs `da8ab3e`, Vercel READY (`dpl_Th4EQGKp8Ut21NKzZUXbZ4Lt32Bi`), HTML/Markdown/feed/Atualizações com HTTP200 no domínio conferidos em20/10. Próxima dependência: M09–M10; quatro canais pintados/32 autorados e plano71 continuam abertos.

Continuação atual: [catálogo e materiais PBR](MATERIAIS-PBR-2026-10-20.md). Catálogo original de 82 materiais selecionável por canal, miniaturas reais no atlas, sete mapas PBR, mistura por altura e UV/triplanar local/mundial com controles por barras. Escala/offset em metros; normais e constantes neutras corrigidas. Reaplicar fonte existente republica a biblioteca canônica antes dos slots dos chunks. PNGs/fontes/propriedades persistem no histórico e em sessão nova; nenhum original foi reduzido.

**102/102 cenários host**, **30 modos GPU reais**, **16/16 unidades Android ARM64 sintáticas**; seis capturas executáveis em 1280×720 e 655×300. 146 PNGs runtime (302.147.871 bytes) conferidos por hash/CRC/dimensões. Atlas/enum sincronizados com 479 entradas em 2048×2452, com alfa opaco somente na prévia UI derivada. Duas asserções condicionais de symlink indisponíveis no Windows. APK 110 permanece anterior; sem novo APK ou ADB.

M01 conserva 32 camadas, mas o controle/render atuais usam quatro; M02–M04 avançaram com consumidores, sujeitos ao aceite completo. M05–M08 continuam sem consumidores completos de ruído/anti-tile/resampling/clusters. Próxima implementação: [especificação M05–M08](PROXIMO-M05-M08-2026-10-20.md). O plano integral de 71 itens e todas as propriedades comprometidas foram preservados; bloco 1 e meta global permanecem abertos.

Publicação PBR confirmada: engine `ebd31aec` na branch; AstraDocs `5f1029e`, Vercel READY e HTML/Markdown/feed/Atualizações servidos por astraengine.com.br. Continuação M05–M08 publicada conforme checkpoint acima; aceite Android continua pendente. As revisões abaixo são históricas.

Checkpoint anterior: [pintura independente na GPU](PINTURA-GPU-2026-10-20.md). Consumidor por fragmento, GLB/cache/publicação e prévia do mapa integrados ao relevo; 128 células como padrão de criação e barra 8–512. 96/96 cenários integrados, 18/18 de texturas e oito modos gráficos na GPU do PC passaram; painel host conferido. 14/14 unidades Android passaram na compilação sintática; sem APK novo e sem ADB. Catálogo/PBR extensível é a próxima dependência real; P11, bloco 1 e os 71 itens continuam abertos.

Código funcional `47b4cd6b` publicado; AstraDocs `2f3efbb` publicado, Vercel READY e conteúdo servido pelo domínio conferido. A conferência adicional dos pixels da revisão visível após cold reload passou. Próximo avanço: catálogo original/PBR integrado ao editor e renderer, preservando todos os controles do plano.

Continuação de 20/10: [mapas autorais, recuperação e catálogo](CONTINUACAO-2026-10-20.md). 90/90 cenários integrados host; mapas autorais com slider/entrada precisa, checkpoints/deltas da receita e reconstrução de GLB integrados; vizinhança e materiais avançaram no backend/contrato. Shader extensível, grupos no editor, geometria/GC global, vegetação e grafos continuam pendentes. ADB suspenso pelo proprietário até concluir a implementação e a conferência local; APK 110 não contém estas mudanças.

Checkpoint funcional `23fe8dde` publicado. AstraDocs `f35821b` publicado e Vercel READY, com guia próprio de 20/10 e nota nova. Próxima integração necessária: consumir os mapas de controle por fragmento e os materiais extensíveis na publicação/cache/renderer, preservando canais, variantes, identidade e qualidade dos assets. Modelos isolados não encerram esses itens.

Última revisão: [sliders e compartilhamento de texturas](INTERFACE-MEMORIA-2026-10-19.md), código `9fc2f719`. 64/64 cenários de terreno e 18/18 de publicação passaram no host. APK 110 instalado: criação plana, sliders, escultura ao vivo, Undo/Redo e reabertura com LOD conferidos no POCO F7. Biblioteca do projeto antigo: 127,27→43,25 MiB. D09, primeiro bloco e plano de 71 itens continuam abertos.

Última continuação: [base plana, escultura suave e reinício, APK 109](ESCULTURA-2026-10-19.md).
59/59 testes; APK 109 instalado. Android: fluxo completo conferido no 108;
correção dos padrões ao reiniciar/reeditar confirmada no 109.
O relato do 107 abaixo permanece como evidência da revisão anterior.

Continuação de 19/10: [aplicação da prévia e integração Android](APLICACAO-2026-10-19.md).
ADB reautorizado pelo proprietário para validar depois da implementação.
As restrições de ADB nos checkpoints abaixo são históricas.
Revisão atual: 58/58 cenários host; Astra Terreno 107 compilado, auditado e
instalado por atualização. ADB desbloqueado: prévia branca corrigida e validada
no projeto antigo; escultura/aplicação/histórico testados no novo. Permanecem
saturação da biblioteca antiga e teto CPU ao aplicar (D09). Veja o registro
acima para evidências e limites; primeiro bloco e plano completo ainda abertos.

Atualização de 19/10: [integração do outro PC e validação no Windows](PROGRESSO-2026-10-19.md).
Engine publicada em `62547f30`: 56/56 testes integrados, 4/4 do planejador de
buffers, UI de LOD capturada em 655 e 1280 pixels. Este é o ponto de retomada
da integração anterior. O relato de pausa abaixo é histórico; o trabalho foi retomado
por solicitação do proprietário. Sem ADB; primeiro bloco e plano completo abertos.

Atualização de 17/10: [fontes locais confirmadas, caminhos regionais e LOD autoral](PROGRESSO-2026-10-17.md).
Esse registro mais recente complementa o checkpoint histórico abaixo; o plano integral permanece aberto.

Pausa solicitada pelo proprietário para encerrar a sessão. Nada foi mesclado na main da engine. ADB continua suspenso.

## Escopo preservado

- Plano completo: `plano-implementacao.csv`, 71 itens, sem cortes de funcionalidades/propriedades.
- Primeiro bloco: 28 itens (D01–D09/D12, P01–P08/P10–P11, M01–M08).
- `bloco1-aceite.csv` repete integralmente as propriedades comprometidas e separa implementação verificada de pendências.
- `bloco1-execucao.md` registra implementação, provas e limites. O bloco 1 e os 71 itens NÃO estão concluídos.

## Último ponto validado

- `279d174`: 48/48 cenários host, incluindo indexação sem perda da prévia; `ea85ce0`: pincéis, caminhos, UI, catálogo de assets e publicação por faixas.
- 4/4 testes Vulkan Windows com readback; 6 testes de contratos de materiais; seis unidades Android passaram em checagem de compilação antes da última correção de caps.
- Assets: 82 entradas, 146 texturas únicas; originais preservados. Oito referências de máscara ausentes declaradas. Biblioteca ainda não integrada ao renderer PBR completo.
- Último APK concluído: `build/artifacts/Astra-Terreno-86-arm64.apk`. Nenhum APK novo desta rodada está pronto.

## Alterações de checkpoint ainda não validadas

- `terrain_asset.cpp`: correção de caps retos/quadrados para não receber arredondamento de segmentos vizinhos; considera pontos terminais coincidentes.
- `terrain_path_caps_clip_geometry_and_paint_at_terminal_planes`: cenário novo em `test_terrain.cpp`; precisa executar. Não incluir esse cenário nos 48/48 já verificados.
- Verificadores persistentes em `tools/verify-terrain-apk.py` e `tools/verify-terrain-apk-identity.ps1`; ampliados para receita 5 e vinte ícones, ainda aguardam APK novo.

## Próxima execução

1. Compilar/executar `aether_terrain_tests` em `build/editor-host`, `-j1`; verificar caps e depois suíte completa. Corrigir falhas antes de declarar validação.
2. Retomar Android, usando o cache existente: `cmake --build android/app/.cxx/RelWithDebInfo/425y142t/arm64-v8a --target aether_android aether_transform -j1`. Build interrompido durante as 76 etapas, log `build/terrain-evidence/block1-android-native-build.log`.
3. Empacotar com Gradle (um worker, sem paralelismo), pacote `dev.aether.editor.terrain`, label `Astra Terreno`, código novo 88 e nome `0.3.1-dev.terrain.20261016.1`; validar assinatura, identidade, ZIP, atlas e igualdade do ELF. Não usar ADB.
4. Docs: checkout `build/AstraDocs`, checkpoint separado no GitHub. Guia de 16/10 e notas não publicados no site. Build anterior passou com 1406 páginas conferidas e zero erros de links; revisão desktop/390 px feita. Houve edição posterior do texto para 48 testes/indexação; regenerar cobertura/build/links, revisar, publicar main das Docs e confirmar Vercel READY.
5. Continuar as lacunas de `bloco1-aceite.csv`: materiais/controle/PBR, interface altura-volume, LOD, GC/checkpoints, vizinhança e resoluções independentes. Não tratar catálogo como integração concluída.

## Disco

- 1.932 intermediários Android retirados do índice e ignorados (`2fca165`); arquivos locais preservados para reaproveitar compilação.
- Não repetir a exclusão física de caches bloqueada pela revisão automática. Nenhuma limpeza física ocorreu nessas tentativas.
- Usar um compilador por vez. Última investigação detectou download em segundo plano do instalador Visual Studio e paginação do Windows nos picos; não encerrar processos externos ao trabalho.

Checkpoint das Docs publicado: https://github.com/kacerato/AstraDocs/tree/codex/terrain-block1-checkpoint-20261016 — commit c6f5590. Cobertura de Atualizações validada; publicação em produção continua pendente.

## Manutenção local de 16/10/2026 — base b20d275d

Branch de trabalho: `codex/terrain-voxel-generator`, checkout separado em
`C:/Users/donod/Downloads/atchengine-terrain`. O checkout
`C:/Users/donod/Downloads/atchengine` continua na branch
`codex/ui-play-performance-apk-export`, com suas alterações preservadas.
Não houve merge, push, novo APK ou uso de ADB nesta manutenção.

### Onde o plano parou

A matriz mantém 71 itens: Digger 12, Path Painter 12, MicroSplat 18,
Advanced Terrain Grass 13 e MapMagic 16. O aceite do primeiro bloco mantém
28 itens: 6 marcados como implementados no host, 15 parciais e 7 abertos.
Essas classificações são do checkpoint; os testes abaixo não promovem itens.

Há implementação nativa parcial de escultura, caminhos, persistência e
superfície de quatro albedos. Não são apenas nomes ou planos. Entretanto,
nenhum dos cinco plugins está integralmente incorporado: vegetação ATG e
grafo/execução MapMagic continuam ausentes como sistemas integrados.
`native/editor/editor_terrain.cpp` chama `updateTerrain`; a receita persiste
os caminhos, e `native/rhi/shaders/terrain_surface.glsl` consome quatro
albedos com pesos normalizados. `clearGrass`, `clearTrees` e `showVegetation`
continuam dados persistidos sem consumidor de vegetação. A biblioteca de
82 entradas e 146 texturas é preparação de assets; não fecha o PBR M01–M08.

### Iteração focada e pressão de disco

21 cenários existentes de geometria/receita foram movidos, sem alteração
das asserções, para `tests/native/test_terrain_core.cpp`. Permanecem também
na suíte `aether_terrain_tests`: os 49 nomes/corpos do checkpoint foram
conferidos, sem perda nem duplicação. O alvo `aether_terrain_core_tests`
compila o código real `terrain_asset.cpp` com três unidades de tradução;
dispensa editor, física, renderer, assets e suas bibliotecas neste recorte.

```powershell
cmake -S native -B build/terrain-core -G Ninja -DAETHER_TERRAIN_CORE_ONLY=ON -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_CXX_FLAGS_DEBUG=-O0 -g1"
.\tools\build-editor.ps1 -Target TerrainCore -BuildDirectory build/terrain-core -RunTests
# Para uma regressão específica:
.\tools\build-editor.ps1 -Target TerrainCore -BuildDirectory build/terrain-core -RunTests -Filter terrain_path_caps
# Integração, somente em um cache completo configurado para ESTE checkout:
.\tools\build-editor.ps1 -Target Terrain -RunTests
```

O script usa um job por padrão, conserva o cache incremental, recusa cache
de outro checkout e mantém um lock comum do Git durante build e execução.
O lock serializa invocações desta versão do script entre worktrees; comandos
Ninja/Gradle diretos e versões antigas do script não participam desse lock.
`-Jobs` continua disponível. `-RunTests` é explícito: compilar não executa
testes automaticamente. A configuração CORE_ONLY deve usar seu próprio
diretório; não aplicá-la por cima do cache completo de editor/Android.

Checkout seletivo local: fontes, testes, ferramentas, documentos e JSON do
catálogo. Os demais assets/payloads LFS não foram baixados para esta rodada;
um build completo exige materializá-los. Não compartilhar objetos compilados
com a outra branch. A correção de indentação no `case TerrainTool::Hole`
permite GCC com `-Werror` sem alterar seu comportamento.

Referências de mecanismo: [CMake --build/--parallel](https://cmake.org/cmake/help/latest/manual/cmake.1.html#build-a-project)
e [Ninja job pools no CMake](https://cmake.org/cmake/help/latest/prop_gbl/JOB_POOLS.html).
Limitar compiladores e dependências reduz trabalho; não garante um teto de
ocupação do disco contra outros processos ou paginação do Windows.

### Evidência desta manutenção

- `maintenance-caps.log`: 1/1 passou, incluindo caps Round/Flat/Square e
  endpoints coincidentes. A correção pendente foi validada neste recorte host.
- `maintenance-core-tests.log`: 21/21 passaram; build incremental sem trabalho
  mais execução em 5,394 s. Executável de 2,12 MiB. Não é comparação percentual
  contra a suíte completa nem medição Android.
- `maintenance-material-contracts.log`: 6/6 passaram após materializar o JSON
  do catálogo que faltava no checkout seletivo. Valida contratos de dados,
  não imagens/PBR no renderer.
- Recusa de cache de outra branch e contenção do lock comum verificadas;
  chamada posterior do build respondeu `ninja: no work to do`.
- `git diff --check` passou. A suíte integrada de 49 cenários não foi recompilada
  nem executada nesta manutenção; APK/Android/aceite visual permanecem pendentes.

Logs locais em `build/terrain-evidence/`. O registro anterior de 48/48 permanece
histórico; não somar essa rodada aos 21 testes como se fossem casos distintos.

### Limpeza: identificada, mas bloqueada

Foram identificadas 84 pastas temporárias dos testes antigos, 664 arquivos,
50,38 MiB, sob o `build/` do checkout original. Os nomes correspondem aos
prefixos gerados pelos fixtures de collision-recipe, convex-bake, gui-context,
gui-storage, handles e occlusion. A revisão automática recusou a exclusão
com `blocked by policy`; nenhum desses arquivos foi apagado.
Lista: `build/terrain-evidence/cleanup-candidates.txt`; inventário detalhado:
`build/terrain-evidence/maintenance-cleanup-inventory.json` no checkout de terreno.
Caches CMake/Android, LFS, APKs, fontes e projetos foram preservados.

Próxima retomada funcional: suíte integrada do terreno no checkout correto,
depois as lacunas de materiais/PBR, interface altura-volume, LOD,
checkpoints/GC, vizinhança e resoluções independentes. O cache Android citado
no checkpoint anterior não existe neste checkout; não reutilizar o cache de
outra branch apenas para evitar configurá-lo. ADB continua suspenso.
