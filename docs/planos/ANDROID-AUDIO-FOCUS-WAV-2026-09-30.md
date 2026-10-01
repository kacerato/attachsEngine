# Android: foco de áudio e picker WAV — 2026-09-30

O host Android inicializa o áudio Device com autorização negada. A sessão prepara vozes sem iniciar saída audível; o gate persistente de Play é liberado somente após AUDIOFOCUS_GAIN. `audioWantsFocus()` decide a demanda usando vozes reais em reprodução (sem demanda por Paused/Stopped/EOF) e os gates de pausa/background. A perda de foco de áudio não muda o estado de execução do gameplay.

Seguindo [Android Developers, Manage audio focus](https://developer.android.com/media/optimize/audio-focus), AetherActivity usa AudioFocusRequest no API26+, USAGE_GAME/CONTENT_TYPE_MUSIC, delayed gain e willPauseWhenDucked com listener na UI thread. API25 e anteriores usam requestAudioFocus(listener, STREAM_MUSIC, GAIN). Stop, onPause e onDestroy abandonam a mesma request/listener; cada geração invalida callbacks antigos. API35 pode negar foco quando o aplicativo não está no topo; essa negação conserva o gate fechado.

O JNI consulta o estado verdadeiro da plataforma:1autorizado,2delayed,0negado/liberado,-1perda. Todas as perdas, inclusive transient/duck, pausam o áudio em vez de reduzir ganho. Só gain autoriza retomar, e os gates adicionais da sessão impedem retomar em background ou Play pausado. Perda permanente não gera tentativas por quadro; novo Play ou retorno ao foreground é o próximo pedido. O polling aplica a mudança no próximo quadro nativo; não anuncia latência zero. JNI indisponível/exception conserva autorização negada.

O caminho WAV usa ação própria `consumeWaveImportRequest()` e o ModelPicker SAF existente, single-document. O picker aceita `*/*` para não ocultar WAV de provedores que o declaram octet-stream; os bytes e extensão são validados pelo decoder real, com máximo32MiB. Nenhum URI é convertido em caminho POSIX nem exige permissão geral de armazenamento.

Após validar, o host reserva `Audio/nome.wav` ou sufixo livre, aplica safePath e cria com O_EXCL para não sobrescrever fonte existente. Copia e fsync antes de chamar `importWaveClip`, que publica o registro e fonte pela transação real existente. Mudança de projeto/epoch descarta resultado. Arquivo parcial de cópia é removido; se registro falhar, fonte copiada permanece para reimportação/recuperação de journal, sem simular sucesso. Não há reprodução automática de preview.

O decoder roda na conclusão do picker e novamente na publicação existente; é trabalho limitado de importação, mas ainda pode custar uma pausa do quadro em WAV maior. Não foi movido para callback de áudio. Build Android e captura em dispositivo são validações distintas; este agente não executou compilação, teste nem ADB nesta integração. Call/duck/delayed focus exigem cenário no aparelho antes de declarar aceitação física.


Validação central: APK final compilou, incluindo foco/SAF, miniaudio e schema30; hash e contagens na auditoria consolidada. Áudio offline4/4 passou, sem prova de focus gain/loss ou som físico no dispositivo. Nenhum ADB.
