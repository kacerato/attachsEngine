# Volumes, contato físico e filtragem de água

## Ownership e integração

`WaterWorld` guarda até 16 campos por valor, identificados por IDs não nulos
fornecidos pela cena. O ID não é índice de slot. Alterar/remover volumes acontece
entre jobs; consultas simultâneas são permitidas enquanto configuração e espelhos
espectrais emprestados não mudam. A cena continua responsável por manter o espelho
vivo. Não há GPU readback neste sistema.

`setVolume` valida antes de substituir. Um erro preserva o volume anterior.
Consulta sem resultado retorna volume zero, não altura zero como água implícita.
Ordem de seleção: camada compatível, cobertura molhada, maior prioridade, maior
altura, menor ID. Consultas são por coluna XZ: não representam ainda cavernas com
vários volumes verticais ou pressão hidrostática em volumes fechados.

`WaterRuntime` pertence à instância da cena física e registra até 128 corpos.
É uma camada de integração separada (`aether_water_runtime`), sem dependência
inversa do renderer sobre Jolt. Não cria corpos nem avança o mundo por conta própria.

Sequência por passo físico:

1. Atualizar os provedores espectrais para o tempo de simulação.
2. Chamar `WaterRuntime::apply(physics, water, simulationTime, settings)`.
3. Chamar `AetherPhysics_Step` com o timestep fixo da engine.
4. Consumir eventos e estatísticas; sincronizar transforms com a cena.

O tempo deve crescer estritamente: um segundo apply no mesmo tempo é recusado,
evitando duplicar forças. O caller não deve executar várias chamadas apply sem
o Step correspondente. `clear` reinicia registros e relógio para outra cena.
Ao destruir um corpo, chamar `unbind` antes; handles destruídos são detectados
pela leitura de pose com lock e contados como indisponíveis se ainda registrados.
`unbind` não sintetiza eventos. Eventos sobrevivem até o próximo apply bem-sucedido
ou clear; não guardar o span depois disso. O sistema não é thread-safe para escrita.

## Física e eventos

Consulta de todas as posições em lote, conversão para plano local, forças em lote.
Volume submerso exato de caixa/esfera contra esse plano; ondas curvas sobre um
casco grande continuam uma aproximação planar, não integração completa do casco.
Entrar e sair usam frações diferentes por histerese (2% / 0,5% por padrão).
Trocar de volume emite saída do anterior e entrada do novo, nessa ordem.
Capacidade de eventos é duas vezes a de corpos, sem descarte silencioso.

Corrente e velocidade orbital são somadas uma vez. Arrasto/empuxo consomem a
mesma consulta. Corpos em repouso continuam sujeitos à política existente de
sono: não são acordados automaticamente por mudança de corrente. Forças usam
um perfil de fluido por apply; fluidos com densidades distintas por volume,
casco composto e política explícita de despertar são trabalhos pendentes.

Nenhuma alocação é feita pelos registros/consultas/apply após construção. O
solver Jolt tem seus próprios recursos. Custo máximo da seleção é corpos ×
volumes; volume submerso é avaliado para contato e novamente para forças. Não
há promessa de custo constante ou de 120 FPS sem medição no hardware.

## Gráficos e controles efetivamente conectados

`WaterShadingSettings` possui dois eixos independentes, sem alterar o formato
serializado do WaterProfile existente:

- Specular AA: 0–1, padrão 0,5. Derivadas da normal ampliam a distribuição GGX
  quando a variação não cabe no pixel; a rugosidade autorada é o piso. Zero pula
  o filtro. Não é antialiasing de silhueta e não substitui AA temporal/MSAA.
- Espuma de contato: 0–10 m ao longo do raio de visão, padrão 1,35. Zero desliga.
  Não equivale à distância horizontal da costa nem a um solver de arrebentação.

Ambos percorrem UI Android -> snapshot JNI -> setter validado -> UBO -> shader
analítico/espectral. Nenhuma textura, descriptor ou passe novo; reutilizam dois
componentes antes reservados. O contrato binário da UBO mantém o mesmo tamanho.
A persistência desses novos eixos em recursos/Inspector não está implementada.

## Limite da entrega

Os gráficos estão ligados ao demo existente. O WaterRuntime está compilado como
módulo Android/host e é exercitado com Jolt real nos testes, mas o demo oceânico
ainda não registra corpos nele. Multiágua visual, bathymetry no shader, reflexos
HDR/refração, authoring persistente e equivalência de todos os espectros entre
CPU/GPU não estão concluídos. Não apresentar esses módulos como paridade KWS.

## Evidência local desta rodada

### Agendamento adicionado depois da rodada inicial

`WaterSimulation` oferece o caminho completo apply -> Jolt StepV2 para o dono
da cena. Possui timestep fixo de 1/240 a 1/30 s, limite de 1 a 32 subpassos,
limite de delta aceito, time scale 0–4 e pausa. Default: 60 Hz, 8 subpassos,
250 ms por frame. Relata separadamente tempo efetivamente simulado, fração
restante para interpolação e tempo descartado; não esconde a perda de tempo
quando o dispositivo não acompanha. Pausa não acumula tempo de parede.

