# FPS e aquecimento no editor — POCO F7 / Sponza

Data: 05/10/2026. Relato: POCO F7, menos de 50 FPS em alguns projetos, incluindo Sponza.

## Resultado após autorização de instalação e medição

O usuário autorizou explicitamente instalar e medir. O Release foi instalado
com `adb install -r` (Success), sem desinstalação nem limpeza de dados. O hash
SHA-256 do APK instalado foi lido no aparelho e corresponde ao artefato preparado.
O pacote instalado perdeu a flag DEBUGGABLE. Ao terminar a coleta, o aplicativo
foi parado para encerrar a carga e a sessão de profiling; o Release permanece
instalado para abertura normal pelo ícone.

| Medida | Debug instalado antes | Release instalado agora |
|---|---:|---:|
| FPS da engine, trecho estável | 31,31 | 69,73 |
| Duração das janelas estáveis selecionadas | 95,82 s | 215,13 s |
| CPU ativa da thread, média | 13,09 ms | 2,16 ms |
| CPU, maior p95 entre janelas | 15,21 ms | 3,20 ms |
| GPU, média | 21,75 ms | 12,37 ms |
| GPU, maior p95 entre janelas | 22,29 ms | 14,38 ms |
| Draws visíveis | 156 | 165 |
| Triângulos visíveis | 1.934.309 | 1.943.158 |
| Escala interna efetiva | 0,75 | 0,75 |

O SurfaceFlinger confirmou **31,02 FPS exibidos** no Debug em coleta contínua de
33,01 s e **66,68 FPS exibidos** no final do Release em coleta contínua de 16,75 s.
A coleta final do Release ocorreu com outro enquadramento, 217 draws e 2.470.581
triângulos visíveis. Uma primeira coleta SurfaceFlinger do Release teve lacuna
e foi preservada como inválida para continuidade, sem servir como aceite.

Os trechos estáveis são Debug janelas 2–6 e Release janelas 3–27 (600 frames
cada). As primeiras janelas foram separadas por carregamento/acomodação. No
Release, a geometria visível muda a partir da janela 28, incluindo janelas de
55–62 FPS durante a mudança; estas permanecem no JSON bruto e não foram incluídas
na média da vista estável. A coleta total Release vai aproximadamente de
12:01:47 a 12:07:30, horário de Brasília. Não é um teste de endurance de 20–30 min.

**Aquecimento não resolvido:** bateria de 39,0 °C antes do Release para 44,3 °C
ao final. Às 12:06:27 o monitor da engine passou de `none` para `light` por
headroom 0,852; o status térmico Android continuava 0. A política realmente
reduziu o filtro de sombras de 25 para 9 taps, mantendo quatro cascatas de 4096
e o piso de escala 0,75. Não se mediu potência elétrica nem se demonstrou que
o consumo do Release é menor que o Debug; o Release também produz mais frames.

O limitante passou de **CPU + GPU** no Debug para **GPU** no trecho estável
Release. Nesta vista, o custo GPU médio foi 9,37 ms nos opacos, 1,74 ms no pós
e 1,24 ms na UI. São os próximos alvos de investigação; a troca de build não
implementou uma nova otimização no renderer. A meta continua 120 Hz e não foi
atingida. Não foi alterado preset, limite de FPS ou qualidade salva.

Preservação verificada por SHA-256 antes e depois:

- `scenes/editor.aescene`: `02FA99D59FDE13BEDCD58FE5F0ABE78396AE9DCB2C4B1D61ECC24CDB90A4CD1A`.
- `.astra/rendering.astra`: `0A0E76B1456C0D2C36C7B944078073EC93810C05A4EC819A5176AA9588784928`.

O APK Debug instalado correspondia byte a byte ao APK Debug local de hash
`D900DD744E4CC65ADE6BA5EC7E5D1D28D5A5CC3292F32F0DC8B9D76984BA2D3F`.
As capturas `debug.png` e `release.png` mostram pequenas diferenças de
enquadramento entre aberturas. Portanto os resultados demonstram melhora
observada, **não um A/B causal estritamente idêntico** nem garantia para outros
projetos ou para navegação contínua.

