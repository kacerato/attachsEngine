# Aplicação da prévia — D07, 19/10/2026

Continuação dos 71 itens, sem alterar o universo ou as propriedades comprometidas.
O usuário reautorizou ADB: implementar primeiro, validar no aparelho depois.

## Mudança

O commit síncrono e o job assíncrono de terreno passam a capturar a revisão
imutável da prévia. Quando a receita serializada coincide e a malha está completa,
compartilham esse resultado sem regenerar campo ou malha. Caso contrário,
`updateTerrain` recebe a revisão anterior para reutilizar chunks compatíveis.
Prévias que contêm apenas campo não são consideradas malhas prontas.

O job mantém sua própria referência; cancelar/trocar a prévia da UI não invalida
o resultado em processamento. Permanecem as verificações de cancelamento,
projeto, época de cena, revisão do documento e alvo antes da publicação.
GLB, importação e publicação agregada continuam existindo. Não é conclusão de
D07, arena GPU por chunk ou sincronização localizada.

Não foram reduzidas texturas, geometria, propriedades nem controles de qualidade.
Referência oficial do D07: Unity 6000.0,
[Mesh.SetVertexBufferData](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Mesh.SetVertexBufferData.html).
Essa referência fundamenta atualizações de geometria; o cache é implementação
própria da Astra, não reprodução do algoritmo interno da Unity.

## Integração Android

O primeiro build identificou ausência da dependência `:export-core` no app e do
callback `onProjectExport` no shell. A dependência foi conectada e o callback
abre a Activity existente com nome e caminho do projeto escolhido. Não é uma
nova implementação do exportador. A compilação Java passou após a correção.

## Evidências

- Primeira rodada: 57/57 cenários host, incluindo reuso síncrono/assíncrono e
  equivalência exata dos vértices, normais, UVs e pesos na aplicação regional.
- Rodada final: **58/58 cenários passaram**. O cenário de soltura inicialmente
  pressupunha que o arrasto não acrescentava amostra na ponta. A hipótese estava
  errada: o sampler descarrega o trecho residual ao soltar. O teste final usa
  um toque estacionário com prévia concluída, cuja receita efetivamente coincide;
  o arrasto mantém sua amostra final e pode usar atualização regional.
- APK 106 compilado: `0.3.1-dev.terrain.20261019.1`, ARM64, pacote
  `dev.aether.editor.terrain`, 116.331.828 bytes. Assinatura v2/certificado anterior,
  alinhamento ZIP, ELF idêntico à saída nativa, assemblies e 397 ícones conferidos.
  SHA-256: `b01ab536badb57c8feb85d62eb2c14d120212b674e61f20bfe46a786ae525f71`.
  Arquivo local: `build/artifacts/Astra-Terreno-106-arm64.apk`.
- APK 106 instalado inicialmente; após desbloqueio, testados criação, elevação
  com prévia, aplicação, Undo/Redo, salvar e reabrir no projeto TerrainQA106.
- O projeto antigo TerrenoQA_1510 (795 operações) revelou prévia branca:
  log confirmou 260 texturas para 255 slots disponíveis. O fix compartilha os
  bindings originais da fonte na biblioteca e conserva o perfil de importação.
  Não acrescenta texturas, não reduz resolução nem altera o histórico.
- 58/58 cenários passaram após o fix, agora verificando também igualdade das
  imagens compartilhadas e de todos os bindings de camada na prévia.
- APK **107**, `0.3.1-dev.terrain.20261019.2`, 116.331.384 bytes, compilado,
  auditado e instalado com Success; versão confirmada pelo Android.
  SHA-256: `4cb5239334613059e224bdb1b0e1a87cc28c62a7d6d4c7acf370fb0e99989aa1`.
  Arquivo: `build/artifacts/Astra-Terreno-107-arm64.apk`.
- No 107, prévia com grama/rocha confirmada no mesmo projeto antigo, antes da
  soltura. Log: 256 texturas reaproveitadas, zero enviadas; ainda existe um
  descritor excedido na própria biblioteca antiga. Aplicar continua recusado
  pelo orçamento agregado de 128 MiB CPU. Não houve exclusão de revisões.
- No projeto novo, 107 confirmou elevação texturizada durante gesto, aplicação
  (14 para 28 operações), Undo (14), Redo e salvamento. Capturas reais no
  diretório local de evidências; gizmo de mover ausente na autoria do terreno.
  Os controles principais são legíveis e acessíveis em paisagem; a mensagem
  longa de orçamento fica cortada no rodapé. Não declarar aceite integral da UI,
  de voxel, transições LOD, todos os pincéis ou desempenho nesta rodada.
- Compilação com um worker Gradle e pool nativo de um compilador. O APK não
  passou no critério agregado ELF de 16 KB; não declarar essa compatibilidade.
- Logs locais: `build/terrain-evidence/apply-cache-20261019.log`,
  `apply-cache-final-20261019.log`, `android-build-20261019.log`,
  `android-build-retry-20261019.log`, `device106-apk-audit.json`,
  `device106-identity.log` e `device106-crash-buffer.txt`.

O primeiro bloco de 28 itens continua incompleto. Materiais/PBR extensíveis,
mapas independentes, altura-volume, GC/checkpoints, vizinhança e demais lacunas
do aceite permanecem obrigatórios.

## Evidências posteriores ao desbloqueio

- `preview-textures-20261019.log`: 58/58.
- `android107-build.log`: build incremental com um compilador, sucesso.
- `device107-identity.log`, `device107-apk-audit.json`: identidade e pacote.
- `device106-bindless-before.log`: falha de descritores reproduzida.
- `device107-old-held.png`, `device107-old-logcat.log`: prévia corrigida.
- `device107-old-release.png`: orçamento continua protegendo o projeto antigo.
- `device107-fresh-held.png`, `device107-fresh-applied.png`, `device107-undo.png`:
  escultura e histórico no projeto novo.

Próximo ponto obrigatório: D09, residência/histórico por referências, sem apagar
recursos usados em cenas ou Undo; D07 continua parcial. A biblioteca antiga ainda
satura o limite do renderer. Não resolver aumentando limites ou cortando assets
sem estudar o gerenciamento. O plano e as propriedades comprometidas permanecem.

## Publicação desta entrega

Engine: `03015d8c`, publicada somente em `codex/terrain-voxel-generator`.
AstraDocs: `4bbec6909c43b8c2c3786f0c6d3e759703a1b6f8`, publicada na main do site.
Deploy Vercel `dpl_DKp5aMh89c76BndCp2u6NxuNqHaP`: READY, domínio
`astraengine.com.br`; nota 107 confirmada no site. Build: 2.434 páginas HTML,
2.433 Markdown, dez exemplos; verificador: 594.213 links, zero erros.
Reabertura final do 107 preservou 28 operações (`device107-reopened-final.png`).
O APK público não foi substituído; 107 é a distribuição de teste instalada via ADB.
