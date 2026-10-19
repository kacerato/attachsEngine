# Retomada — 16/10/2026

Em andamento: [sliders e compartilhamento de texturas](INTERFACE-MEMORIA-2026-10-19.md). 64/64 cenários de terreno e 18/18 de publicação passaram no host. APK 110 em compilação; não confundir com validação Android. D09, primeiro bloco e plano de 71 itens continuam abertos.

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
