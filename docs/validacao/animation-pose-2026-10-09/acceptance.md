# Aceite físico — Astra Dev code 28 — 2026-10-09

Base a868c8ac518a774c867a7703482391c37df3990a + alterações locais de autoria.
Repo: https://github.com/kacerato/attachsEngine.git. POCO F7, Vulkan, paisagem.
APK 0.3.1-dev.pose.20261009.1, dev.aether.editor.u07.
SHA-256 local/instalado: 9d93256d2cd27638093b2c3e1ce9dbd1c0d68a0e125b215eeebb4a4267111350.
Aplicativo público preservado. Pacotes privados não publicados.

## Fluxo observado

- Projeto AnimationStudioPacotes-20261009-v2: Kyle, Viking Idle/Walk/Run, quatro GLBs e quatro clipes editáveis. Materiais e skin visíveis.
- Auto-key desligado: entrada numérica X=0,5 criou pose temporária. SHA-256 do clipe permaneceu 26da41898aef531291485375e931e72a1ad48c617d96a455d6ca782c31ca906e. Cancelar restaurou X=0.
- Gizmo: arraste no eixo X preparou X=0,649003. Gravar publicou o recurso; Undo restaurou X=0 e Redo X=0,649003.
- Após salvar, Play, Stop, encerrar processo e reabrir: X=0,649003 e hash do clipe 0276b818d873d2f66f3c26351b48099dce4bd8cbf8e26c90cca87ed3e159d52b preservados na reabertura.
- Auto-key numérico: oito Backspaces por ADB, texto 0.25 e confirmação no botão real do teclado Android. X=0,25 gravado; Undo restaurou X=0,649003; Redo e salvar concluídos. Hash após Auto-key d4c3b4cd2d04987ae2c08a05b7187e117897dff64de7062a33479fe70e546f34. Hash após Undo diferente; não se declara reversão byte a byte.
- Tentativas anteriores device-autokey-record/ime/undo não provaram edição; usar somente capturas *confirmed para esse aceite.

## Revisão dos vídeos, quadro a quadro

Inspeção humana realizada de todas as folhas, não apenas contagem pelo decodificador.

| Arquivo | Quadros | Folhas vistas | Intervalo | Observação |
| --- | ---: | --- | --- | --- |
| gizmo-acceptance-code28.mp4 | 657 | gizmo-frames/sheet-00.png a sheet-21.png | 0–656, último PTS 9,920956 s | Eixo selecionado, arraste e pose deslocada; histórico restaura e reaplica. Sem malha quebrada observada na região dos personagens. |
| package-play-code28.mp4 | 230 | play-frames/sheet-00.png a sheet-07.png | 0–229, último PTS 3,874344 s | Kyle move o braço; Viking Walk/Run alternam pernas e escudo; Idle tem movimento sutil. Sem explosão de skin observada. |

Total: 887 quadros, 30 folhas, zero quadros omitidos na extração. frames.csv registra PTS e SHA-256 RGB24 por quadro; extraction.json registra hash do vídeo, ROI e folhas. Capturas integrais separadas conferem os controles. A revisão de ROI não afirma inspeção de cada pixel fora da região.

## Evidência separada e limites

Host final: 38/38 cenários direcionados. Integração C# nativa ABI 1–5: 1 passou, 0 falharam, 0 pulados. Build Android final passou e hash instalado corresponde.
Rotação/escala/morphs, pai girado, shear recusado e comandos SDK de pose têm evidência no host; não se declara aceite físico equivalente.
Não mede FPS sustentado, temperatura, qualidade geral da locomoção, foot sliding ou IK. Não conclui retargeting, rig completo, eventos/drivers, bake FK/IK/root motion, merge de reimportação ou biblioteca integral dos pacotes.
