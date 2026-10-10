# Revisão visual Android — 10/10/2026

Os 1.538 quadros decodificados foram inspecionados em sequência nas 52 folhas abaixo,
30 quadros consecutivos por folha (a última contém 8). Nenhum quadro do arquivo foi
pulado. A inspeção visual é do agente, separada da extração automática; o campo
review_status em extraction.json conserva o estado do extrator, não é um aceite automático.
O CSV registra PTS e SHA-256 dos bytes RGB24 de cada quadro; o vídeo original acompanha
este registro. ROI 550,235–1020,500 contém os dois painéis durante Play. A revisão
não garante cobertura de toda a superfície fora da ROI em cada quadro.

## Observações

- 0–235: logo e splash; 236–271: transição da página de projeto.
- 272: transição preta a 2,271 s. O próximo PTS é 4,265 s; há uma lacuna na captura.
  Não se afirma observar frames renderizados que o screenrecord não gravou.
- 273–650: cena em edição e espera pela compilação/início do CoreCLR.
- A partir de 651: os dois mecanismos entram em movimento. Não há geometria
  intermitente ou pose corrompida observada no trecho de execução.
- Na fase de peso zero o Animation volta à pose base e o Animator conserva a
  pose resolvida. Ambos deixam de emitir eventos, como conferido pelos callbacks.
  Não se declara que os dois avaliadores têm a mesma política visual de peso zero.
- Reinício, pausa e reverso correspondem às fases deliberadas do probe. A pausa
  é uma condição de teste, não uma falha inferida de um frame parado.
- O log confirma seis verificações, payloads e conexões filtradas. O vídeo não
  substitui essas asserções nem mede FPS sustentado, temperatura ou desempenho.

## Cobertura inspecionada

