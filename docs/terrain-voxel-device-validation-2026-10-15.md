# Terreno e voxel — reedição, base sólida e autoria por toque

**Relatório em andamento — 15/10/2026.** Branch `codex/terrain-voxel-generator`, sem merge na main da engine. A validação final do host passou **24/24 cenários**, com revisão das capturas nativas. O **APK Dev 78 foi compilado e auditado** quanto à identidade, assinatura e conteúdo; mantém a limitação de alinhamento ELF de parte do runtime .NET. Este documento registra a segunda iteração, ainda sem aceite de instalação ou execução no aparelho. Os resultados restantes serão acrescentados após a execução correspondente.

Escopo e contratos: [plano desta iteração](planos/terreno-voxel-iteracao2-2026-10-15.md), [contrato da base sólida](terrain-solid-contract-2026-10-15.md), [entrega inicial no host](terrain-voxel-validation-2026-10-14.md) e [APK 75 anterior](terrain-apk-2026-10-15.md). A evidência de uma entrega anterior não valida automaticamente esta revisão.

## Mudanças implementadas no código

- **Reabrir a autoria:** a ação contextual **Editar terreno** no Inspector encontra a receita a partir da raiz ou de um chunk. A resolução usa o vínculo da instância com o recurso importado; não depende do nome visível da entidade. A abertura pelo menu de criação fecha o menu anterior.
- **Superfície ou base sólida:** novos terrenos por altura começam com base sólida. A espessura mínima fica abaixo do ponto mais baixo da superfície já esculpida. O gerador acrescenta fundo plano e paredes apenas no perímetro externo, sem paredes entre chunks. Rocha é a camada das paredes e do fundo. A colisão Mesh recebe essa geometria.
- **Persistência compatível:** novas receitas usam `AETERRAIN2`, com `solidBase` e `baseDepth`. O leitor mantém `AETERRAIN1` como superfície aberta; abrir uma receita antiga não acrescenta volume silenciosamente. A profundidade válida é de 0,1 a 512 unidades locais, com padrão 8. Receitas inválidas não substituem a saída válida.
- **Organização da autoria:** abas **Forma, Pincel, Camadas e Gerir**, com subseções Estrutura/Relevo/Detalhes e Ferramentas/Precisão. Seleção direta de Relevo, Voxel suave e Blocos; controles numéricos com passos; predefinições Plano/Colinas/Montanhas/Ilha que preservam as amostras de escultura. A troca de modo fica bloqueada quando há escultura incompatível com essa troca.
- **Pincel e gesto:** o viewport recebe toque/arraste pelo roteamento de entrada, com espaçamento de amostras em função do raio. Uma aplicação por arraste reúne várias amostras e publica uma revisão ao soltar; Undo reverte o gesto inteiro. O limite da receita é de 256 amostras. **Remover última amostra** é uma operação diferente de Undo. A edição por XYZ continua disponível para atuar dentro do volume.
- **Rascunho e aplicação:** propriedades pendentes não alteram imediatamente a revisão da cena. Aplicar gera e publica a nova revisão; Reverter recupera a receita aplicada. Salvar informa quando ainda é necessário aplicar ou reverter. Cancelar uma geração não publica geometria parcial. A publicação preserva identidade da raiz/chunks e personalizações de instâncias compatíveis.
- **Espaço útil e mensagens:** conteúdo rolável e recortado, ações fixas no rodapé e Cancelar acessível durante a geração. No layout compacto, o estado **Por aplicar/Aplicado** divide a linha com a mensagem da operação, preservando avisos de erro. Em 655×300, o painel reserva 108 px de conteúdo para Forma/Pincel. A execução final dos testes confirmou o alcance por rolagem e a separação do rodapé; as capturas nativas mostram o layout compacto corrigido.
- **Consumo da biblioteca:** Gerir mostra os bytes e partes retidos em relação aos tetos de 128 MiB e 4.096 partes. A admissão de uma revisão nova verifica esse orçamento; uma fonte idêntica pode ser reutilizada pelo hash. A medição representa fontes retidas na biblioteca do projeto, **não a RAM do processo**. Não há limpeza automática de revisões ou fontes.

## Conceito visual e implementação

A [proposta de 15/10](design/terrain-2026-10-15/README.md) foi gerada a partir de uma captura anterior do aparelho. `concept.png` é uma hipótese de organização e identidade visual, não uma captura do APK novo. O terreno ilustrado e o gizmo também são conceituais. A inconsistência da imagem que mostra Superfície selecionada junto de um bloco sólido não foi adotada no produto.

