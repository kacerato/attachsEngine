# Aceite de áudio Android com autoria real

Importe astra-audio-probe.wav pelo fluxo Audio / Importar WAV (SAF). Tom original PCM16 stereo48k, 6 segundos; generate_tone.py reproduz o asset offline sem dependências. Copie AudioProbe.cs para scripts do projeto e anexe acceptance.audio a um objeto. O script não cria objetos/componentes/áudio.

Crie Source com AudioSource Enabled=true, WAV atribuído, Volume=0.5, Mute=false, Loop=false, Pitch=1 e pedido inicial Parar. Dimension2D é válido; para3D deixe Source perto do Listener e dentro min/max distance. Crie AudioListener Enabled=true, Volume=1 e objeto ativo. Bus não atribuído usa master. Salve/reabra antes de Play para provar persistência. Evite scripts concorrentes na mesma voz; autorize foco Android.

Probe procura componentes ativos na árvore. AUDIOREADY identifica source/instance/GUID/listener. Exige Playing e cursor nativo maior0.1s; pausa e verifica Paused/cursor preservado; retoma e exige progresso; para e exige Stopped/cursor aproximadamentezero. AudioVoice.Snapshot é observação real ABI23; relógio do jogo apenas espaça verificações. Playback getter nunca é prova.

AUDIOWAIT indica configuração ou saídaDevice suspensa/indisponível inclusivefoco. AUDIOFAIL encerra por exceção/erro/divergência. AUDIOPASS prova comandos/observação, não audibilidade. Ouvir o tom e registrar foco/retomada no aparelho é aceite físico separado. Seek não suportado/testado. Fixture não executada neste bloco; root controla build/ADB.