| Folha | Frames (base zero) | PTS inicial–final (s) | Revisão |
| --- | --- | --- | --- |
| [sheet-00.png](sheet-00.png) | 0–29 | 0.000000–0.232056 | Inspecionada em sequência |
| [sheet-01.png](sheet-01.png) | 30–59 | 0.240289–0.481567 | Inspecionada em sequência |
| [sheet-02.png](sheet-02.png) | 60–89 | 0.489789–0.731333 | Inspecionada em sequência |
| [sheet-03.png](sheet-03.png) | 90–119 | 0.739811–0.981211 | Inspecionada em sequência |
| [sheet-04.png](sheet-04.png) | 120–149 | 0.989556–1.233822 | Inspecionada em sequência |
| [sheet-05.png](sheet-05.png) | 150–179 | 1.241689–1.482744 | Inspecionada em sequência |
| [sheet-06.png](sheet-06.png) | 180–209 | 1.490678–1.732833 | Inspecionada em sequência |
| [sheet-07.png](sheet-07.png) | 210–239 | 1.740800–1.980789 | Inspecionada em sequência |
| [sheet-08.png](sheet-08.png) | 240–269 | 2.005111–2.246167 | Inspecionada em sequência |
| [sheet-09.png](sheet-09.png) | 270–299 | 2.254256–4.627811 | Inspecionada em sequência |
| [sheet-10.png](sheet-10.png) | 300–329 | 4.635944–4.926733 | Inspecionada em sequência |
| [sheet-11.png](sheet-11.png) | 330–359 | 4.935878–5.220822 | Inspecionada em sequência |
| [sheet-12.png](sheet-12.png) | 360–389 | 5.228533–5.523189 | Inspecionada em sequência |
| [sheet-13.png](sheet-13.png) | 390–419 | 5.646078–5.967256 | Inspecionada em sequência |
| [sheet-14.png](sheet-14.png) | 420–449 | 5.975178–6.216644 | Inspecionada em sequência |
| [sheet-15.png](sheet-15.png) | 450–479 | 6.225056–6.466544 | Inspecionada em sequência |
| [sheet-16.png](sheet-16.png) | 480–509 | 6.474889–6.716211 | Inspecionada em sequência |
| [sheet-17.png](sheet-17.png) | 510–539 | 6.724856–6.965944 | Inspecionada em sequência |
| [sheet-18.png](sheet-18.png) | 540–569 | 6.974378–7.216878 | Inspecionada em sequência |
| [sheet-19.png](sheet-19.png) | 570–599 | 7.225333–7.465589 | Inspecionada em sequência |
| [sheet-20.png](sheet-20.png) | 600–629 | 7.473733–7.715600 | Inspecionada em sequência |
| [sheet-21.png](sheet-21.png) | 630–659 | 7.723922–8.202500 | Inspecionada em sequência |
| [sheet-22.png](sheet-22.png) | 660–689 | 8.210167–8.609656 | Inspecionada em sequência |
| [sheet-23.png](sheet-23.png) | 690–719 | 8.616178–9.006400 | Inspecionada em sequência |
| [sheet-24.png](sheet-24.png) | 720–749 | 9.014378–9.315344 | Inspecionada em sequência |
| [sheet-25.png](sheet-25.png) | 750–779 | 9.335867–9.598144 | Inspecionada em sequência |
| [sheet-26.png](sheet-26.png) | 780–809 | 9.606167–9.872378 | Inspecionada em sequência |
| [sheet-27.png](sheet-27.png) | 810–839 | 9.880233–10.138511 | Inspecionada em sequência |
| [sheet-28.png](sheet-28.png) | 840–869 | 10.147833–10.388500 | Inspecionada em sequência |
| [sheet-29.png](sheet-29.png) | 870–899 | 10.397389–10.638767 | Inspecionada em sequência |
| [sheet-30.png](sheet-30.png) | 900–929 | 10.647756–10.888978 | Inspecionada em sequência |
| [sheet-31.png](sheet-31.png) | 930–959 | 10.897611–11.138322 | Inspecionada em sequência |
| [sheet-32.png](sheet-32.png) | 960–989 | 11.147278–11.389056 | Inspecionada em sequência |
| [sheet-33.png](sheet-33.png) | 990–1019 | 11.397011–11.638956 | Inspecionada em sequência |
| [sheet-34.png](sheet-34.png) | 1020–1049 | 11.647044–11.889100 | Inspecionada em sequência |
| [sheet-35.png](sheet-35.png) | 1050–1079 | 11.897144–12.137656 | Inspecionada em sequência |
| [sheet-36.png](sheet-36.png) | 1080–1109 | 12.147200–12.387322 | Inspecionada em sequência |
| [sheet-37.png](sheet-37.png) | 1110–1139 | 12.395344–12.637344 | Inspecionada em sequência |
| [sheet-38.png](sheet-38.png) | 1140–1169 | 12.646378–12.887789 | Inspecionada em sequência |
| [sheet-39.png](sheet-39.png) | 1170–1199 | 12.895967–13.135889 | Inspecionada em sequência |
| [sheet-40.png](sheet-40.png) | 1200–1229 | 13.145311–13.385189 | Inspecionada em sequência |
| [sheet-41.png](sheet-41.png) | 1230–1259 | 13.395222–13.637422 | Inspecionada em sequência |
| [sheet-42.png](sheet-42.png) | 1260–1289 | 13.645244–13.886500 | Inspecionada em sequência |
| [sheet-43.png](sheet-43.png) | 1290–1319 | 13.895244–14.136789 | Inspecionada em sequência |
| [sheet-44.png](sheet-44.png) | 1320–1349 | 14.143189–14.386911 | Inspecionada em sequência |
| [sheet-45.png](sheet-45.png) | 1350–1379 | 14.394222–14.636167 | Inspecionada em sequência |
| [sheet-46.png](sheet-46.png) | 1380–1409 | 14.643667–14.885800 | Inspecionada em sequência |
| [sheet-47.png](sheet-47.png) | 1410–1439 | 14.893944–15.137644 | Inspecionada em sequência |
| [sheet-48.png](sheet-48.png) | 1440–1469 | 15.144722–15.385567 | Inspecionada em sequência |
| [sheet-49.png](sheet-49.png) | 1470–1499 | 15.393378–15.634422 | Inspecionada em sequência |
| [sheet-50.png](sheet-50.png) | 1500–1529 | 15.643244–15.884344 | Inspecionada em sequência |
| [sheet-51.png](sheet-51.png) | 1530–1537 | 15.893111–15.951789 | Inspecionada em sequência |
