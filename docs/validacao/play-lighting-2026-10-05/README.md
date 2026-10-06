# Play: especializacao de iluminacao e superficies solidas

Estado: ganho aceito no POCO F7; APK final instalado e sessao normal conferida. M3/M5 e M8 continuam parciais.

O passe opaco era o maior custo restante da Sponza. A bancada separou as luzes pontuais e a consulta de sombra direcional; esses modos alteram a imagem deliberadamente e existem somente para atribuicao, com Full como modo normal. Nao foram publicados como presets ou propriedades de projeto.

A variante de producao conserva o mesmo material, BRDF, alcances, intensidade, sol, cascatas, pos-processamento e geometria. O renderer valida os registros de luzes DEPOIS da escolha do orcamento e da alocacao de tiles: somente pontos com cone exatamente unitario e sem tile local podem usar a especializacao. Uma luz spot, sombra local ou metadata desconhecida devolve o frame ao caminho geral. A selecao ocorre novamente em cada frame.

A familia ainda depende do contrato solido existente: nenhum desenho visivel com alpha mask/blend, agua, impostor ou dither de LOD. Essa comprovacao permite eliminar os caminhos de agua/impostor do programa especializado. Os shaders originais e seus descartes permanecem para os demais casos. Os programas especializado, distante e de variantes de material possuem criacao/destruicao reais; falha em criar a otimizada registra aviso e conserva a familia geral.

## Referencias e adaptacao

[Khronos Vulkan Samples, documentacao latest](https://docs.vulkan.org/samples/latest/samples/performance/specialization_constants/README.html): valores conhecidos antes da criacao da pipeline permitem eliminar programas e desenrolar lacos. [Codigo oficial do exemplo, main consultado em 2026-10-05](https://github.com/KhronosGroup/Vulkan-Samples/blob/main/samples/performance/specialization_constants/specialization_constants.cpp) compara duas pipelines na propria Sponza. Astra adapta o principio com um predicado sobre dados efetivamente publicados e fallback por frame, em vez de fixar a iluminacao da cena.

[Godot 4.5, GPU optimization](https://docs.godotengine.org/en/4.5/tutorials/performance/gpu_optimization.html): atribuicao entre geometria e shading antes da escolha da tecnica. O pacote corresponde ao recorte M3/M5 do roadmap; os modos sem luz/sombra nao contam como ganho.

## Validacao estrutural

Um teste host protege mudancas de ponto para spot, alocacao/remocao de sombra, cone invalido, NaN e uma lista em que apenas a ultima luz viola o contrato. SPIR-V bindless e fallback validados; shader geral possui tres OpKill e o solido nenhum. A variante aceita reduz o SPIR-V especializado bindless de 76.300 para 51.520 bytes e as instrucoes de amostragem comparativa de sombra de 29 para 12, mantendo o programa direcional. Sao dados do SPIR-V, nao uma captura do binario final do driver.

A primeira candidata, apenas cone/sombra local, ficou em 4,8806% de reducao media de GPU e p95 pior. Nao foi aceita isoladamente. A segunda tambem elimina os programas de agua/impostor com o mesmo contrato de elegibilidade. As duas foram comparadas separadamente com A/B/B/A; nao somar seus percentuais.

## Limites do aceite

A bancada usa Play em pose estacionaria com simulacao ativa e zero frames reutilizados do editor. A/B conserva escala 1, filtro, configuracao e mesmo APK. Os modos diagnosticos ficam desativados na sessao normal. Endurance em rota, 120 FPS e reducao de potencia/aquecimento continuam sem aceite. O aparelho permanece no carregador por escolha explicita do usuario; temperatura de bateria nao equivale a consumo da GPU.

## Resultado controlado em resolucao nativa

Mesmo APK candidato, Play, escala 1, sem DRS nem alteracao de qualidade; A/B/B/A, 30 segundos continuos de SurfaceFlinger por rodada. Janela nativa de 600 frames, 2.400 frames A e 2.400 B, todos sem reutilizacao de cena do editor. B executou a nova familia em todos os 2.400 frames; A em nenhum. Triangulos visiveis/submetidos: 2.434.559 em ambas as variantes; cena visual completa: 3.739.944.

| Medida | A: familia anterior | B: familia especializada |
|---|---:|---:|
| GPU media | 21,5681 ms | 19,3661 ms |
| Pior p95 de janela GPU | 29,1261 ms | 19,5741 ms |
| FPS do motor | 42,4091 | 46,6640 |
| FPS apresentado, rodadas | 42,05 / 42,18 | 46,30 / 46,91 |
| GPU opaco, rodadas | 17,87 / 17,68 ms | 15,59 / 15,55 ms |
| GPU pos, rodadas | 3,467 / 3,461 ms | 3,457 / 3,477 ms |

Reducao de GPU total: 10,2099%; aumento de FPS do motor: 10,0329%. Os dois PNGs B sao identicos pixel a pixel a A: zero dos 3.548.160 pixels diferentes. O buffer GPU continua em 148.038.208 bytes. RSS maximo observado A 833.167.360, B 834.547.712 bytes; alocacoes de pipelines do driver nao sao atribuidas pelo allocator da engine. P95 acima e o pior percentil de janela, nao um percentil agregado.

Bateria 44,6 -> 46,5 graus durante a sequencia inteira, AC powered true em todas as rodadas. Thermal Status 0 e IsStatusOverride false; sem desativar protecao termica. Nao atribuir essa variacao de temperatura a uma das variantes nem chamar o ganho de GPU de reducao medida de potencia. O numero de FPS e especifico da vista, desta bancada e deste aparelho; 60/120 FPS sustentados em resolucao nativa permanecem sem aceite.

Evidencia bruta: `solid-abba/`, `solid-comparison.json`, `solid-shader-validation/`, `isolation/`, `isolation-summary.json`, `first-candidate-comparison.json` e `lighting-contract.log`. Na primeira coleta diagnostica sem pontuais houve um intervalo de apresentacao de 5,55 s; ela foi repetida, com maior intervalo de 41,6 ms. Nao usar o FPS da coleta com pausa como efeito da iluminacao.

## APK final e sessao normal

Release otimizado arm64 compilado com sucesso (log `android-build.log`), instalado com `adb install -r` preservando o projeto. SHA-256 do APK salvo e do base.apk instalado: `0574357EAF9E3B4B13C555B402C15330599223BA7D257BC22EDF4A1336402902`. Os hashes da cena e da configuracao permaneceram respectivamente `10F98AC40F3F4B5CB02CFD960059498BB57FBF8828F9F3BB8AA719961ED468BC` e `0A0E76B1456C0D2C36C7B944078073EC93810C05A4EC819A5176AA9588784928`.

Sessao aberta somente com profile + autostart, sem extras de qualidade ou de variante: 61,9649 FPS apresentados em 30 s continuos, minimo por segundo 59. A politica adaptativa existente estava ativa, escala 0,75 e filtro 9; portanto este numero nao e resolucao nativa nem atribuicao isolada da otimizacao. Tres janelas de 600 frames usaram a especializacao em todos os frames, com zero reutilizacao M7; GPU de 14,4605 / 14,4421 / 14,4154 ms.

Joystick movimentou o player, gesto de olhar alterou a camera e Saltar elevou a vista; capturas reais inspecionadas. Stop retornou ao editor, com authoring preservado, e o projeto foi reaberto sem extras de bancada. Evidencias em `default-acceptance/`. Nenhum pedido de energia/potencia foi concluido como medido; carregador permaneceu conectado conforme resposta do usuario.
