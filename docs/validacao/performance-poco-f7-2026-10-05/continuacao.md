# Continuação física — POCO F7 / Sponza

## Resultado demonstrado

M7 agora existe no renderer Vulkan: a cena estática reutiliza a cor e a profundidade intermediárias já alocadas. A composição da interface e a apresentação continuam em todos os frames. Câmera, projeção, uniformes, tempo, recursos e propriedades precisam coincidir; alterações invalidam a reutilização. Play e caminhos dinâmicos incompatíveis usam a renderização completa.

O problema global de navegação em cenas pesadas permanece aberto. A economia abaixo vale para o editor com a cena parada; não representa redução universal de custo em Play ou em todos os projetos.

| Comparação no mesmo aparelho e APK | Antes | Depois | Evidência |
|---|---:|---:|---|
| GPU média, cena parada, alvo 60 FPS | 14,066 ms | 9,554 ms | `editor-scene-reuse-ab/comparison.json` |
| Potência média do aparelho, mesma cadência | 4,446 W | 2,590 W | `editor-scene-reuse-ab/power-comparison.json` |
| Economia de potência observada | — | 41,75% | A/B/B/A, bateria descarregando e sem carregador |
| Imagem A/B | referência | idêntica | Duas capturas B, comparação pixel a pixel |
| Apresentação | 59,72–59,83 FPS | 60,06 FPS | SurfaceFlinger contínuo nas quatro rodadas |

O alvo 60 é controle de comparação de energia, igual em A/B; não é alteração do alvo normal do produto. Escala 0,75, pose, conteúdo, sombras e demais opções são iguais dentro desse conjunto. Nenhum gráfico foi reduzido entre A e B. A temperatura de bateria variou entre 42,4 e 44,0 °C durante o conjunto; há inércia térmica. A queda de potência não deve ser descrita como resfriamento instantâneo.

## Comparações que não autorizaram promoção

A divisão espacial retirou 502.893 triângulos enviados (22,43%) na vista principal, mas a GPU melhorou apenas 4,41%, com 44.038.144 bytes extras de buffers e diferenças em 40 pixels por reordenação. Permanece desligada no uso normal. HZB corrigido produziu imagem idêntica, mas piorou o custo de GPU nessa vista; também permanece desligado. Dados em `spatial-geometry-ab/` e `hzb-corrected-ab/`.

As pastas `spatial-geometry-interrupted-refresh/`, `editor-scene-reuse-second-view-interrupted-quality/` e `editor-scene-reuse-second-view-interrupted-context/` preservam tentativas rejeitadas. Mudança de refresh, adaptação de qualidade e registro truncado impedem usá-las como comparações válidas. Não apagar nem misturar esses dados com rodadas aceitas.

## Instrumentação e referências