As abas, os subgrupos, a seleção explícita, os passos numéricos, a base sólida, a espessura, a rolagem e o ciclo Aplicar/Reverter correspondem a campos, serialização, geração e ações reais. As capturas atuais do rasterizador de UI nativo no host foram revisadas após estas mudanças e comprovam somente essa camada; não comprovam renderização Vulkan ou interação Android. A avaliação no aparelho continua pendente.

Quatro ícones vetoriais próprios foram integrados pelo pipeline real, com SVG e PNG em `assets/astra-visual/icons`, catálogo, enum e atlas regenerados juntos:

| Conceito | Identificador C++ | Fonte vetorial |
| --- | --- | --- |
| Base sólida | `UiIcon::TerrainSolidBase` | `vector/terrain/solid-base.svg` |
| Superfície | `UiIcon::TerrainSurface` | `vector/terrain/surface.svg` |
| Gerir | `UiIcon::TerrainManage` | `vector/terrain/manage.svg` |
| Editar terreno | `UiIcon::TerrainEdit` | `vector/terrain/edit.svg` |

O script `tools/generate-terrain-authoring-icons.py` produz os vetores e a rasterização correspondente. A auditoria local do atlas encontrou 375 entradas e confirmou, por nome, que as 371 células anteriores permaneceram iguais pixel a pixel. Essa verificação de assets não substitui legibilidade no aparelho. Evidências locais: `build/terrain-evidence/terrain-authoring-icons.png` e `terrain-authoring-icons-audit.json`.

## Assets reutilizados e origem

Permanecem as quatro texturas fornecidas no pacote **Path Painter II 2.1.10**: Grass, Cliff, Dirt e Path. As cópias usadas pela engine são PNG RGB de 256×256, adaptadas dos originais com redução Lanczos. [O manifesto](../assets/terrain/manifest.json) registra arquivo de origem, transformação e hashes. A [licença fornecida](../assets/terrain/LICENSE-supplied.txt) autoriza uso, reprodução, adaptação e implementação; para comercialização, prevê comunicação posterior ao representante autorizado, sem transformá-la em pedido de aprovação.

A análise inicial também consultou Digger 6.0 e MicroSplat 3.9.41. A implementação C++ da Astra é própria; os algoritmos Unity/Burst, DLLs de editor e shaders completos desses pacotes não foram portados. Os ícones novos são vetores próprios, não recortes da proposta gerada. Licença e proveniência fazem parte do empacotamento; a auditoria do APK 78 confirmou ambas idênticas aos arquivos locais.

## Referências oficiais versionadas

As referências abaixo já sustentam o plano e o contrato desta iteração. Fundamentam as capacidades estudadas; não representam paridade com essas engines nem cópia de sua aparência.

