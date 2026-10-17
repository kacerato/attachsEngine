# Terreno: fontes locais, edição regional e LOD autoral

Branch exclusiva: `codex/terrain-voxel-generator`, checkout
`C:/Users/donod/Downloads/atchengine-terrain`. A outra branch da engine não foi
alterada. Este registro não fecha o bloco 1 nem os 71 itens dos cinco plugins.
Não houve ADB, APK, instalação ou aceitação Android nesta rodada.

## Fontes confirmadas

Os cinco arquivos originais estão em
`C:/Users/donod/Downloads/analise-plugins/analise-plugins`. A extração seletiva
fica em `build/terrain-reference-sources/`, ignorado pelo Git. Não copiar fontes
ou DLLs dos plugins para a documentação pública.

`extract-terrain-packages.py --sources-only` faz duas leituras sequenciais, sem
staging duplicado de texturas/previews. Processa oito subpacotes ATG um por vez
com spool temporário de até 8 MiB em memória. Assets omitidos são inventariados;
fontes/configurações acima de 16 MiB ficam explicitamente omitidas. Isso não é
extração integral de assets/manuais. As DLLs são apenas inspecionadas estaticamente.

Conferência inicial: 1.088 arquivos selecionados, 258,38 MiB, todos com hashes
iguais aos caminhos de `arquivos.csv`; nenhuma divergência ou entrada nova.
Depois foram extraídas as quatro DLLs do Path Painter. Não confundir arquivos
ou as 11.281 declarações do catálogo histórico com capacidades implementadas.

Path Painter `Engine/010/PathPainter.dll`: SHA-256
`e5260af3d34526070c8335102cae88a04ced814659c48ad7121c38c242190f4c`,
MVID `7c639190-cf63-4283-b579-3df9ea5e608e`. REA identificou Unity/Mono,
AnyCPU, CIL, .NET Framework 4.6 e atributo Dotfuscator. Metadados com 70 tipos,
474 campos e 961 métodos; a API `Painter` possui 48 campos e 92 métodos.
Observação estática: sem carregar a DLL, resolver Unity, decompilar C# ou executar.

Evidências REA:
- `ev_66dcc1ba615040fab9e591e7c79b5ce7183febe0975da520b05cf146ae48a9a1`
- `ev_619d5d0c9614b1b28011be8993d97c99e3ab7fec8c395310b04942fdf42f790b`

O retorno dos membros excedeu o transporte; a análise foi exportada sem repetir
a inspeção em `build/terrain-evidence/pathpainter-rea-20261017.json`
(2 registros, 3.537.339 bytes). Assinaturas não resolvidas permanecem explícitas.

## Implementação

**D07 / caminhos:** mover pontos, trocar material/canais, reordenar, desativar ou
remover caminhos deixa de invalidar a malha inteira. Diferenças do campo marcam
chunks e halos geométricos; footprints antigos e novos marcam pintura. O sufixo
reordenado é incluído para preservar sobrepintura. O campo dos caminhos ainda
é regenerado integralmente; a reutilização demonstrada é dos chunks de malha.
Arena GPU, crescimento/redução e sincronização/culling locais ficam pendentes.

**D08:** `AETERRAIN6` salva 1–4 níveis e erro relativo de 0,0001–0,1. Leitores
1–5 continuam disponíveis; o escritor novo emite 6, inclusive para desligar LOD.
Leitores antigos não aceitam essa receita. O importador GLB recupera a política
na importação, sem flag temporária de sessão. O meshoptimizer existente mantém
bordas e incorpora normais e quatro pesos de material na métrica para preservar
caminhos pintados. O nível original permanece para colisão; seleção e histerese
usam o orçamento em pixels existente no renderer.

