# N07 — fontes, conversão de material e prévia do objeto

Revisão: 10/10/2026. Estado do bloco completo N07: **parcial**. Este documento
registra a fatia implementada e os limites; o inventário de um arquivo não é
certificação de que a Astra executa a capacidade que ele contém.

## Fontes e cobertura

`plugins-cobertura.md` é a matriz por família, `plugins-itens.csv` registra as
2.816 entradas presentes, caminho, tamanho e SHA-256, e
`plugins-declaracoes.csv` relaciona declarações dos catálogos fornecidos com sua
origem e destinos N01–N16. A extração cobre UMotion, FinalIK, BoZo e integrações;
POLYGON Icons v1.01 foi localizado em pasta adicional de Downloads; o inventário
incorpora suas 1.075 fontes extraídas, arquivo do pacote e aviso de licença.
Os 520 sprites/GLB e 16 paletas possuem pipeline offline já existente, mantido
em `local-assets/synty-polygon-icons-v1.01`, sem redistribuição nas docs públicas.
`tools/validation/inventory_plugin_sources.py` reproduz a extração sem copiar
payloads privados para documentação pública. As regras de licença estão na
matriz; uma referência Unity não equivale a plugin executável no Android.

## Decisões e referências

[Unity 6.0 Material Inspector](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Material.html)
separa o recurso compartilhado de seus usos e expõe propriedades conforme o
shader. A adaptação Astra mantém o Inspector real e oferece vínculo no slot,
localização de usos e prévia contextual, sem introduzir propriedades sem consumidor.

