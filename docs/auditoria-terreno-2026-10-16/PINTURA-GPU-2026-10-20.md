# Pintura independente na GPU — 20/10/2026

Continuação do plano de 71 itens, na branch `codex/terrain-voxel-generator`. O primeiro bloco e o conjunto dos plugins continuam abertos. Sem ADB nesta etapa; o APK 110 não contém estas alterações.

## Implementação

O renderer passa a amostrar o mapa de controle por fragmento. A pintura deixa de depender apenas da interpolação das cores dos vértices: uma marca pode aparecer no interior de um triângulo mesmo quando nenhum vértice cai dentro do pincel. O mesher conserva os pesos de vértice para compatibilidade e para pintura volumétrica.

Terrenos novos criados pelo editor começam com 128 células no mapa de pintura. **Forma → Detalhes → Células do mapa de pintura** continua sendo uma barra de 8–512 em passos de oito, com entrada precisa; 0 conserva o modo legado. Abrir terrenos antigos não altera automaticamente a receita. Resolução da altura e da pintura continuam distintas. Vegetação/detalhe independentes ainda não estão implementados: P11 permanece parcial.

O GLB usa `astraTerrainSurface:2`, com PNG de controle linear RGBA8 e transformação de UV explícita. Os limites externos correspondem aos centros dos texels extremos, evitando deslocamento de meia amostra e repetição de uma borda na outra. A quantização conserva exatamente 255 unidades por texel; os pesos de autoria permanecem float na receita/campo reexecutável. O PNG é lossless, tem filtro linear/clamp e um nível, de até 513×513 (~1 MiB). A resolução desse dado não cai por compressão ASTC, limite de dimensão de albedo ou redução de mip. Falta de orçamento produz recusa explícita.

Laterais e fundo do relevo fechado têm um marcador de elegibilidade separado no sinal da variação macro; conservam os pesos fixos, sem projeção acidental da pintura do topo. A variação macro dessas faces é decodificada preservando seu valor. Voxels mantêm sua pintura tridimensional e a superfície v1.

O caminho completo transporta as referências por importação, cache, blocos de fonte, biblioteca achatada, deduplicação de texturas e publicação Android. As referências de controle são remapeadas junto com os albedos. Os dados transitórios da publicação não sobrevivem como spans pendurados. A prévia produz a nova imagem diretamente do campo; aplicar reutiliza o build já concluído, sem repetir a geração do campo para exportar o GLB.

Cache derivado: seção opcional `ASTRATC1`, versão 1; leitores atuais aceitam caches anteriores sem a seção. A revisão do importador sobe para 8 e força a regeneração dos derivados antigos. O formato de receita continua AETERRAIN7. Versões anteriores do renderer não oferecem a pintura por fragmento; preserve o projeto para abrir em revisão compatível.

Sem bindless ou quando o descritor não pode ser alocado, o shader preserva a avaliação por vértice. Esse fallback não equivale à pintura fina. O catálogo extensível, normal/altura/AO/roughness/metallic por camada, ruídos, clusters e a UI completa de materiais permanecem pendentes. Os assets originais não foram reduzidos ou substituídos.

## Referências oficiais

- [Unity 6000.0 — TerrainData.alphamapResolution](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainData-alphamapResolution.html): resolução própria do mapa de mistura, distinta da altura.
- [Unity 6000.0 — TerrainData.SetAlphamaps](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/TerrainData.SetAlphamaps.html): pesos de mistura por posição e camada. A codificação, os limites e o pipeline GPU desta implementação são próprios da Astra.

## Validação

- **96/96 cenários integrados** passaram no host: `build/terrain-evidence/terrain-control-final.log`. Incluem GLB real, cache, pintura antes de soltar, cancelamento, Undo/Redo e reabertura. A primeira passagem reteve dois objetos compilados antes das últimas edições; a recompilação incremental e a execução final passaram.
- **18/18 cenários de publicação de texturas** passaram: `terrain-control-textures.log`, incluindo material/UV/canais e lightmap. As contagens são de suítes distintas, não de recursos dos plugins concluídos.
- **Oito modos offscreen na GPU NVIDIA GeForce RTX 3050 6GB Laptop GPU** passaram com leitura de pixels: `terrain-control-gpu.log`. O executável compila os includes de produção `terrain_surface.glsl` e `material_uv_transform.glsl` e usa o publicador C++ real das linhas. Confere faixa interior sem vértices, exterior, bordas UV exatas, transformações assimétricas, face fechada e fallback. Usa uma tabela de cinco descritores; não prova o alocador bindless completo ou a cena Android.
- Todas as famílias de shaders embutidos foram regeneradas e validadas por SPIR-V: `terrain-control-shaders.log`. Não se embarca uma variante com o stride anterior de 14 linhas depois de ampliar para 15.
- Painel de produção executado/rasterizado em 1280×720 e 655×300: `control-ui-1280.png`, `control-ui-655-map.png`, `control-ui-655.png`. Barra, valor preciso, legibilidade, rolagem e ação fixa conferidos. Fundo vazio do teste não é uma cena 3D.
- Dois testes condicionais de symlink não executaram as asserções específicas por falta de privilégio no Windows; a suíte identifica essa limitação no log. Corrupção, recuperação e publicação atômica foram exercitadas normalmente.
- **14/14 unidades Android ARM64** passaram na compilação sintática, incluindo importação/cache, sessão, shell e renderer: `terrain-control-android.log`. Isso não prova link, APK ou runtime; sem ADB nesta etapa.


## Continuidade

Código funcional `47b4cd6b` publicado na branch de terreno. O teste de reabertura foi reforçado depois para comparar os pixels do material **visível** com a receita reexecutada, distinguindo-o das revisões antigas retidas: passou em execução focal (`terrain-control-reload-pixels.log`), sem alterar o runtime depois da suíte integrada.

A próxima integração é o catálogo original no editor e no renderer PBR: seleção persistida por identidade, imagens originais/canais corretos, publicação de tabela própria de camadas e consumidores de normal/roughness/metallic/AO/altura. O contrato isolado de materiais não constitui esse recurso; M01–M08 e os demais itens permanecem no plano integral.


## Publicação

AstraDocs `2f3efbb805f1a0a1b3450badf4bb6573996bf098` publicado na main do site. Nota nova `2026-10-20-terreno-pintura-por-fragmento`; todas as notas anteriores preservadas semanticamente. Guia próprio: https://astraengine.com.br/pt-br/desenvolvimento/2026-10-20/terreno-pintura-gpu/ . Build local: 2436 páginas, 2435 Markdown/JSON e dez exemplos; 594696 links verificados, zero erros. Revisão do guia em desktop e viewport mobile realizada; emulação restaurada, aba temporária fechada e servidor 4187 encerrado.

Vercel `dpl_DUaKJTv9DDrMiqwv5GYkhjDvq399` READY, domínio principal atribuído ao commit acima. HTTP 200 e conteúdo atual confirmados no guia, Markdown, feed e HTML de Atualizações. Nenhum APK novo foi gerado/distribuído; ADB continua suspenso e a meta dos 71 itens permanece ativa.
