# Pós da cena do editor — revisão de 21/10/2026

Implementação local em validação. ADB suspenso por solicitação do proprietário;
não há medição móvel desta revisão. O atraso da câmera continua aberto. Reutilizar
a cena parada não demonstra desempenho durante movimento.

O editor pode conservar a imagem após o pós-processamento e antes da interface.
A interface continua sendo desenhada a cada quadro: menus, seleção, cursor e
texto não entram no cache. Play e Player conservam seu caminho de apresentação.
Os limites continuam30FPS no editor e50FPS no jogo; são tetos, não FPS garantido.

A reutilização exige a prova anterior de cena HDR inalterada. Além disso, a chave
inclui parâmetros completos do shader, uniformes, política, recursos, formato e
dimensões. Mudança de câmera, cena, efeito, recurso ou resolução impede reutilização.
Temporal AA, upscaling temporal, autoexposição e grain não usam esse cache.

A imagem tem a mesma resolução e formato do destino. A cópia Vulkan preserva os
bytes codificados, incluindo SRGB, sem filtragem ou conversão. O orçamento real de
alocação é14MiB. Resolução excessiva, uso não suportado ou falha de alocação mantém
o caminho completo. Nenhum original de textura, material, catálogo ou ícone foi
reduzido. O profiler passa a distinguir `post_scene_reused_frames` de reutilização
HDR para medir o caminho efetivamente executado.

Somente o contexto Authoring solicita transferência adicional, quando suportada.
Play/Player e consumidores genéricos conservam `clipped=true`. Trocar de contexto
negocia essa opção no ponto seguro já existente de reconstrução, liberando o
renderer antes das views, sem recriação por quadro. A captura integral
requer `clipped=false`; o custo de apresentação dessa escolha depende do aparelho
e precisa de comparação. Falha de criação opcional tenta o contrato anterior.
O código também corrige o tratamento de `oldSwapchain`: ele é aposentado mesmo
quando criar o novo falha, portanto nunca pode ser novamente adquirido como válido.
Referência: [Vulkan, VkSwapchainCreateInfoKHR, especificação consultada21/10/2026](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainCreateInfoKHR.html).

## Evidência local

- 54/54 cenários de política/frame passaram, incluindo orçamento e invalidação.
- 264 comparações Vulkan na RTX3050 passaram: pós de produção, RGBA/BGRA,
UNORM/SRGB, FXAA/AO/bloom, alteração do alpha de UI, recorte e duas dimensões.
Comparações fundido/separado e cache/interface fresca toleram até1 nível de8bits
por canal. Isso não significa alteração de assets nem equivalência bit a bit.
- Fixture usa overlay representativo com blend real; não instancia todos os
descritores/fontes/atlas/swapchain do renderer Android. Não foi executada com
camadas de validação nesta máquina.
- Revisão adicional corrigiu restore para descartar o conteúdo anterior do destino:
uma imagem recém-adquirida pode nunca ter sido apresentada. A fixture ganhou um
destino realmente novo por formato/dimensão; as8 comparações novas passaram.
- APK111.5 compilou, assinatura/alinhamento ZIP e assets foram conferidos e a
instalação autorizada passou. Médio em movimento:18.057 e17.854 FPS em duas
janelas de gestos de5s, excluindo repouso posterior; p95 intervalo83.256ms.
Snapshot p95 de29.260/27.874ms. Não comparar diretamente com a janela antiga
mista movimento+repouso. A primeira tentativa ocorreu durante carga e não produziu
amostras válidas: descartada sem FPS. A precisão dos timestamps não identifica
sozinha toques físicos: eventos injetados podem ser reamostrados pelo Android.
- Em14:03:35 o foco mudou para Astra u07; processo Terreno continuou vivo,
sem crash no buffer consultado. A guarda bloqueou gestos posteriores. Aguardar
confirmação de exclusividade antes de retomar, sem abrir/capturar o outro app.
- Não há janela concluída de600 frames do contador de cache nesta passagem.
Lifecycle completo, resize/background, fallback/OOM, ganho GPU/consumo, Play50,
Player exportado e aparência da água continuam pendentes. Não há aceite móvel.

## Medidas de texto

A lista de desenho agora possui suas métricas de fonte. Antes guardava um ponteiro
para o argumento de `begin`, enquanto `UiFont::metrics` devolve um valor: chamadas
com temporários deixavam a medição pendurada após a instrução. Cópia própria evita
alterações de quebra de linha e divergência entre desenho e alvo de toque. Os
cenários novos verificam mutação da fonte externa, cópia da lista e temporário.
Compilação/link host passaram;238/238 cenários de Editor/Play/Player/UI e239/239 de
terreno passaram, inclusive a abertura real da segunda malha antes falhando. Dez
capturas da UI executável em655×300 e1280×720 foram conferidas: três presets,
gráficos do jogo e Play sem câmera. Esses rasters verificam UI, não cena3D móvel.

Migração: nenhuma mudança de formato do projeto ou de assets. O cache é transitório.
Não distribuir o APK como correção de desempenho comprovada antes do teste móvel.