O callback beforeStep pode atualizar os provedores para o tempo exato antes
das consultas. Se falhar, nenhuma força é aplicada e o passo pode ser repetido.
O afterStep entrega os eventos de cada subpasso, evitando perder Enter/Exit
quando um frame realiza vários passos. Erros Jolt são preservados na máscara
de retorno e exigem reset explícito; não se repete um passo já executado.
Reset remove bindings e permite trocar de mundo. Não chamar Physics_Step por
fora quando WaterSimulation controla esse mundo. Os callbacks são síncronos
no thread dono e não podem reentrar nem destruir o mundo durante advance.

Configuração de forças pode mudar por setWaterSettings entre frames; timestep
fica fixo até reset. A fração retornada não interpola transforms sozinha.
O mapa Android ainda é estático: antes de mostrar corpos, é necessário adicionar
atualização coordenada de matrizes, bounds, sombras, HZB e registros de culling.
Não foi contornado esse contrato escrevendo diretamente no buffer de instâncias.

### Caminho de atualização dinâmica adicionado em seguida

`queueMapDrawPose` agora prepara matriz de instância/normal e bounds a partir
da esfera LOCAL da malha. Aceita até 64 draws distintos por frame, coalescendo
edições do mesmo draw, sem alocar. Commit acontece depois da aquisição/fence,
antes de sombras e culling. Sombras ficam dirty; histórico HZB é invalidado
porque um occluder movido poderia esconder outros draws na posição antiga.
Draws dinâmicos deixam de usar HZB histórico individual; frustum e sombras
continuam usando os novos bounds. Isso tem custo explícito, sobretudo quando
há movimento contínuo e o cache de sombras deixa de ser reutilizável.

Transformações singulares/projetivas são rejeitadas, identidade de geometria e
material é preservada. Raio usa limite conservador válido para escala e shear.
Água e grupos com vários LODs não são aceitos neste caminho inicial. O owner
deve chamar na thread de render, sem concorrer com load/shutdown. Ainda falta
ligar entidades do demo e seus meshes a essa API; não há corpos flutuantes
visíveis adicionados por esta alteração.

Sem vetores de movimento por objeto, atualizações também invalidam o histórico
temporal de cor para evitar ghosts. Isso reduz reaproveitamento temporal durante
movimento; não equivale a suporte completo de motion vectors. A malha de colisão
estática do pacote não acompanha poses dinâmicas: corpos móveis precisam de
colliders próprios, gerenciados pela cena física.

### Teste Android autorizado

410/410 testes host e build Release passaram. APK instalado no Xiaomi 25053PC47G.
FFT confirmou três cascatas e textura de slopes RG16F. Medição PID 8349, epoch 4,
janela 1: resolução 2772x1280, escala fixa 1,0, DRS desligada, 600 frames,
48,73 FPS; GPU média 18,42 ms e p95 18,60 ms. Bateria em torno de 37 graus C.
Uma tentativa anterior manteve DRS ativa e caiu para escala 0,5; não é evidência
de desempenho nativo. O painel foi corrigido para refletir overrides de launch.
Logs: `build/android-validation/water-runtime-native.log`. Capturas confirmam
que clareamento e faixa de horizonte ainda persistem; qualidade não aprovada.

### Laboratório físico visível (incremento posterior)

O pacote oceânico agora contém três caixas de dimensões distintas, com materiais
próprios e flag NoCollision para não duplicar colliders estáticos. OceanValidation
é o owner de demonstração: cria mundo Jolt e fundo estático, registra caixas em
WaterSimulation e publica poses por queueMapDrawPose. A correspondência por
material é convenção explícita deste asset de teste, não identidade pública da
engine. Substituir por referências de entidade ao integrar o authoring de cenas.

No modo analítico, renderer e consulta usam o mesmo perfil, impulsos e relógio
fixo. O painel oferece densidade 500–2000 kg/m³ e pausa. As caixas têm a densidade
padrão Jolt de 1000 kg/m³; mudando a do fluido, flutuam ou afundam até o fundo.
O laboratório desativa sono apenas nesses três corpos para reagirem às mudanças.
Reset da surface/renderer destrói esse mundo de teste. A cena floresta não o cria.

FFT agora aciona a mesma física por um `WaterSpectralMirrorSet` de CPU. O conjunto
é construído a partir das cascatas efetivamente aceitas pelo renderer, soma as
três bandas antes de inverter o deslocamento horizontal e entrega altura,
inclinação e velocidade orbital ao `WaterField`. A resolução é adaptativa por
comprimento mínimo de onda, com teto configurável; o laboratório usa 32², 32² e
128², em vez de copiar cegamente os três campos GPU 128². Não há readback nem
latência variável entre GPU e Jolt.

