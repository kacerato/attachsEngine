# UI, roadmap e biblioteca Synty — 03/10/2026

Checkout `C:\Users\donod\Downloads\atchengine`, remoto confirmado
`https://github.com/kacerato/attachsEngine.git`, branch `codex/gameplay-runtime`.
Alterações locais; esta rodada não fez commit/push. Mudanças anteriores preservadas.

## Implementado nesta rodada

- O Pressed preserva o alpha do fundo autorado; alpha zero não produz retângulo.
  Transparência continua independente do clique/captura. AEUI 2 já guarda ARGB;
  não houve criação de flag desconectada nem alteração da ABI 39.
- Inspector oferece **Desenhar fundo**, alpha e hexadecimal. Voltar a habilitar
  o fundo estabelece alpha 255; não memoriza a opacidade anterior.
  Hex no Inspector usa RRGGBBAA; SDK/arquivo usam AARRGGBB.
- Seletor de imagens com busca sem distinção de maiúsculas, até 4096 candidatos,
  ImGuiListClipper e janela posicionada na superfície disponível com teclado.
  A varredura continua limitada a 8192 entradas e o atlas a 64 fontes por documento.
- Biblioteca Synty completa, com fontes preservadas e conversão offline real.
  Projeto `local-assets/SyntyIconLibrary-20261003/`, disponível também no Android
  em `Projetos/SyntyIconLibrary-20261003`.

## Inventário e conversão

Fonte: POLYGON Icons Pack v1.01, `poly.unitypackage`, fornecido pelo usuário.
SHA-256: `15df14542add7105a73c94abaa2a3f6834b3b70149026b5bb182eca5f611aa0e`.

| Recurso | Extraído | Uso/limite |
|---|---:|---|
| FBX | 520 | geometria e UV convertidas para 520 GLBs |
| Prefab Unity | 520 | GUID e binding de material usados na conversão; comportamento não convertido |
| Material Unity | 17 | fontes preservadas; materiais de ícone alimentam textura do GLB |
| Texturas PNG | 16 | originais 4096² preservados; derivados 1024² para UI e GLB |
| Cena Unity | 1 | fonte preservada; não é cena nativa |
| Lighting asset Unity | 1 | fonte preservada; não é bake de iluminação nativo |
| Meta | 1075 | GUIDs e conteúdo original preservados |

São 1075 payloads originais. As prévias do pacote são opacas e ficam separadas;
os **520 sprites RGBA** usados na UI foram renderizados da geometria real.
Todos têm alpha entre 0 e 255, sem remoção artificial do fundo das prévias.
Blender oficial **4.5.0** portátil, hash do ZIP conferido contra
`tools/asset-tools.lock.json`:
`2ee75e9466d293a784fdf020f60fe1309c1e0610ecf73c64f1fc09b01e5eec56`.

O cook embute a paleta derivada 1024²; geometria, UV e ligação por GUID permanecem.
Os 520 GLBs totalizam **34.633.780 bytes**. Não impor ao aparelho a decodificação
da mesma paleta de 16 milhões de pixels em cada importação. O lote final de bake
foi concluído em aproximadamente um minuto neste host; essa medida não é SLA.
Cada saída possui provenance/hash em `bakes/`, `baked-catalog.json`,
`baked-palettes.json` e `synty-resources.json`.

Galeria: 79 nós por documento, 24 slots, **22 páginas editáveis**, primeira página
em `UI/main.aeui`; navegação por C# durante Play. Só os recursos da página ficam
no atlas. O último grupo contém os ícones 505–520. PNGs podem ser atribuídos a
Image; GLBs entram pelo importador real existente. Não são aliases de prefab.

A biblioteca e suas capturas ficam locais/ignoradas no Git público. Scripts de
extração, bake e geração são reproduzíveis e não contêm binários do pacote.

## Validação no host

- Build nativo passou; **45/45** cenários. Regressão dirigida verifica fundo
  transparente após save/read, captura no Down, desenho durante Pressed e Click
  no Up. A suíte mantém as proteções existentes de GUI/renderer compartilhado.
- **22 documentos / 520 PNGs** lidos pelo `GuiDocument`/decoder/atlas reais.
- **520/520 GLBs** lidos pelo importador nativo, com desenhos, texturas e zero
  referências de textura omitidas. Isso prova importação CPU, não upload/render
  de cada modelo 3D no Vulkan do aparelho.
- **16/16 paletas** derivadas decodificadas e colocadas no atlas, uma por vez.
  Não declarar que 16 imagens de 1024² cabem simultaneamente no atlas 2048².
- C# da galeria compilado contra o projeto atual `Astra.Scripting`: zero erros
  e zero avisos. Não usado backend simulado para afirmar navegação Android.
- Capturas do EditorSession executável mostram imagens reais, seleção e
  composição; zero comandos ImGui recusados.

Logs locais em `build/ui-roadmap-20261003/`:
`native-tests-final.log`, `native-library-cooked.log`, `native-palettes.log`,
`managed-gallery.log`, `android-build-ime.log` e `bake-cooked.log`.

## Validação no Android

Modelo **25053PC47G**, pacote `dev.aether.editor`, captura real 2772×1280.
Build Debug arm64 passou e a revisão final foi instalada. SHA-256 do APK:
`70A2A36AF9F2E90262B9BD1B7FF604DF6C2AC2F3AEBF95D3665C1DA4552D9017`.

Conferido:

1. Abrir projeto no shell e Interface; ícones transparentes em composição editável.
2. Abrir seletor: **536 imagens** (520 sprites + 16 paletas).
3. Buscar `yinyang`, confirmar pelo teclado: **1/536**; último ícone acessível.
4. Atribuir esse PNG ao primeiro Image, salvar, encerrar app e reabrir: recurso
   `images/synty/SM_Icon_YinYang_01.png` preservado e desenhado.
5. Ligar Desenhar fundo: superfície preta aparece; desligar: desaparece.
6. Restaurar o documento original da galeria; entrar em Play. Script compila
   no Android e percorre as 22 páginas; página final mostra **505–520 / 520**.

Capturas locais: `device-final-author.png`, `device-search-final.png`,
`device-reopened.png`, `device-background-on.png`, `device-background-off.png`,
`device-play-last-final.png`. Documento salvo de aceite: `device-saved.aeui`.
O PNG conceitual não foi usado como evidência; estas são capturas executáveis.

## O que continua planejado

Esta rodada elimina bloqueios reais e entrega a biblioteca; não fecha uma UI
universal completa. Trilhos/fills/checkmarks ainda precisam de skins por parte.
Joysticks autoráveis, bindings de player, motor de Cylinder dinâmico, temas,
9-slice, texto/IME como controle de jogo, listas/scroll, animação, múltiplos
canvases e acessibilidade seguem nos pacotes P1–P7.

Character não pode ser simplesmente anexado a Body/Collider no mesmo objeto:
o controlador atual possui cápsula e autoridade própria sobre a transformação.
O plano oferece Character com malha visual arbitrária ou motor sobre Body
dinâmico, com criação/conversão, referências, histórico e persistência completos.

Roadmap, referências Unity/Godot versionadas, bibliotecas, vídeos, contratos,
dependências e critérios de aceite:
[UI-ROADMAP-COMPLETO-2026-10-03](../planos/UI-ROADMAP-COMPLETO-2026-10-03.md).