Perfetto `android.power` fornece corrente, tensão e carga pela HAL de bateria. A análise integra potência no tempo e verifica descarga/ausência de carregador. O sinal inclui tela, rádios e o resto do aparelho; não mede watts isolados do aplicativo ou da GPU. Power rails não estão disponíveis neste dispositivo. Fonte: [Perfetto — battery counters](https://perfetto.dev/docs/data-sources/battery-counters).

O diagnóstico pode fixar a qualidade autoral durante uma comparação, mantendo os sinais térmicos e a proteção do Android. Essa opção exige o profiler, não é persistida e não muda a política normal do aplicativo. O contexto completo usa `__android_log_write` para evitar o limite de formatação observado em `__android_log_print`; a pose bloqueada inicia uma nova época ao atingir a posição solicitada.

Referência arquitetural: [Godot 4.5 — SubViewport](https://docs.godotengine.org/en/4.5/classes/class_subviewport.html), separação entre atualização e apresentação do render target. Na Astra, a adaptação usa o consumidor e os recursos Vulkan reais, com invalidação e fallback, sem novo componente de autoria.

Código oficial observado: [Godot 4.5 — RendererViewport](https://github.com/godotengine/godot/blob/4.5/servers/rendering/renderer_viewport.cpp#L696), que decide a atualização por modo/visibilidade e muda `UPDATE_ONCE` para `UPDATE_DISABLED` depois do desenho. Uso real documentado: [Godot 4.5 — troubleshooting do editor](https://docs.godotengine.org/en/4.5/tutorials/troubleshooting.html), que relaciona atualização contínua a maior consumo e calor. A Astra conserva a cadência da UI e reaproveita apenas a cena, conforme seus próprios consumidores, em vez de expor um controle manual de congelamento.

## Aceite e limitações

Segunda vista: alvo 120 FPS, mesma pose e qualidade entre A/B; FPS interno 110,21→120,12, GPU 7,607→6,878 ms, imagem idêntica. Potência total 6,454→3,293 W; as cadências efetivas diferem, portanto esse valor não substitui a comparação de energia a 60 FPS. A apresentação de A foi contínua; B teve lacuna e seu FPS apresentado não é agregado. Dados em `editor-scene-reuse-second-view/`.

Interação real: mover a câmera alterou a imagem, reduziu os frames reutilizados de 600 para 547 numa janela de movimento e voltou a 600 ao parar. Desligar a iluminação mudou 432.657 pixels no recorte do viewport; restaurá-la retornou à captura original sem nenhum pixel diferente. Sponza não entrou em Play por um MeshCollider já existente sem malha de colisão no objeto raiz. Play/Stop foi verificado em `RuntimeGameplay0910`: janelas em Play com zero reutilização; após Stop, 600/600 novamente. Não houve edição da autoria para contornar o erro da Sponza. Capturas, logs e resumo em `editor-scene-reuse-dynamic/`.

Endurance de 20 minutos concluído: 119 janelas estáveis, 71.400 frames após exclusão da primeira janela, 60,056 FPS internos médios, GPU média 9,529 ms, pior p95/p99 de janela 9,733/11,422 ms. Todas as janelas estáveis reutilizaram 600/600 frames, escala 0,75 e pressão `none`. Memória GPU da engine permaneceu em 580.297.216 bytes. Potência média total de 2,925 W por 1199,80 s de trace; bateria descarregando, nenhum carregador detectado nas 185 amostras térmicas. Temperatura de bateria 44,0→42,6 °C, estabilizada em 42,6 °C ao final.

O coletor SurfaceFlinger perdeu continuidade nessa execução longa. Seus timestamps brutos são preservados, mas a média de FPS apresentado **não** é aceita nem usada como prova de continuidade por 20 minutos. Isso deixa a medição de apresentação longa parcial; a comparação A/B/B/A de mesma cadência tem as quatro rodadas contínuas. Não comparar a potência longa diretamente com uma rodada A curta como se fossem um novo A/B.

Dados em `editor-scene-reuse-soak/`; [gráfico térmico/energia](editor-scene-reuse-soak/01-B/endurance.png) e [PDF](editor-scene-reuse-soak/01-B/endurance.pdf). O sinal térmico é da bateria, não do SoC. O resultado demonstra eficiência em repouso e estabilidade do cenário testado; não garante ausência de aquecimento em navegação, temporal, água ou Play.

A reutilização estática foi promovida no código padrão após as verificações de imagem, interação, Play/Stop e consumo. Na primeira conferência do padrão, sem fixar câmera/qualidade/resolução: escala 1, sombras com 25 taps, câmera livre, reutilização habilitada e 600/600 frames nas últimas janelas; FPS interno 120,09–120,12. SurfaceFlinger confirma 120,11 FPS em aproximadamente 36,4 s contínuos registrados. O início do coletor sofreu uma falha do servidor ADB, portanto o trecho registrado não deve ser descrito como 45 s completos. Dados em `final-release/`.

O leitor de opção booleana passa a permitir um default explícito: ausência de `aether.editor_scene_reuse` habilita o padrão; `false` o desliga para isolar outras comparações. A chave `aether.disable_editor_scene_reuse` também mantém o caminho completo para A/B. Nenhuma opção de diagnóstico é decorativa. Ambas as escolhas foram verificadas no APK final: opção explícita `false` com 0/600 frames reutilizados, abertura padrão com 600/600, câmera livre e qualidade não bloqueada. Nessa abertura, a DRS existente estava em 0,95; FPS interno 120,067 e 120,095. A rodada com opção `false` usou escala 0,90, portanto **não** é apresentada como outro A/B de ganho. A economia demonstrada continua sendo a comparação controlada de mesma resolução/cadência.

APK final instalado por atualização, preservando os dados: `build/performance-poco-f7-20261005/editor-performance-final.apk`, SHA-256 `2EC3BE84C613DB18D5CD28BDF9FCB5F3E19BD441CDA450A5C244D8C9395D3ED3`. Release otimizado, arm64, não-debuggable, sem validation layer, assinado pela mesma chave local de desenvolvimento. Build, assinatura, manifest de fontes, hashes do projeto e aceites em `final-release/`. Não representa revisão Git limpa: o workspace contém alterações anteriores de outras tarefas, que não foram validadas por este trabalho. HZB e divisão espacial continuam opt-in.

No APK final, a apresentação normal foi conferida novamente: 118,530 FPS em trecho contínuo de 20,872 s, mínimo de janela móvel de um segundo de 116 FPS (`default-final-surface.json`). A cena e a configuração gráfica preservaram os SHA-256 iniciais depois das verificações. O editor foi reaberto na Sponza sem profiler nem extras de diagnóstico, pronto para uso. Esses trechos curtos não substituem o aceite de apresentação contínua durante 20 minutos.

Esta entrega fecha a melhoria de repouso estático e sua instalação; não fecha todo o roadmap. A continuidade de apresentação longa, navegação aquecida e caminhos temporais/dinâmicos precisam de seus próprios aceites. O erro de MeshCollider da Sponza continua diagnosticado, sem alteração silenciosa da autoria.

Plano: [PERFORMANCE-POCO-F7-2026-10-05.md](../../planos/PERFORMANCE-POCO-F7-2026-10-05.md). Histórico anterior: [estrutural.md](estrutural.md). O roadmap fornecido orienta dependências e prioridades; não é evidência de capacidade já implementada.

Atualização posterior, a pedido do usuário: o erro de MeshCollider da Sponza foi resolvido em uma alteração autoral explícita com player e iluminação, mantendo o backup original. O aceite também encontrou e corrigiu um erro de amostragem do render target durante a redução térmica de escala máxima. O novo APK instalado é `69074805E5BC0907CA197FDBDA7FEC6D7AF51688CE35D3C07CC07658966F1181`. O histórico acima descreve o pacote anterior; o aceite novo e seus limites de FPS/pressão ficam em [Sponza player](../sponza-player-2026-10-05/README.md). Play permanece fora da economia estática de M7.
