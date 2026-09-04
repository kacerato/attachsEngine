# Visibilidade estável e água nativa

## Contrato de entrega

Trabalho em andamento. Compilação e testes de host não substituem validação
visual no aparelho. A captura `build/android-validation/aether-current-artifact.png`
é a referência do defeito de folhagem. Não declarar paridade com Unity nem
120 FPS sustentados antes de testes em movimento, em escala equivalente e soak.

## Ordem e gates

1. Corrigir LOD em pixels da saída, cobertura e bounds animados. Preservar
   qualidade próxima; frustum/HZB removem somente geometria comprovadamente invisível.
2. Fechar baker multivista, normais, material, UVs, mips e compatibilidade legada.
   Testar cores contra o GLB e culling após o processamento de chunks.
3. Validar transições de sombra e material em aproximação, afastamento, rotação,
   mudança de escala e pressão térmica. Sem sumiço de casters por LOD da câmera.
4. Água nativa em passes explícitos, configurações serializáveis, lifecycle,
   orçamento e capacidades. Sem UnityEngine no runtime e sem copiar código proprietário.
5. Cena oceânica Aether com câmera de voo por gestos, sem joystick, inicialmente
   independente da floresta; depois retomar as medições do renderer completo.

## Pacote inspecionado

`C:/Users/donod/Downloads/Assets/Assets/KriptoFX/WaterSystem/WaterResources`
identifica versão 1.4.03 no README. Há `SimpleDemo.unity`; a documentação requer
espaço linear, Cinemachine e Post Processing. Scripts usam UnityEngine,
ScriptableObject e CommandBuffer; shaders usam o pipeline Unity. Não há
compatibilidade binária nem importador desses componentes na Aether.
O proprietário informou em 04/09/2026 que detém a licença do pacote e autorizou
seu uso no projeto; licença não é bloqueio desta tarefa. A incompatibilidade
técnica permanece: a integração Aether será nativa e data-driven, sem carregar
tipos UnityEngine no runtime.

## Matriz funcional da água (pendente, não implementada)

| Área | Recursos observados | Integração Aether necessária |
| --- | --- | --- |
| Superfície | FFT, vento, turbulência, tempo | simulação, espectro, normais e deslocamento, fallback por capacidade |
| Malha | oceano infinito/finito, quadtree, spline de rio, tessellation opcional | patches com costura e bounds conservadores, sem exigir tessellation mobile |
| Óptica | transparência, turbidez, cor, IOR, dispersão | depth/color read-only, absorção e refração com bordas seguras |
| Reflexos | SSR, planar, cubemap, anisotropia, sol | passes opcionais, fallback fora da tela, cache e cadência |
| Costa | ondas de praia, espuma, espuma particulada | recursos de costa, profundidade, transições por cobertura |
| Interação | flowmap, fluido 2D, ondas dinâmicas, chuva | domínio local limitado, timestep independente do render |
| Luz | cáusticas, sombras, iluminação volumétrica | passes com resolução/custo próprios e recursos de profundidade |
| Subaquático | detecção, volume, distorção/blur | transição da superfície, lifecycle e ordenação |
| Física | consulta de altura/normal, flutuação | amostragem com validade explícita e integração desacoplada |
| Ferramentas | edição de flowmap/costa/rios, salvar dados | recursos versionados e propriedades para futuro Inspector |
| Máscaras | recortes, buracos de barco, camadas | volumes de exclusão, sem lógica específica de jogo |

Todos os eixos devem distinguir solicitado, resolvido, indisponível e fallback.
Uma flag sem consumidor real não conta como funcionalidade implementada.
Importar a demo Unity exigiria mapear GUIDs, transforms, recursos e cada componente;
componentes desconhecidos devem gerar diagnóstico, nunca desaparecer silenciosamente.

## Medição existente

Release com impostor antigo: 117,06 presents/s a escala 0,55.
Release sem impostor: 120,06 presents/s, escala mínima 0,50.
Essas execuções **não isolam** custo de impostor porque a resolução variou.
GPU p95 de 7,279 ms corresponde a 99,26% do orçamento GPU de 7,333 ms,
não do intervalo total de frame de 8,333 ms. Não extrapolar esse teste curto
de pose fixa para movimento ou estabilidade térmica.

## Alterações desta etapa (04/09/2026)

- Seleção de LOD mede erro na altura da saída, não na resolução interna dinâmica.
  A resolução pode variar sem adiantar as trocas de geometria.
- Defaults de coverage S/A/B/C: 2/3/4/6 pixels de saída; overrides continuam
  independentes. Isso privilegia estabilidade visual e pode aumentar custo: não
  interpretar como ganho de FPS antes da medição.
- Bounds de geometria billboard incluem todas as rotações do shader, escala
  não uniforme e shear, inclusive depois do processamento em chunks.