- **Godot 4.6 — [SurfaceTool](https://docs.godotengine.org/en/4.6/classes/class_surfacetool.html):** construção de malhas com atributos, normais e UVs.
- **Godot 4.6 — [BoxMesh](https://docs.godotengine.org/en/4.6/classes/class_boxmesh.html):** volume retangular e subdivisão das faces, referência para o fechamento da base.
- **Unity 6.0 — [Heightmaps](https://docs.unity3d.com/6000.0/Documentation/Manual/terrain-Heightmaps.html):** dimensões, resolução e altura do terreno.
- **Unity 6.0 — [Raise or Lower Terrain](https://docs.unity3d.com/6000.0/Documentation/Manual/terrain-RaiseLowerTerrain.html):** seleção de ferramenta, pincel e intensidade aplicada na cena.

## Matriz de validação em andamento

| Camada | Estado nesta edição do relatório | Evidência e próxima verificação |
| --- | --- | --- |
| Assets/atlas | Verificação local concluída | Quatro ícones novos, 375 entradas, preservação das 371 anteriores; ainda conferir aparência no APK. |
| Host — primeira rodada | **21/23 passaram — histórico** | O rodapé compacto não expunha o estado pendente: a UI foi corrigida. O teste do Inspector também precisava abrir Painéis → Inspector antes de procurar o cartão no modo compacto: o cenário de teste foi corrigido. Ambos passaram na execução final. Log local: `build/terrain-evidence/iteration2-host-tests.log`. |
| Host — repetição após correções | **24/24 passaram** | Recompilação concluída com exit code 0. Inclui o novo cenário para orçamento de fontes retidas e reutilização de hash. Logs: `build/terrain-evidence/iteration2-host-build-final.log` e `iteration2-host-tests-final.log`. |
| UI nativa — 1280×720 e 655×300 | **Testes e oito capturas do host revisados** | Alcance por rolagem, clipping, rodapé, abas/subseções, estrutura, predefinições e ciclo pendente/aplicar/reverter/salvar passaram. Capturas regeneradas dos PPM da execução final: `build/terrain-evidence/iteration2-host-ui-{1280,655}-{0,1,2,3}.png`. |
| APK Dev 78 — compilação | **CONCLUÍDA** | `BUILD SUCCESSFUL in 7m 15s`, 53 tarefas: 19 executadas e 34 atualizadas. Log: `build/terrain-evidence/iteration2-android-build-retry.log`. Identidade e hash abaixo. |
| APK Dev 78 — conteúdo e integridade | **PASSOU** | CRC, ELF ARM64, marcador `AETERRAIN2`, menu, quatro texturas incorporadas, licença/proveniência, atlas de 375 ícones com os quatro novos e igualdade dos quatro módulos gerenciados. Compatibilidade do leitor legado é evidência do host, não de busca de strings no APK. |
| APK Dev 78 — identidade/assinatura/alinhamento | **Identidade, assinatura v2 e ZIP passaram; ELF 16 KB incompleto** | Pacote `dev.aether.editor.u07`, código 78, Astra Dev, ARM64, não debuggable e mesma chave do APK 75. libhostfxr e seis bibliotecas .NET ainda usam segmentos de 4.096 bytes. |
| Aparelho — instalação preservando dados | **PENDENTE** | Confirmar versão/certificado da instalação existente antes de atualizar. Não apagar projetos nem desinstalar para contornar incompatibilidade de assinatura. |
| Aparelho — autoria nos três modos | **PENDENTE** | Criar, reabrir por raiz/chunk, esculpir/pintar, testar arraste, Undo/Redo, aplicar/reverter, salvar/reabrir e colisão; registrar capturas e logs da execução. |
| Publicação da entrega | **PENDENTE** | Publicar apenas a branch da engine e uma nova nota App/Docs no AstraDocs com o estado real. Não tratar revisão local como APK distribuído nem alterar o download público sem entrega correspondente. |

Os nove testes que passaram em 14/10 e a auditoria do APK 75 permanecem evidência histórica separada. A existência de capturas anteriores do aparelho demonstra apenas o estado capturado naquele momento; não valida os controles nem a geometria desta revisão.

### Resultado final do host

O executável `build/editor-host/aether_terrain_tests.exe` concluiu com `24/24 scenarios passed`. A cobertura reúne quatro cenários de interação/layout da UI, sete de reedição/pincel/orçamento da biblioteca, nove cenários originais de terreno e quatro de base sólida. Foram exercitados os três modos, o importador GLB real, migração e reabertura de receitas, publicação atômica, Undo/Redo, salvar/reabrir, cancelamento, limites e raycasts de colisão. Isso valida esses caminhos no host, sem transferir o resultado para Android.

Os dois cenários que falharam na primeira rodada foram reexecutados com sucesso: o rodapé compacto mantém o estado pendente e a explicação do bloqueio de salvamento; após abrir o Inspector compacto pelo menu de painéis, tocar em Editar terreno abre a receita pela raiz ou pelos chunks. A segunda correção completou a preparação do teste, sem representar outro defeito do produto. O novo cenário confirma a recusa de crescimento acima do orçamento e a reutilização de uma fonte de hash existente.

Os oito PPM produzidos pela execução final foram convertidos para PNG e revisados nas quatro abas em 1280×720 e 655×300. Na resolução maior, a hierarquia de campos e grupos permanece legível, com seleção e ação principal distintas. Na menor, o painel reserva espaço para o viewport e mantém abas, subgrupos, estado e ação principal visíveis; os controles que excedem a área aparecem por rolagem, cujo alcance foi verificado nos testes. Não foram identificados vazamentos de conteúdo sobre o rodapé ou sobreposição indevida nos estados capturados. O recorte parcial da próxima linha de conteúdo indica continuidade rolável. As capturas mostram o estado inicial de autoria, sem geometria de uma cena Android; erro, aplicação e reabertura são cobertos pela interação automatizada do host, não por screenshots de aparelho.

## Compilação e auditoria do APK 78

O artefato auditado está em `android/app/build/outputs/apk/release/app-release.apk`, com **109.172.209 bytes**. Identidade: pacote `dev.aether.editor.u07`, nome **Astra Dev**, código **78**, versão **`0.3.1-dev.terrain.20261015.1`**, ABI `arm64-v8a`, mínimo Android API 26 e alvo API 35. O manifesto não marca a aplicação como debuggable. SHA-256:

```text
a804f9d096fd0c5d1039b8ac65e95d42e956149c3a71e050625f7f06dbda1ee7
```

A primeira tentativa concluiu a compilação nativa, mas falhou em `mergeReleaseAssets`: o Windows mantinha uma seção mapeada de `merger.xml`. A remoção do cache incremental pelo Gradle e a repetição do build permitiram o fechamento, sem correção adicional nas fontes. O retry terminou com `BUILD SUCCESSFUL in 7m 15s`, 53 tarefas, das quais 19 executadas e 34 já atualizadas. Logs locais: `build/terrain-evidence/iteration2-android-build.log` e `iteration2-android-build-retry.log`.

Os scripts locais `build/verify-terrain-apk.py` e `build/verify-terrain-apk-identity.ps1` foram executados pela raiz e produziram `build/terrain-evidence/iteration2-apk-audit.json` e `iteration2-apk-identity.json`. O primeiro confirmou CRC do ZIP, ELF ARM64, marcador da receita `AETERRAIN2`, menu, quatro texturas incorporadas, licença/proveniência, atlas atual e seus quatro novos ícones, igualdade dos quatro módulos gerenciados com o build e ausência da camada Vulkan de validação. O atlas incorporado tem 375 entradas e SHA-256 `4956fc34ce0b60b4929b9cc48c2a41c20e205a3e3ad5da70ec3ff3d0f44cb396`.

A primeira execução do auditor exigia o literal `AETERRAIN1` contíguo no ELF. Essa exigência foi removida: o compilador pode substituir a comparação por imediatos, de modo que a ausência dessa string não demonstra ausência do leitor. Nenhuma garantia de migração é inferida pela busca binária. A compatibilidade legada passou no teste host `terrain_solid_recipe_migrates_legacy_and_roundtrips_glb`; o fluxo correspondente em um projeto antigo no aparelho permanece pendente. Os demais checks do auditor foram preservados.

Comandos de auditoria usados:

```powershell
python build/verify-terrain-apk.py
./build/verify-terrain-apk-identity.ps1 -ExpectedVersionCode 78
```

A assinatura APK v2 passou, com certificado SHA-256 `68a37f8803e5f27155676328e8dc97385b41930bd878ad4168b1c962026599be`, igual ao APK 75 gerado nesta máquina. Ela difere da chave histórica documentada; essa igualdade não autoriza presumir a chave da instalação de destino. Saídas: `iteration2-apksigner.txt`, `iteration2-aapt-badging.txt` e `iteration2-zipalign.txt`, na mesma pasta de evidências.

O `zipalign -c -P 16 -v 4` passou. A inspeção separada dos segmentos ELF encontrou alinhamento de 16.384 bytes em engine, transform, libc++ e OpenSSL; `libhostfxr` e seis bibliotecas do runtime .NET 8.0.27 permanecem em 4.096 bytes. Portanto, o APK 78 **não tem conformidade ELF integral para páginas de 16 KB**. Os dois relatórios mantêm `runtime_verified: false`: assinatura, conteúdo e alinhamento não comprovam execução ou interação no aparelho.

## Limitações mantidas

- Terreno finito e regeneração completa por aplicação, sem remeshing incremental, streaming, LOD, vegetação ou erosão hidráulica.
- Base sólida por altura é uma casca triangular fechada para render e colisão estática. Não acrescenta simulação de massa nem cavernas ao modo por altura; escavar/adicionar volume continua sendo função dos modos voxel.
- Pintura seleciona camada por região horizontal. UVs pelo eixo dominante não implementam mistura triplanar entre projeções.
- Limites atuais incluem 8–128 células horizontais, 8–64 verticais nos voxels, orçamento de 600 mil amostras, 200 mil triângulos e 256 amostras de pincel. O armazenamento retido também é limitado na admissão de revisões; não há coleta automática de fontes antigas.
- As métricas da biblioteca não são medição de RAM, desempenho, FPS ou consumo no Android. O relatório ainda não contém aceite de estabilidade, ciclo de vida ou renderização no aparelho para esta revisão.
- A exportação de APKs de jogos permanece fora desta validação; o checkout não contém `:export-core`. A compilação do editor não comprova esse caminho.

### Casos de falha identificados na revisão estática

**Legibilidade de Rocha/Terra no APK 78.** As capturas reais
`build/terrain-evidence/device78-heightfield-solid.png` e
`device78-paint-drag.png` mostram paredes e pintura excessivamente pretas.
Os PNGs fornecidos já têm valores escuros: mediana de luminância sRGB ponderada
(escala 0–255) de 33,9 em Rocha e 28,4 em Terra, contra 72,2 em Grama.
O GLB texturado usa fator de cor branco, metálico zero e rugosidade 0,95;
o importador mantém a textura sRGB e cor de vértice branca quando não há
`COLOR_0`. A revisão não encontrou um tint preto adicional nesse caminho.

O A/B no aparelho confirmou a perda de detalhes pelo processamento:
`device78-scene-effects.png` e `device78-post-disabled.png`, na mesma pasta
de evidências e com o mesmo enquadramento, mostram recuperação da textura
das laterais e da leitura da grama ao desligar **Post Processing**.
Há um amplificador concreto no processamento da imagem: o preset de prévia
da cena no APK 78 usa contraste 1,06; `post_process_common.glsl` aplica
`(color - 0.5) * contrast + 0.5` após tone mapping, ainda em display-linear,
e limita negativos a zero. Nesse contraste, canais abaixo de aproximadamente
0,0283 ficam pretos.

A correção nas fontes altera somente `defaultSceneViewEnvironment().contrast`
de 1,06 para **1,0**, em `native/renderer/scene_environment.cpp`. Saturação
1,04, exposição, bloom, vinheta e oclusão foram preservados. Não modifica
texturas, materiais, shader global nem ambientes autorados do runtime.
O build host deverá incorporar esse ajuste; a confirmação visual da correção
continua **pendente no APK 79, com Post Processing ligado**, sem tratar o A/B
do APK 78 como validação do binário novo.

O teto da biblioteca não é uma coleta de recursos. Se o GLB for importado com
sucesso e a aplicação posterior falhar por limite de objetos ou chunks
duplicados, essa fonte pode permanecer retida sem uma nova referência na cena.
A cena e seu Undo são preservados; a fonte continua contando no orçamento.
Antecipar essas validações ou integrar o rollback da publicação é trabalho
pendente, não uma garantia desta entrega.

Se Undo trocar a fonte do terreno para um GLB ausente ou corrompido enquanto
o painel estiver aberto, a tentativa automática de reabertura pode repetir a
leitura e a contagem da biblioteca em atualizações seguintes. A revisão
identificou esse caso excepcional; ele não foi corrigido nem validado no
aparelho nesta etapa. O fluxo normal com as revisões preservadas passou nos
testes de Undo/Redo. Evite apagar fontes de revisão manualmente.

## Fechamento a preencher após execução

| Evidência final | Valor |
| --- | --- |
| Commit funcional / commit publicado da branch | PENDENTE |
| Host: total, falhas e log final | **24/24, zero falhas**; `build/terrain-evidence/iteration2-host-tests-final.log`; compilação em `iteration2-host-build-final.log`, exit code 0 |
| Capturas executáveis finais e avaliação visual | **Host revisado**, oito PNGs `iteration2-host-ui-{1280,655}-{0,1,2,3}.png`; capturas Android pendentes |
| APK: arquivo, código, versionName, bytes e SHA-256 | `android/app/build/outputs/apk/release/app-release.apk`; **78**, `0.3.1-dev.terrain.20261015.1`, **109.172.209 bytes**; SHA-256 `a804f9d096fd0c5d1039b8ac65e95d42e956149c3a71e050625f7f06dbda1ee7` |
| Assinatura, identidade, ZIP/ELF e conteúdo | **Assinatura v2, identidade, ZIP e conteúdo passaram**; mesma chave do APK 75. **ELF 16 KB incompleto** nas bibliotecas .NET descritas acima. Relatórios `iteration2-apk-audit.json` e `iteration2-apk-identity.json`. |
| Aparelho: modelo/API, instalação, cenários e logs | PENDENTE |
| Nota pública, commit AstraDocs e deploy verificado | PENDENTE |