O relógio fixo da física alimenta tanto o espelho quanto o relógio espectral do
renderer. Ganho, choppiness, direção, velocidade, densidade e pausa continuam
controles globais independentes. `Ocean Lab` e `Boat On Water` selecionam FFT
declarativamente no template; dispositivo sem os recursos necessários mantém o
fallback analítico. Interpolação de transforms entre passos e múltiplos pontos
de casco continuam pendentes.

- 406/406 testes nativos; 7/7 testes Python de geometria/assets.
- Shaders analítico/espectral compilados, SPIR-V validado e headers reproduzíveis.
- Android Release compilado, incluindo painel/JNI/shader atualizados.
- Convergência 60/120 testada com sono desativado explicitamente; o teste de
  repouso com política default continua passando. Não confundir tolerância de
  sono com igualdade numérica entre timesteps.
- Sem instalação, captura ou medição no aparelho nesta rodada. Qualidade visual,
  custo do filtro e FPS continuam aguardando validação autorizada no hardware.

### Ondulação bidirecional e continuidade visual (2026-09-06)

O laboratório analítico fecha agora o primeiro laço corpo -> água -> renderer.
Cada corpo submerso injeta no `WaterRippleField` uma perturbação proporcional à
velocidade vertical e ao volume molhado. O ganho desta reação é independente do
controle de interação por toque: `bodyRippleGain` percorre painel Android,
snapshot JNI e `OceanValidation`, com faixa 0–4 e padrão 0,75. Zero desliga a
reação sem desligar boiância, ondas globais ou impulsos manuais.

O campo usa 96x96 amostras em 48 m no demo, timestep fixo e borda absorvente.
`maximumAmplitude` é uma propriedade do solver (padrão global 2 m; 1,5 m no
laboratório) e limita tanto injeções quanto integração e contorno. O limite evita
que uma configuração extrema acumule energia não física sem transformar o valor
em constante de shader. A leitura fora da grade usa texel virtual zero e uma
transição cúbica nos dois últimos texels; CPU e GPU usam a mesma regra, portanto
a área móvel não desenha um retângulo no oceano.

`copyHeightsTo` publica um snapshot contíguo no SSBO de binding 15 sem executar
uma interpolação CPU por texel. O mesmo sampler GLSL compõe altura no vertex e
inclinação no fragment tanto no provedor analítico quanto no espectral. A
diferença central acompanha o maior entre célula do campo e footprint do pixel,
evitando cintilação subpixel. Os shaders de água têm módulos vertex próprios;
alterar o fragment sem regenerar o vertex não deixa mais o pipeline parcialmente
atualizado. Os headers embarcados são reproduzíveis por
`tools/generate-embedded-shaders.ps1` e validados com `spirv-val`.

O detalhe microscópico deixou de desaparecer numa distância fixa de 1.300 m.
A cadeia mip do normal map agora decide continuamente o que o pixel resolve;
`WaterProfile::microWaveStrength` continua sendo o eixo global de intensidade.
Para `MapMaterialWaterCameraGrid`, a cunha eventualmente descoberta além da
última aresta recebe a direção refletida do ambiente; superfícies finitas e
cenas sem oceano continuam amostrando o panorama completo.

Validação desta integração:

- 456/456 testes nativos no host, incluindo saturação do solver, snapshot e
  continuidade de borda;
- build Android Release (`assembleRelease`) concluído;
- APK instalado no Xiaomi 25053PC47G, 2772x1280, escala 1,0 e DRS desligada;
- provedor analítico: pico observado 0,1142 m, com decaimento para 0,0099 m,
  sem explosão, emenda retangular, VUID ou falha do processo Aether;
- provedor FFT: três cascatas, 3.342.336 bytes e slopes RG16F, inicializado e
  renderizado sem VUID;
- capturas em `build/android-validation/water-ripple-integration/analytical-final.png`
  e `spectral-final.png`.

Incremento FFT + física validado depois dessa rodada:

- 459/459 testes nativos no host;
- build Android Release concluído e APK instalado com SHA-256 host/device
  `090d9b15ecb317a3082bb7b3d2fc9f12e36746f7b4e4de72ec2d7a91d92e0d92`;
- abertura pelo ícone -> shell -> `Water Lab`, sem extras ADB, selecionou o
  provedor espectral com três cascatas e espelho físico de resolução máxima 128;
- a telemetria de 0 a 34 s registrou alturas distintas para as três caixas e
  rotação X/Z do barco, além de quatro impactos corpo -> água aceitos;
- capturas em `build/android-validation/fft-physics/11-water-final-default.png` e
  `13-water-band-controls.png` mostram física e authoring sem separação visual persistente
  entre os corpos e a superfície;
- escala nativa 1,0, DRS desligada, cadência VSYNC observada de 107,56 Hz no
  smoke test; isso não substitui uma janela determinística de benchmark.

Limite preservado: o barco ainda usa um único proxy de caixa. Plano local por
seção, forças distribuídas no casco e esteira direcional continuam pendentes.
A faixa atmosférica clara visível no horizonte pertence ao HDRI autorado e não é
uma costura da água; sua substituição por atmosfera/aerial perspective permanece
no portão da Fase 3 do plano ASTRA.