- A próxima cascata cobre explicitamente a faixa onde o shader começa a
  misturá-la. Limites nominais permanecem iguais para seleção e fade.
- Pressão térmica preserva por padrão limites de LOD, alcance material e horizonte
  da sombra. Pode reduzir filtragem/resolução, mas não desliga toda sombra.
  `ProjectRenderingSettings.thermalDistanceScaling` permite optar pela adaptação
  antiga; no Android, `aether.thermal_distance_scaling=true`. Ainda não há UI.
- Bake multivista: 175 grupos, oito azimutes, color atlas e normal atlas separados.
  Cor considera vertex color e fator do material em espaço linear. Normais são
  geométricas em espaço de mundo; normal maps de detalhe não estão incorporados.
  Ângulo usa uma amostra por pixel com seleção Bayer, compartilhada com coverage.
  Mips são reduzidos por vista; footprints ficam dentro da vista residente.
- Metadados zero mantêm compatibilidade com impostores legados. O layout novo
  exige dimensões válidas e recurso de normal antes de chegar à GPU.
- `--replace-baked` remove apenas o sufixo de geometria/material pertencente ao
  baker e preserva os níveis fonte. Backup anterior em
  `build/foliage-before-multiview-20260904-171050`.
- Runner Android herda limites de LOD por padrão; antes `-EnableLod` restaurava
  silenciosamente coverage=32. Agora os relatórios podem testar a política real.
- A captura física `feedback-20260904-1748.png` demonstrou que a mancha branca
  também atinge árvores próximas. Isolamento no mesmo ponto confirmou origem na
  iluminação, não no albedo nem exclusivamente no impostor. O shader aplicava
  `specularFactor=0` somente em F0, mantendo F90 branco; agora o peso controla
  toda a reflexão dielétrica e o split-sum IBL não aplica Fresnel/peso duas vezes.
- O relatório detalhado de ADPF agora informa `CPU + GPU` como trabalho total do
  ciclo, mantendo as parcelas separadas. Antes usava o maior valor e escondia o
  custo da render thread do governador. O alvo continua sendo 8,333 ms em 120 Hz;
  não há solicitação privada de clock nem redução visual implícita.

Validação de host: 334 testes nativos, 16 testes de assets, três testes do gerador
de rota e 52 testes de FrameProfile passaram. Shaders recompilados e validados.
`tools/generate-camera-sweep.py` gera rotas AERT v1 fechadas, vinculadas ao hash do
pacote, para comparar aproximação/afastamento sem depender de gestos manuais.

Release compilado com JDK 21 (a instalação local do JDK 17 está incompleta;
JBR 25 não é aceito pelo Gradle atual). APK assinado e assinatura verificada:
`build/render-rebuild-multiview-release.apk`, SHA-256
`83C2B25FC213D56DA50025C0C86799294E13268F4AD6A77205054D98BF3C9331`.
Água permanece pendente na matriz acima, não representada por flags sem consumidor.

### Prova física do primeiro APK multivista

No Xiaomi SM8735, em escala fixa 0,55 e alvo 120 Hz:

| Captura | FPS exibido (SurfaceFlinger) | Presents/s da engine | GPU média | Pior p95 de janela |
| --- | ---: | ---: | ---: | ---: |
| Pose fixa | 111,80 | 112,72 | 7,583 ms | 7,766 ms |
| Rota de distância | 105,45 | 107,34 | 7,850 ms | 10,777 ms |

Evidências: `build/android-validation/multiview-fixed055` e
`build/android-validation/multiview-distance-sweep055`. Rota de 2400 frames,
do hotspot até (-35,145.27,25), retornando suavemente. Sem aviso térmico reportado;
isso não comprova ausência de DVFS. Câmeras, escala e erros de LOD foram confirmados
na telemetria. CPU média ~1,3–1,4 ms: déficit permanece principalmente na GPU.
Sem crash observado nos testes; este APK Release não substitui validação Khronos.

As primeiras capturas não enquadraram as manchas brancas, mas a captura posterior
na posição indicada pelo usuário as reproduziu. A correção especular exige nova
captura e rota no APK atualizado antes de ser considerada aprovada. O usuário
confirmou visualmente no aparelho que o branco foi corrigido e que, com clock
normal, a cena manteve 120 FPS estáveis.
**Meta de 120 FPS sustentados ainda não cumprida.** Não usar o status `passed`
do runner (lifecycle/telemetria) como se fosse aprovação do orçamento de 120 Hz.

Próxima otimização em validação: preferir também superfícies RGBA sRGB, além de
BGRA sRGB, para usar transferência de saída por hardware quando anunciada.
Fallback UNORM e codificação explícita continuam disponíveis. Ganho não presumido.