Autoria: Forma → Detalhes → Detalhe à distância. Níveis e erro ficam lado a lado
no formato compacto. Um nível desativa geração e desabilita o campo de erro.
Aplicar gera os níveis; a prévia de escultura mantém a malha original.
Referência privada: Digger 6.0, `ChunkLODGroup.cs`, `Chunk.cs` e controles de
`DiggerMasterEditor.cs`. Referências oficiais:
[Godot 4.5 — Mesh LOD](https://docs.godotengine.org/en/4.5/tutorials/3d/mesh_lod.html)
e [Unity 6000.0 — Terrain.heightmapPixelError](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Terrain-heightmapPixelError.html).
Princípio adaptado: geração no importador, bordas protegidas e seleção por erro
projetado, reutilizando as estruturas da Astra.

## Evidência e limites

- `terrain-core-lod.log`: 23/23 cenários, incluindo os 21 anteriores. Não somar
  rodadas repetidas como casos distintos.
- `terrain-lod-attributes.log`: 1/1 após ampliar a métrica: persistência, redução,
  todas as arestas de borda, pintura estreita, seleção longe/perto e conversão
  de colisão original.
- Importador e ação de editor passaram compilação sintática GCC/C++20 com
  `-Wall -Wextra -Werror`. Não substitui ligação/execução da sessão integrada.
- `source-extraction-tests.log`: 2/2; gzip Unity, pathname após asset, subpacote
  e recusa de travessia de diretório.
- `aether_terrain_surface_preview` executa a superfície, roteador, fonte/atlas
  desta branch e instâncias reais, usando o rasterizador de teste existente.
  Capturas `lod-surface-1280.png` e `lod-surface-655.png`; os controles são
  alcançáveis pela rolagem. Sem cena de jogo, viewport 3D ou aparelho.
- Teste de importação completa conectado à suíte integrada, ainda não executada
  nesta rodada. D08 continua parcial até sessão/salvar/reabrir/renderer final
  e transições visuais serem conferidos.

Builds serializados com `Jobs=1` e lock comum entre worktrees. `TerrainSurface`
usa o cache `build/terrain-core`, sem recompilar a sessão inteira. Não se afirmou
um teto de ocupação do disco: faltaria medição contínua de todos os processos.

## Pendências preservadas

Interface altura-volume, GC/checkpoints, APIs runtime/navegação, vizinhos,
mapas independentes, biblioteca/PBR e módulos MicroSplat, vegetação ATG com
limpeza por caminhos, grafo/nós/tiles MapMagic e cenários de aceite continuam
obrigatórios. Controles de vegetação persistidos ainda não têm consumidor.
Encontrar os pacotes e avançar D07/D08 não fecha essas dependências.

## Complemento: importador real e publicação

Depois das verificações acima, `terrain-import.log` confirmou **3/3 cenários**
com `importGlb` e codecs reais. Os casos foram extraídos da suíte integrada,
sem duplicar nomes nem retirar a cobertura dela. Incluem os três modos,
decodificação dos quatro albedos, pesos/bindings e LOD autoral na importação
fria. A receita com um nível também vence um perfil genérico que pede quatro.
Isso encerra a pendência do importador isolado, não a sessão/salvar/reabrir nem
o renderer final. Os registros anteriores continuam como evidência histórica.

O alvo opcional `TerrainImport` usa as definições existentes de Draco,
meshoptimizer, Basis e ASTC, movidas sem alteração para
`native/cmake/ImportCodecs.cmake`. Não há codecs falsos ou dependência do editor,
Jolt ou APK. O primeiro build compila os codecs uma vez, com um job; a iteração
de geometria continua no alvo `TerrainCore`, que dispensa esses codecs.

```powershell
cmake -S native -B build/terrain-core -DAETHER_TERRAIN_CORE_ONLY=ON -DAETHER_TERRAIN_IMPORT_TESTS=ON
.\tools\build-editor.ps1 -Target TerrainImport -BuildDirectory build/terrain-core -RunTests
```

Nota pública de desenvolvimento `2026-10-17-terreno-lod-autoral-host` publicada
no AstraDocs, commit `e7d4989da98a5f1aa97b93a2d727dc4395a572bf`, deploy
`dpl_FJjkTFM1cmeJPki6yTJEV2v19Trk` **READY**, domínio `astraengine.com.br`.
Build exportou 2.430 páginas Markdown/JSON; 593.490 links verificados, zero
erros. Nota, busca no histórico e disponibilidade conferidas no site público.
A nota preserva o estado da validação no momento em que foi publicada, anterior
ao complemento de importação acima. Não foi reescrita após publicação.

## Continuação D07: capacidade dos buffers Vulkan

O planner distingue extensão lógica de capacidade GPU. Crescimento dentro da
reserva envia diferenças do prefixo e a cauda nova; redução não limpa bytes
inativos. Crescer novamente sempre envia a região reativada, mesmo que ela
contenha bytes de uma revisão anterior. O uso sem capacidade explícita mantém
o contrato anterior de substituir em qualquer mudança de tamanho.

`DirtRoadResources::rebuildAuthoringLibrary` aplica essa política ao consumidor
real: substitui vértices e índices independentemente, mantém uploads parciais
em lote depois de preparar texturas e só adota buffers novos no sucesso.
Reserva de crescimento: 25%, mínimo de 4 KiB e máximo de 8 MiB por buffer.
Redução que deixe mais de 8 MiB ociosos força compactação. Se a reserva opcional
não couber no allocator, tenta o tamanho exato; se ele também falhar, recusa a
publicação. Contadores existentes registram os bytes e faixas realmente enviados.
As contagens de desenho, picking e colisão continuam seguindo a extensão lógica.

Referências: [Godot 4.5 ArrayMesh](https://docs.godotengine.org/en/4.5/classes/class_arraymesh.html),
[MeshStorage 4.5, atualização de regiões](https://github.com/godotengine/godot/blob/4.5-stable/servers/rendering/renderer_rd/storage_rd/mesh_storage.cpp#L520)
e [Vulkan, vkCmdCopyBuffer](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyBuffer.html).
Princípio: atualizar intervalos de buffers existentes, preservando limites e
ownership; a reserva limitada é uma decisão da Astra para edição mobile.

`terrain-capacity-vulkan.log`: **6/6** cenários (4 do planner, 2 Vulkan real),
Intel UHD Graphics. Readback conferiu crescimento, redução, regrowth, bytes
inativos, identidade do buffer e recusa do lote inválido antes de modificar a
primeira faixa. O script usa o lock comum entre worktrees e aceita MinGW GCC
ou LLVM. A unidade `dirt_road_resources.cpp` passou `-fsyntax-only`, C++20,
`-Wall -Wextra -Werror`, NDK 27.1, alvo `aarch64-linux-android26`.

**Limite:** não é uma arena com alocações estáveis por chunk. Inserções no meio
da biblioteca ainda podem deslocar bytes seguintes. A chamada do renderer ainda
espera os leitores globalmente; culling, filas e barreiras por chunk continuam
pendentes. A sessão Android, rollback após falha de dispositivo e desempenho
no aparelho não foram executados. Sem ADB, APK novo ou alteração de formato.