Limitação encontrada na telemetria: `FrameProfileContext` foi emitido antes de
terminar a importação, identificando `empty-workspace` e seis instâncias; não
foi atualizado após a publicação da Sponza. Por isso não foi usado o agregador
de captura que exige contexto estável. Os registros por janela foram validados
pelo parser existente com `ExpectedInstances=411`, e a cena foi identificada
pelos logs de reabertura, hashes e capturas. Corrigir a atualização desse
contexto e a captura da câmera efetiva é necessário para um futuro A/B rigoroso.

Evidências: `results.json`, `debug-profile/`, `release-profile/`, `debug.png`
e `release.png`. As médias de CPU/GPU usam janelas do mesmo tamanho; p95 é o
maior p95 de janela, nunca uma média de percentis.

## Evidência inicial, antes da autorização

Leitura passiva de `dumpsys package`, `dumpsys battery` e registros já existentes no
`logcat` do aparelho conectado. Não houve abertura de cena, benchmark, instalação,
mudança de preset nem alteração dos projetos nesta coleta.

- Modelo ADB: `25053PC47G` / `onyx`.
- Pacote instalado: `dev.aether.editor`, versão `0.2.0-editor-ui`, versionCode 4,
  atualização em 05/10/2026 às 11:19:50; flag `DEBUGGABLE` presente.
- Às 11:46:16, o processo 25652 informou
  `[VulkanDiagnostics] validation=enabled debug_utils=enabled`.
- O build Debug local em `android/app/.cxx/Debug/1y1q5570/arm64-v8a/build.ninja`
  compila `editor_session.cpp`, `instanced_renderer.cpp` e `rhi/device.cpp` sem
  flags de otimização e sem `NDEBUG`. Algumas bibliotecas de terceiros têm `-O2`;
  isso não significa que o editor/renderer esteja otimizado.
- O mesmo processo pediu 120 Hz à Surface e 8,333 ms ao ADPF, tick fixo de 60 Hz.
  O launcher contém `aether.target_fps = 120.0f`.
- Política registrada: preset S, quatro cascatas de sombra de 4096, 25 taps,
  FXAA, bloom desligado, escala 1,00 e DRS de 0,75 a 1,00.
- Durante a abertura, antes da publicação das malhas, a DRS caiu a 0,75 com
  tempos GPU de aproximadamente 11,6–12,6 ms. Esses valores são de carregamento,
  **não** um baseline estável da Sponza.
- Depois da reabertura de `Fontes/main_sponza/NewSponza_Main_glTF_003.gltf`,
  o log mostrou 382 candidatos, 162–233 draws visíveis e 1.923.227–2.242.538
  triângulos submetidos, de 3.738.920 totais. Render interno: 960 × 2072.
- Nessas amostras, frustum culling descartou 149–220 candidatos; HZB não testou
  objetos e GPU culling estava desligado. A ausência de HZB não demonstra,
  isoladamente, que ativá-lo traria ganho.
- Sombras usaram cache nas amostras estáveis (`shadow_cascades=0/4` e hits
  crescentes). Não atribuir quatro renders de sombra a todos os frames.
- Temperatura da bateria: 39,1 °C na leitura inicial e 39,3 °C no snapshot salvo.
  Não é temperatura do SoC. Thermal API informou status 0 / pressure none nos
  registros disponíveis; **throttling térmico não foi demonstrado**.

Os arquivos `observed-logcat.txt`, `installed-package.txt` e
`battery-snapshot.txt` preservam as leituras. O relato de menos de 50 FPS é do
usuário; não foi reproduzido por benchmark nesta etapa.

## Interpretação e primeira ação

Existe custo de desenvolvimento comprovadamente ativo no APK em uso: validação
Vulkan. O build Debug local também deixa os caminhos principais sem otimização.
A contribuição de cada fator ao FPS e aquecimento ainda precisa de comparação.

