# attachsEngine — containers, imagem e canvas 3D

NÃO IREI SER SIMPLISTA NO DESIGN.

## Contrato do pacote

Expandir os seis controles existentes com Image, HBox, VBox e Grid. Compor
elementos, ajustar propriedades, salvar/reabrir e consumir o mesmo documento
em tela ou num plano 3D. Os dez tipos usam o mesmo histórico e runtime; nenhuma
versão paralela do renderer, material ou scene foi criada.

## Referências e decisões

- [Unity uGUI 2.0 — Auto Layout](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/UIAutoLayout.html): separar medição mínima/preferida/expansível da distribuição feita pelo pai. Adaptado para medidas tipadas e duas passagens, com invalidação por revisão.
- [Godot 4.5-stable — BoxContainer source](https://github.com/godotengine/godot/blob/4.5-stable/scene/gui/box_container.cpp): filhos visíveis participam do fluxo; tamanho, espaçamento, alinhamento e expansão pertencem ao layout. Filhos fora do fluxo conservam anchors.
- [Godot 4.5 — TextureRect](https://docs.godotengine.org/en/4.5/classes/class_texturerect.html): separar textura de geometria e modo de ajuste. Stretch, Contain e Cover têm desenho e UV próprios.
- [Unity uGUI 2.0 — World Space](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/HOWTO-UIWorldSpace.html): resolução autoral independente do tamanho físico; posição e rotação do plano. Adaptado para um plano centrado, unidades por pixel e a câmera real da sessão.
- [Unity Learn — Worldspace UI, 2019.4.10f1](https://learn.unity.com/tutorial/creating-a-worldspace-ui?version=2019.4): criação/configuração no canvas e verificação na cena. Referência de workflow/documentação; o vídeo não foi assistido integralmente.

## Cadeia aplicada

```text
GuiWorkbench + histórico
   -> GuiDocument / GuiSizing / GuiCanvas / referência de imagem
   -> AEUI 2 (leitura também aceita AEUI 1)
   -> GuiRuntime: medir de baixo para cima, distribuir de cima para baixo
   -> UI em tela OU GuiWorldFrame: plano -> projeção homogênea
   -> UiDrawList -> UiInstanceBuilder -> Vulkan
   -> ponteiro em tela OU raio -> coordenada local -> evento -> C# ABI 39

Imagem relativa ao projeto
   -> safePath -> leitura limitada -> decoder real PNG/JPEG/KTX2
   -> atlas RGBA com gutters / revisão / erro por recurso
   -> GPU com ownership e upload após fence -> UV e tinta
```

## Implementação por pacote

| Pacote | Propriedades e comportamento | Aceite |
|---|---|---|
| HBox/VBox | mínimo, preferido, peso de expansão, padding, gap, alinhamento; ocultos saem do fluxo; fora do fluxo usa anchors; ordem Subir/Descer | aninhar, redimensionar, ocultar, reordenar e reabrir |
| Grid | colunas, linhas derivadas dos filhos visíveis, tamanho mínimo por célula, padding, gaps XY e alinhamento | trocar 3 para 2 colunas recompõe os filhos sem editar offsets |
| Image | caminho do projeto, Stretch/Contain/Cover, tinta ARGB, radius, tamanho intrínseco na medição | carregar PNG real; mudar ajuste; arquivo ausente produz erro visível, sem conservar a imagem antiga |
| Canvas 3D | destino, resolução, centro XYZ, rotação XYZ em graus, unidades/pixel, oclusão | plano inclinado com perspectiva; clique por raio; câmera ativa; salvar/reabrir |
| API | tipos novos, Sizing, Image/ImageStyle, Canvas, ordem; ABI 39 anexada depois dos callbacks anteriores | escrita tipada chega ao runtime; valores/handles inválidos recusados; autoria isolada do Play |

## UX e reversão

Canvas continua dominante. A proposta estrutural aplicada é separar
**composição 2D** de **destino do canvas**: o botão Canvas abre propriedades de
apresentação na mesma área contextual. Mundo 3D é uma configuração do documento;
entrar em Play mostra o resultado na cena. O fluxo não troca o documento por
uma ferramenta ImGui nem usa estado da janela como fonte do jogo.

Selecionar Image mostra recurso, ajuste e erro. Selecionar container mostra suas
regras; selecionar um filho controlado mostra tamanho e ordem. Anchors/arraste
ficam desabilitados enquanto o pai controla o retângulo; Fora do fluxo é uma
ação explícita. Pesos aparecem somente onde têm consumidor (HBox/VBox).
Ícones novos de composição e canvas mundial entram no atlas real do editor.
Todas as mudanças autorais, inclusive modo/transformação, passam pelo histórico.
Play conserva sua própria cópia. Arquivo inválido não substitui o documento.

## Recursos, performance e limites

Até 1024 nós, 64 imagens únicas por atlas 2048², até 1024² por imagem decodificada
e 8 MiB por fonte. PNG/JPEG e KTX2 Basis usam o decoder já existente; formatos
não suportados são recusados. Caminhos externos e symlinks são recusados.
Mtime é inspecionado a cada 300 ms ou ao mudar o documento; decodificação e
upload só ocorrem quando o conjunto de fontes muda. Layout resolve por revisão
ou tamanho do canvas; GPU recebe projeção por instância, preservando perspectiva
e clipping local sem gerar uma textura do canvas a cada frame.

O canvas mundial usa o depth buffer real da cena e não escreve profundidade.
A entrada consulta geometria do mundo atual para impedir clique através de
meshes. Recursos que não têm geometria de picking seguem a aproximação
documentada do seletor existente. Transparência da cena, objetos sem geometria
de picking e jitter temporal não equivalem a picking por pixel do framebuffer.

Esta etapa possui um documento/canvas ativo por projeto e Play. Não inclui
vários canvases, vínculo do plano a um objeto, clipping de texto multilinha,
sprites sliced/tiled, mipmaps do atlas, iluminação do plano nem exportação de
jogo. Esses limites não são apresentados como suporte implementado.

## Validação

Host: cenários integrados de layout aninhado/ordem/arquivo v1-v2, recursos/UV,
projeção perspectiva/ortográfica/inversa e ponte ABI real. SDK: sizes/offsets e
roteamento tipado. Android: build, autoria executável, imagem real, recomposição
em Play, troca Tela/Mundo 3D, interação e persistência. Capturas de software
servem para layout; capturas do aparelho são a evidência de Vulkan.

Resultados, limites e capturas: [validação executada](../validacao/GUI-EXTENSAO-2026-10-02.md).
