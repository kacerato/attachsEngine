# attachsEngine: auditoria e fechamento do transporte de áudio

Repositório confirmado: https://github.com/kacerato/attachsEngine.git. Branch local `codex/gameplay-runtime`, base `97ecf56f17fe39e7d2fd527719b7fbce2528cd18`. Este incremento entrega comportamento no backend e API; não amplia o gerador nem declara a família inteira encerrada.

## Caminho real auditado

`AudioSource/AudioListener/AudioBus` → registro de componentes/propriedades → `EditorDocument`/histórico/arquivo de cena → `GameWorld` → `SceneAudio` → miniaudio 0.11.23 → PCM offline ou dispositivo.

Scripts: `AudioVoice` → `NativeBehaviorRuntime.SceneAdapter` → `ScriptSceneAccess.audioCommand` (ABI37) → `ScriptBridge` → `SceneAudio::command` → voz miniaudio. Consultas retornam pelo callback `audioSnapshot`. O Inspector existente lê `SceneAudio::Diagnostic` no `EditorSession`; não foi criado outro armazenamento de estado na UI.

Ownership: `SceneAudio` possui engine e vozes; cada voz mantém `shared_ptr` do PCM, buffer e sound. Destruição libera sound antes do buffer e libera vozes antes da engine. Remoção estrutural permanece válida até `GameWorld::flush`, depois os comandos recusam a instância antiga. O backend limita vozes a 64. Buses encadeiam ganho/mute/solo, com rejeição de ciclos/mais de 32 saídas; não são um grafo completo de efeitos.

## Lacunas corrigidas

- `Play()` antes escrevia somente o enum persistido. Repetir Playing após EOF não emitia comando novo. Agora chama o backend e reinicia o clipe, inclusive já tocando ou terminado.
- `Resume()` retoma uma voz pausada sem rebobinar. Fonte parada/terminada não começa por Resume. `Pause()` e `Stop()` têm comandos próprios; Stop solicita cursor zero.
- `Seek(seconds)` chega ao miniaudio com validação de número finito, sinal e duração do PCM. Pausa e busca podem ser combinadas. Buscar não altera o asset nem salva cursor na cena.
- A ausência de fontes ativas apagava todos os diagnósticos. Fontes existentes desativadas agora continuam consultáveis como Stopped, sem voz alocada.
- `AudioSource::read` mantinha o GUID anterior ao carregar `-` numa instância reutilizada. Agora a referência vazia substitui a anterior.

Comandos usam identidade completa: mundo, objeto, geração e instância. Argumentos inválidos retornam InvalidArgument; fonte sem voz utilizável recusa Play/Seek/Resume com ComponentUnavailable e preserva o diagnóstico específico. Pause/Stop também podem atualizar a intenção de uma fonte sem voz. Handles removidos retornam ComponentMissing; após Stop da execução retornam NotRunning.

## Contrato e compatibilidade

```csharp
using Astra.Components;

var source = Object.GetComponent<Astra.Components.AudioSource>();
if (source is { } audio) {
    var voice = new Astra.AudioVoice(audio.Component);
    voice.Play();       // reinicia
    voice.Pause();
    voice.Seek(0.5);    // precisa estar dentro da duração do clipe
    voice.Resume();    // preserva a posição
    var state = voice.Snapshot;
    voice.Stop();
}
```

`Play` em fonte pausada passa a reiniciar; código que pretendia retomar deve usar `Resume`. Os dois fixtures AudioProbe foram atualizados. A propriedade `Playback` continua sendo estado/pedido autorado, não substitui comandos repetíveis.

A ABI passa de 36 para 37, com um callback acrescentado ao final. Layouts anteriores não são reinterpretados: host e SDK precisam ser recompilados juntos. Arquivos de cena continuam na versão 1 dos componentes de áudio, sem migração. Comandos alteram a instância do mundo em Play, não gravam automaticamente o documento de autoria. Nenhum APK foi produzido nesta entrega.

Miniaudio enfileira o seek para a thread de mixagem, mas `ma_sound_get_cursor_in_pcm_frames` já retorna o destino pendente. Portanto Snapshot não certifica que aqueles frames chegaram ao dispositivo. `OutputRunning` não prova audibilidade. Os comandos sincronizam o audio world antes de operar a voz; ainda não foi medido custo de comandos em massa.

## Referências e adaptação

- [Unity 6000.0 AudioSource.Play](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AudioSource.Play.html), [UnPause](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AudioSource.UnPause.html) e [time](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AudioSource-time.html): transporte, retomada e posição são operações explícitas. Aqui Play sempre reinicia e Resume é separado; não se declara equivalência de todos os detalhes da Unity.
- [Unity 6000.0 workflow de AudioSource](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioSource.html): fonte associa um recurso e propriedades de reprodução/espaço. O workflow existente de importar WAV, vincular, Undo, salvar/reabrir e Play foi preservado e exercitado no host.
- [Manual oficial miniaudio](https://miniaud.io/docs/manual/index.html) e código vendorizado `native/third_party/miniaudio/miniaudio.h`, versão 0.11.23: `ma_sound_start/stop`, seek atômico e consulta de cursor fundamentam a implementação. Nenhuma biblioteca nova foi adicionada.

## Limites reais do fechamento

| Capacidade do atlas | Situação após este incremento |
|---|---|
| F058 Clip/Stream | Parcial: WAV decodificado, binding e limpeza persistente; faltam streaming e loop points autorados |
| F059 AudioSource | Parcial: transporte agora alcança backend; faltam prioridade/preempção e spatial blend contínuo |
| F060 Listener | Consumidor existente preservado e cenários host reexecutados; estado anterior no atlas não é nova validação física |
| F061 Mixer | Parcial: buses de ganho/mute/solo existentes; faltam sends, efeitos e snapshots de mixer |

Eventos de conclusão de voz, transporte agendado e múltiplos one-shots por fonte não foram adicionados. Não foi redesenhada a UI, não há captura nova e não houve teste de dispositivo Android. O SDK foi testado como transporte gerenciado e o callback nativo foi exercitado até o DSP real; isso não é execução integrada CLR/Android.

[Logs e acompanhamento do incremento](../validacao/evidencias/audio-transport-20261002/README.md). A contagem de famílias encerradas não aumenta artificialmente por estes comandos.