Preparar `:app:assembleRelease` do checkout atual, preservando suas alterações
existentes. O Gradle usa CMake RelWithDebInfo para Release; verificar `-O2` e
`-DNDEBUG` nos caminhos reais e ausência da biblioteca de validação no APK.
Usar a assinatura local existente para permitir atualização sem desinstalar.
Não reduzir qualidade ou limitar a 60 FPS para atribuir ganho à troca de build.

## Artefato preparado

- `:app:assembleRelease --offline --max-workers=4 --console=plain` concluiu com
  sucesso em 7 min 17 s. Não foram executadas tarefas de teste.
- APK: `android/app/build/outputs/apk/release/app-release.apk`.
- SHA-256: `8F8651A0CD9758D3F02541E1CA983E4AFBC6420386208F7186C1F782256ABFA9`.
- `apksigner verify` aprovou a assinatura; certificado SHA-256:
  `9dd308380ab1606fa4e328a1d62fb9edda5abdf6cf7e5696bc8ccbf7c83bc66e`,
  igual ao certificado do APK Debug local. O APK instalado não foi extraído
  para comparar seu certificado/bytes nesta etapa.
- `aapt dump badging`: pacote e versão preservados, ARM64, sem flag
  `application-debuggable`. ZIP sem `libVkLayer_khronos_validation.so`.
- Flags confirmadas de editor, renderer e RHI: `-O2 -g -DNDEBUG`; evidência em
  `release-flags.txt`. `device.h` desabilita a validação quando `NDEBUG` existe.
- Log de build em `release-build.log`. O warning de `extractNativeLibs` e avisos
  Java de APIs obsoletas não impediram o empacotamento.
- Na etapa inicial o artefato ainda não estava instalado nem medido. A seção
  de resultado acima registra a instalação e as medições posteriores.
  Não houve alteração de código-fonte nesta investigação, apenas preparação
  do build existente e documentação da evidência. O checkout já continha
  alterações de outras tarefas; o APK usa esse estado atual.

## Protocolo proposto inicialmente e pendências de rigor

1. Confirmar que a cena está salva antes de interromper o aplicativo.
2. Coletar Debug e Release na mesma cena, pose e configurações. Identificar o
   pacote por SHA-256 e registrar estado térmico e Game Mode.
   Para atribuição exclusivamente ao tipo de build, confirmar também que os
   dois pacotes vêm do mesmo estado de código; o APK atualmente instalado
   não tem essa equivalência comprovada.
3. Separar uma comparação com resolução fixa de outra com DRS habilitada;
   aguardar importação e aquecimento dos caches antes de contar amostras.
4. Usar o FrameProfile existente para CPU ativa, esperas, GPU por passe, memória,
   draws e política, além de frames apresentados quando disponíveis.
5. Repetir com temperatura inicial comparável; depois medir execução sustentada.
   Ganho inicial não comprova estabilidade térmica.
6. Com o estágio limitante identificado, corrigir o custo correspondente:
   preparação/UI se CPU; geometria, materiais, passes ou banda se GPU.

A preferência previamente registrada do usuário exige autorização explícita para
testes e verificações de execução. Build e inspeção de artefatos são separados
de instalação, benchmark e aceite físico.

## Referências

- Roadmap fornecido: `C:/Users/donod/Downloads/Astra_Roadmap_Desempenho_Graficos.md`.
  É uma proposta de trabalho, não evidência de desempenho medido nem autorização
  automática para executar seus benchmarks ou alterar os presets.
- [Android: otimização de consumo](https://developer.android.com/games/optimize/power):
  alinhar cadência à carga sustentável e acompanhar estado térmico.
- `docs/PROFILING-ANDROID.md`: contrato de CPU ativa versus espera, GPU e memória.

Não foi implementada uma nova capacidade gráfica. A troca do APK foi concluída
e apresentou ganho observado de FPS; otimização térmica, A/B com pose idêntica
e endurance longo permanecem pendentes.
