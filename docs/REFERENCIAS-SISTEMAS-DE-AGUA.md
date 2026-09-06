# Referências de sistemas de água — escopo da engine

## Fontes inspecionadas

- GodotOceanWaves, commit `a171446f8174348895aaafc426576c26261058b9`:
  https://github.com/2Retr0/GodotOceanWaves (MIT, Ethan Truong, 2024).
  Inspecionados `main.gd`, `assets/water/water.gd`,
  `assets/water/wave_cascade_parameters.gd` e README. Nenhum código desse
  repositório foi transplantado nesta etapa; a cópia de estudo fica em build.
- Projeto Unity fornecido em `Downloads/extracted/extracted`: configurações
  KWS_Settings, WaterSystem e perfil GalleonDemo.WaterSettings.asset. A propriedade
  do projeto foi declarada pelo usuário; isso não substitui a identificação de
  licenças de dependências caso código delas seja incorporado futuramente.

## Mapeamento de capacidades (não equivalência já entregue)

| Família | Referências | Destino Aether / estado |
| --- | --- | --- |
| Espectro | TMA/JONSWAP, vento, fetch, swell, spread, detalhe por cascata no Godot; FFT/domínios KWS | Provedor espectral com cascatas e seed; FFT ainda pendente. Cinco ondas analíticas atuais não equivalem a esse modelo. |
| Atualização | resolução FFT 128–1024 e frequência independente no Godot | Scheduler com timestep, recursos por cascata e budget separado; pendente. |
| Geometria | clipmap Godot, quadtree/finito/rio/custom KWS | Grade graduada camera-relative operacional; finito existente; spline/quadtree e costuras ainda pendentes. |
| Óptica | cores, turbidez, reflexão solar/aniso, refração e dispersão KWS | IOR/rugosidade/turbidez/absorção no perfil e painel; composição HDR/transmissão RGB/distorção pendentes. |
| Reflexos | SSR, preenchimento de falhas, planar e cubemap com cadência/culling | Ambiente operacional; demais passes pendentes. |
| Espuma | Jacobiano, crescimento e decaimento temporal Godot; costa/sombras/LOD KWS | Atual é máscara instantânea de crista/contato. Campo acumulado e costa são módulos distintos ainda pendentes. |
| Spray | partículas GPU distribuídas por espuma Godot | Emissor por regiões ativas para evitar desperdício de partículas; pendente, não toggle fictício. |
| Fluxo | flowmaps, fluidos locais e prebake KWS | Campo vetorial serializado + simulação local; pendente. |
| Interação | ondas dinâmicas, chuva, buoyancy KWS | Impulsos analíticos/consulta CPU operacionais; solver local e chuva pendentes. |
| Volume | caustics, depth ortho, dispersão, luz volumétrica e underwater KWS | Passes por volume com distância/resolução/cadência próprias; pendentes. |
| Ferramentas | painel por cascata, cor, FPS/frame time, câmera Godot; perfis KWS | Painel touch atual com valores numéricos e grupos; edição por cascata depende do provedor real. Inspector/NoCode devem consumir a mesma metadata, não uma segunda API. |

## Implementação desta etapa

- Seletor envia explicitamente oceano/mapa e ativa free camera somente no oceano.
  Floresta volta ao controlador first-person, que habilita joystick e HUD nativos.
- `onNewIntent` rejeita pedidos de outra cena enquanto o shell está ativo, com
  aviso e log. A seleção de assets permanece imutável, sem fingir que trocou.
  A tentativa de recriação automática foi retirada após reproduzir FORTIFY
  `pthread_mutex_lock called on a destroyed mutex` no teardown em 09:17:28.
  Troca direta a quente não está validada; o lifecycle nativo precisa de auditoria.
- `DirtRoadResources` recebe um único assetRoot; todos os meshes, texturas e
  ambientes são abertos nesse root. O APK contém ambos os demos, o que não
  significa que ambos estão residentes. Log `[SceneSelection]` indica pedido;
  `[DirtRoad] ready assets=...` confirma carregamento concluído.
- Passes e descritores de água são criados somente quando existem draws de água.
- Painel só mostra água no oceano. Quatro novos eixos possuem consumidor real,
  snapshot JNI validado e aplicação no WaterProfile: direção, rugosidade,
  turbidez e IOR. Direção gira o espectro analítico, não simula vento/fetch.
- Sliders mostram o valor real, têm área de toque de 48 dp e largura limitada
  pela tela. O painel permanece não focável para não suspender o render nativo.

O objetivo continua sendo o conjunto de sistemas acima. Esta entrega não é uma
implementação integral das duas referências, nem validação de paridade visual.

Validação em 2026-09-05: build Release passou; 348 testes nativos e sete testes
de geometria/assets passaram. A seleção da floresta pelo menu produziu
`assets=dirt_road controller=first-person hud=1` e carregamento concluído de
`dirt_road` (1029 draws, 72 texturas). Captura: `forest-hud-check.png` em
`build/android-validation`. Esses números não constituem soak de desempenho.

O carregamento do oceano confirmou root `ocean`, duas malhas de origem e uma
textura de material. O particionador produz 19 chunks totais: a descrição anterior
de um único draw não estava comprovada no runtime. A malha é contínua, mas a
submissão ainda passa pelo particionamento automático; medir e corrigir essa
política faz parte da etapa de geometria/performance.

## Continuação: particionamento e lifecycle

O particionador agora preserva o draw de `MapMaterialWaterCameraGrid` sem ordenar
ou dividir seus triângulos; água finita e opacos continuam particionados. Teste
de regressão comprova as duas políticas. Resultado Android: 4 chunks totais e
2 draws submetidos (antes 19 chunks e 17 draws), mantendo 149.504 triângulos.
349/349 testes nativos passaram e APK Release foi instalado. A execução curta
mostrou escala dinâmica 0,70; não é comparação A/B térmica nem prova de soak.

A falha de lifecycle também reproduziu ao sair normalmente do oceano e abrir
a floresta no mesmo processo: FORTIFY às 09:24:14.599. Portanto, não é exclusiva
de `recreate()`. Swappy já estava desativado pelo caminho de lançamento padrão;
não atribuir a falha a ele. Ausência de stack completa impede identificar o
dono do mutex. Reabertura/troca de cena não deve ser considerada validada até
isolar a thread/callback sobrevivente. Lançamentos em processo novo funcionaram.