[Unity 6.0 Standard Shader](https://docs.unity3d.com/6000.0/Documentation/Manual/StandardShaderMaterialParameters.html)
é a referência para fatores e mapas suportados. A conversão é explícita entre
contratos; não promete BRDF idêntica. A
[serialização textual Unity 6.0](https://docs.unity3d.com/6000.0/Documentation/Manual/FormatDescription.html)
fundamenta a leitura limitada de referências GUID/fileID, sem tratar qualquer
YAML como um material compatível.

[Godot 4.5 MaterialEditor, código oficial](https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/material_editor_plugin.cpp)
usa um mundo separado, câmera e geometria real para sua prévia; esfera, caixa e
plano recebem o material efetivo. Esse princípio orienta a próxima fatia de
material sem uso. A prévia implementada nesta revisão usa o objeto autoral já
publicado e não cria esse mundo separado. O
[MeshInstance3D 4.5](https://docs.godotengine.org/en/4.5/classes/class_meshinstance3d.html)
distingue o material ativo das substituições por superfície; Astra conserva a
mesma distinção funcional entre recurso e substituições locais existentes.

A busca por vídeos específicos de workflow não retornou referência verificável
nesta rodada. Não se atribui observação de vídeo ao que foi estudado apenas no
manual/código. A validação visual usa a UI executável e capturas host abaixo.

## Cadeia implementada

```text
.mat Unity original + .meta das texturas
  -> parser limitado Standard metálico
  -> dependências resolvidas por GUID, perfil de cor/normal/dado
  -> TextureAsset/cache existente + MaterialAsset v6
  -> transação nativa arquivo/AssetRegistry
  -> Inspector compartilhado / vínculo de slot com histórico
  -> EditorMapScene resolve valores e texturas efetivos
  -> prévia independente com geometria autoral + backend Vulkan existente
```

`unity_material_import.{h,cpp}` é o parser sem I/O; o limite de entrada é 256 KiB.
`editor_unity_materials.cpp` pertence à sessão e reutiliza transações, registro,
publicação de texturas e histórico já existentes. A UI seleciona o `.mat` no
navegador e oferece **Converter material Unity**; o resultado é o arquivo irmão
`.mat.material`, aberto no Inspector nativo. Não exige script externo para esse
workflow. A importação mantém a fonte e seus metadados.

No Inspector, **Aplicar no slot** vincula o recurso ao slot selecionado por meio
de `assignSlotMaterial`; as substituições locais permanecem. O status informa
esse comportamento. Para os fatores compartilhados serem visíveis, o usuário
usa a herança de material já existente (`clearSlotMaterialOverride`). Undo/redo
do vínculo é alteração normal da entidade; Undo da conversão remove o recurso
nativo somente se ele não tiver sido modificado ou vinculado posteriormente.
Texturas convertidas permanecem reutilizáveis.

Reimportar usa a identidade já registrada, receita e baseline normalizado do
material nativo. Alteração local detectada recusa sobrescrita. Fonte e texturas
são parte de um comando coeso: Undo/Redo restaura valores, receita e dependências
juntos, com revisão monotônica e verificação de metadados externos. A publicação
ocorre antes de alterar biblioteca/histórico em memória; falha de disco preserva
o estado anterior. Reimportar após Undo não acusa conflito por receita posterior.
Fonte e texturas
são relidas antes de publicar; referências ausentes, GUIDs duplicados, perfil
incompatível e entradas inválidas causam diagnóstico. O registro e arquivo
nativo usam `EditorImportTransaction`; caches de textura não alcançáveis podem
permanecer após uma operação recusada, mas nenhum material parcial é publicado.
O carregamento frio segue o contrato do shell: diretório do projeto **e carga do
AssetRegistry persistido**; apenas trocar diretório não carrega o registro.

## Contrato de conversão

| Entrada Unity | Resultado / limite |
|---|---|
| Built-in Standard metálico, shader fileID 46 e GUID built-in | Conversão suportada para parâmetros abaixo |
| `_Color` RGB, `_Metallic`, `_Glossiness` | Cor, metálico e rugosidade `1 - glossiness` |
| `_MainTex` + UV scale/offset | Binding base, GUID real e transformação efetiva compartilhada Standard |
| `_BumpMap` com `_NORMALMAP`, `_BumpScale` | Normal e força, perfil normal |
| `_EmissionMap`/HDR `_EmissionColor` com `_EMISSION` | Emissão normalizada e intensidade dentro do contrato nativo |
| `_OcclusionMap`, `_OcclusionStrength` | Mapa de dado, canal G e intensidade |
| `_Mode` 0 / 1 / 2, `_Cutoff` | Opaque / máscara / blend e corte |
| Shader customizado/URP/legacy/specular | Recusado; fonte preservada |
| MetallicGloss packed, detail, parallax ativos | Recusado; sem descartar silenciosamente mapas |
| Premultiplied, cor com alfa diferente de 1, controles sem equivalente | Recusado com diagnóstico |

O parser não executa shader Unity, não extrai FBX e não traduz materiais BoZo
customizados. Referências de textura são fileID 2800000; outros sub-recursos
pedem suporte específico. Campos desconhecidos continuam na fonte original;
isto não significa que tenham comportamento nativo.

## Prévia contextual do objeto

NÃO IREI SER SIMPLISTA NO DESIGN.

A ação **Prévia do objeto** aparece quando a seleção ou um descendente usa o
recurso. Uma seleção de grupo funciona mesmo quando o renderer pertence ao
filho. `editor_material_object_preview.h` extrai a geometria publicada, reúne
limites mundiais e enquadra o conjunto com perspectiva. Não cria malha substituta,
não altera a câmera principal e não esconde objetos do documento. Sem geometria
publicada ou sem vínculo, a operação apresenta diagnóstico.

O snapshot carrega IDs ordenados imutáveis; `instanced_camera_preview.inl` filtra
os desenhos autorais antes de executar o caminho PBR existente. O ambiente vem
da cena na posição da câmera de prévia. Substituições locais continuam efetivas
e são declaradas na legenda. Água dependente de simulação/depth permanece fora
da prévia, conforme a limitação existente desse backend.

Há uma prévia por sessão; abrir material substitui a prévia de câmera anterior.
Fechar, trocar cena, perder contexto ou ocultar o editor invalida leases/imagens.
Uma conclusão só publica se requestId, epoch, revisão da cena e revisão do
material coincidirem. O orçamento padrão permanece 640×360, 15 Hz, com timeout
de conclusão. Nenhuma UI afirma imagem pronta antes de sucesso do backend.

`N07-material-preview-conceito.png` é hipótese visual gerada, sem evidência GPU.
O ícone `materials/object-preview` é gerado com as primitivas angulares Astra por
`tools/generate-material-preview-icon.py` e empacotado no atlas executável junto
dos demais ícones. As ações têm altura 48, área útil 42 após margem. A captura de
655×300 deve ser julgada separadamente da de 1200×540.

## Validação e limites pendentes

O alvo `aether_unity_material_tests` reúne cinco cenários de comportamento:
conversão/persistência, recusas, transação/histórico/reabertura/conflito,
dependência ausente e workflow real de textura/vínculo por toque/prévia de filho.
Inclui enquadramento, exclusão de objetos alheios, stale lease e fechamento.
A opção `ASTRA_MATERIAL_CAPTURE` rasteriza a UI nativa real em host; o sufixo
`.655x300.ppm` registra a superfície compacta. Essa captura não é imagem Vulkan.

O pacote inicial de conversão e prévia de objeto passou **5/5 cenários host**
no build conjunto de 10/10, junto com recursos de animação 26/26, editor 44/44 e
SDK C# integrado 1/1 (zero pulados). O resultado de materiais está em
`docs/validacao/n01-n07-2026-10-10/host/material-final.log`; capturas e demais
resultados ficam no mesmo ledger. Esses números não aprovam a integração
posterior de viewer independente/keywords alfa descrita abaixo.
ADB não foi usado e a validação física permanece bloqueada pelo proprietário.

N07 ainda exige catálogo navegável por pacote/origem/dependências/status,
miniaturas persistidas, aceite Vulkan do viewer independente, ambiente próprio,
shader mappings específicos,
conversão completa de `.anim`/controller/prefab, importação/aceite de cada asset,
identidade em renomeação de fonte e conflitos de reimportação por propriedade.
A captura/render Vulkan, build Android, reabertura de projeto completo e aceite
físico são evidências separadas ainda necessárias. N07 não está fechado.

## Viewer independente e superfície focal — baseline host aprovada

Os cinco cenários de materiais passaram novamente após integrar o viewer
independente (`docs/validacao/n01-n07-2026-10-10/host/material-viewer.log`).
A captura real `material-viewer.ppm.standalone.655x300.png` foi inspecionada:
área focal e controles são utilizáveis. O raster host mostra “Preparando”
porque não há Vulkan no cenário; não constitui aceite gráfico no aparelho.
O pacote posterior de biblioteca ainda requer build/execução próprios.

O código posterior permite abrir **Ver material em 3D** sem qualquer objeto
usando o material. Esfera/caixa/plano vêm das primitivas reais da Astra, com
vértices, índices, normais, tangentes e UV0/UV1. A geometria pertence à prévia,
nunca ao documento. `EditorMapScene::slotMaterial` resolve o recurso compartilhado
sem substituições locais; o snapshot imutável transporta os parâmetros efetivos.
Órbita horizontal/vertical, zoom e troca de forma invalidam a imagem e leases
anteriores, com limites de câmera. Abertura malsucedida fecha a prévia antiga.

`MaterialStandalonePreviewResources` publica buffers Vulkan próprios de geometria,
instância e extensão UV/canais. A gravação usa variantes PBR opacas/recorte/blend
existentes, descritores reais de textura e ambiente/pós da cena. O fallback de
quatro samplers recebe descritores próprios; textura de oclusão separada sem
bindless é recusada explicitamente. A entrada de extensão codificada1 aponta
para o elemento 0 do SSBO, conforme o shader real. Nenhuma miniatura é simulada.
O renderer atual espera a submissão anterior antes de substituir esses recursos;
fechar/suspender/destruir o alvo libera buffers e pools dessa prévia.

Em landscape compacto a prévia vira destino focal entre a barra superior e o
fim da superfície, substituindo temporariamente painéis e ferramentas da cena.
O viewer reserva uma faixa lateral para forma/órbita/zoom e enquadra a imagem
pela proporção do alvo. Fechar retorna ao contexto preservado. A superfície
real de 655×300 e sua roteirização por toque integram o cenário host existente;
uma captura com “Preparando” prova layout/ações, não renderização GPU.

A referência arquitetural é o editor de material da **Godot 4.5**, que possui
mundo 3D/câmera/geometria próprios, adaptada ao renderer multiview existente e à
área focal mobile da Astra:
https://github.com/godotengine/godot/blob/4.5-stable/editor/scene/material_editor_plugin.cpp.
Ambiente independente, gesto contínuo de órbita/pinça, restauração da navegação
entre sessões e renderização física ainda necessitam ampliação/aceite.

O conversor posterior deriva alfa de tokens completos `_ALPHATEST_ON`,
`_ALPHABLEND_ON`, `_ALPHAPREMULTIPLY_ON`, não do valor editorial `_Mode`.
Keywords simultâneas, `_Mode` divergente, blend/depth personalizado e premultiplicado
são recusados. Defaults de shader para SrcBlend/DstBlend/ZWrite são 1/0/1;
Fade exige 5/10/0. `_EMISSION_FAKE` não ativa emissão. Origem da decisão:
**Unity 6000.0**,
https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Editor/Mono/Inspector/StandardShaderGUI.cs.
