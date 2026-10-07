# Áudio completo: mixer, vozes e streaming (bloco H, F058/F059/F061)

O áudio da cena em Play continua sobre o miniaudio 0.11.23, mas o roteamento deixou de ser só uma conta de ganho. Cada Bus de áudio é um nó DSP no grafo do miniaudio (`runtime/scene_audio.cpp`), com cadeia de efeitos, envios e medidor; o DSP está em `runtime/audio_dsp.h`, testado isoladamente.

## Componentes

| Componente | Id | Referência | O que faz |
|---|---|---|---|
| Fonte de áudio | `astra.audio.source` **v2** | [Unity AudioSource](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioSource.html) | Mistura espacial contínua (0 = 2D, 1 = 3D), prioridade 0–256, pontos de loop, carregamento Memória ou Streaming |
| Bus de áudio | `astra.audio.bus` v1 | [Godot 4.5 Audio buses](https://docs.godotengine.org/en/4.5/tutorials/audio/audio_buses.html) | Nó do mixer: ganho, mute, solo, saída; método `peak_db` |
| Filtro de áudio | `astra.audio.filter` v1 | [AudioEffectFilter](https://docs.godotengine.org/en/4.5/classes/class_audioeffectfilter.html) | Biquad RBJ: passa-baixa, passa-alta, passa-banda, rejeita-banda, pico, prateleiras |
| Eco | `astra.audio.echo` v1 | [Unity Echo](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioEchoEffect.html) | Atraso de até 5 s com realimentação, mistura e original |
| Reverberação | `astra.audio.reverb` v1 | [AudioEffectReverb](https://docs.godotengine.org/en/4.5/classes/class_audioeffectreverb.html) | Freeverb: tamanho da sala, amortecimento, largura, pré-atraso, mistura |
| Compressor | `astra.audio.compressor` v1 | [AudioEffectCompressor](https://docs.godotengine.org/en/4.5/classes/class_audioeffectcompressor.html) | Limiar, razão, ataque, liberação, ganho, mistura; sidechain por outro bus (ducking); método `reduction_db` |
| Envio de áudio | `astra.audio.send` v1 | [Unity AudioMixer](https://docs.unity3d.com/6000.0/Documentation/Manual/AudioMixer.html) | Copia o sinal daquele ponto da cadeia para outro bus |
| Snapshot de mixer | `astra.audio.snapshot` v1 | [Unity Snapshots](https://docs.unity3d.com/6000.0/Documentation/Manual/AudioMixerSnapshots.html) | Até 8 valores (objeto, parâmetro, valor); `transition_to(segundos)` e `apply` |

Efeitos e envios exigem o Bus no mesmo objeto e podem se repetir. A cadeia segue a ordem dos componentes, depois do ganho do bus (como o Attenuation no topo de um grupo da Unity).

## Contrato do runtime

- **Grafo:** fonte → nó do bus → saída do bus (ou endpoint, o "Master"); envios saem por barramentos extras do mesmo nó. Saída em ciclo ou para objeto sem Bus é recusada com motivo no medidor; envio que fecharia ciclo é descartado ("Envio recusado: formaria ciclo"). O grafo só é remontado quando a estrutura muda (buses, saídas, ordem/tipo dos efeitos, destinos); valores mudam ao vivo, sem cortar caudas.
- **Solo:** passam os buses solo, os que desembocam neles, os do caminho de saída deles e os que recebem envio deles.
- **Espacialização própria:** atenuação (linear, inversa, exponencial), cone, pan pela direita do ouvinte e Doppler (OpenAL 1.1) calculados pela Astra; a mistura espacial interpola com o resultado 2D. O volume do Ouvinte é o volume global do motor.
- **Prioridade e vozes virtuais:** acima do limite (32 por padrão), as fontes de menor importância (prioridade, depois audibilidade estimada) ficam virtuais: param de tocar, mas o cursor segue o tempo e o laço; ao ganhar vaga, voltam do ponto acompanhado.
- **Pontos de loop:** início e fim no próprio data source; o laço toca a introdução e depois repete só a região.
- **Streaming:** WAV acima do orçamento de memória (32 MiB ou ~43 s decodificados) é registrado como streaming (até 1 GiB) sem reescrever o arquivo; o Play lê por `ma_decoder` em blocos. O hash é refeito só quando tamanho ou data mudam.
- **Snapshots:** cada slot vira uma transição linear em tempo real da propriedade de destino, limitada à faixa dela; "Aplicar ao iniciar" entra no primeiro quadro. O mundo de Play recebe as escritas; a autoria não muda.

Métodos novos: `is_virtual` (Fonte), `peak_db` (Bus), `reduction_db` (Compressor), `transition_to` e `apply` (Snapshot; Conexões de evento 28 e 29).

## Editor

- Cartões de estado no Inspector: Fonte (Tocando/Virtual/Pausada com cursor; memória ou streaming, 2D/3D, prioridade), Bus (medidor de pico e RMS ao vivo, efeitos, envios e saída), cada efeito com o resumo do que faz, Compressor com barra de redução, Envio com destino, Snapshot com valores e transição em andamento.
- Criar → Áudio: Bus de reverberação, Bus com ducking (abre o seletor do sidechain) e Snapshot de mixer (abre o primeiro objeto).
- Ícones novos no atlas: `audio/filter`, `audio/echo`, `audio/reverb`, `audio/compressor`, `audio/send`, `audio/snapshot`.

## Diferenças da referência

| Aspecto | Astra | Classificação |
|---|---|---|
| Snapshot guarda o mixer inteiro (Unity) | Até 8 parâmetros escolhidos; transição linear | Adaptação explícita |
| Limite de vozes em Project Settings > Audio | 32 fixo no editor; `setVoiceLimit` só no runtime | Pendente (configuração de projeto) |
| Curvas de atenuação personalizadas, Spread, Reverb Zone Mix | Três modelos fixos, sem spread | Pendente |
| Efeitos Godot: Chorus, Phaser, Distortion, EQ de bandas, Limiter, Pitch Shift, Spectrum | Não implementados (Limiter ≈ Compressor com razão alta) | Pendente |
| Formatos MP3/OGG/FLAC | Só WAV (o build compila miniaudio sem MP3/FLAC) | Pendente |
| Pré/pós-fader por envio | Envio sempre depois do ganho do bus, na posição da cadeia | Adaptação explícita |
