# Astra 0.3.2 — prévia Android (16/10/2026)

APK `Astra-0.3.2-preview.20261016-android-arm64.apk`, pacote `dev.aether.editor`,
versionCode 30, gerado da `main` no commit cddc7a45, assinado com a mesma chave das
prévias anteriores (instala por cima da 0.3.1 e preserva os projetos).

## O que entra nesta prévia

- **Animator.** Estado de Mistura 2D com **Modo 2D** (Triangulação, Direcional
  simples, Direcional livre, Cartesiano livre, como os modos 2D da Unity) e
  camada com **Sincronizar com**: copia máquinas, estados e transições da origem
  e guarda as próprias motions. Arquivo do Animator na versão 6; `.aeanimator` na
  versão 4.
- **Play sem solavancos.** O Sistema de interação (carregar) não deixa mais a
  pelve cair e voltar num quadro: o agachamento cai em rampa perto da borda do
  alcance e o corpo move no máximo 2 m/s; inclinação e agachamento medem o braço
  na pose animada. IK de dois ossos sem polo mantém o lado do cotovelo do repouso
  com o braço quase reto.
- **Clipes Humanoid da Unity mais fiéis.** Orientação do Avatar encaixada no
  eixo, quadril pelo centro de massa do próprio esqueleto, corpo normalizado pela
  pose em T: erro médio das mãos contra o projeto UMotion de 15–16 cm para
  6,7–7,2 cm. Bake Into Pose aceito; Offset de rotação da raiz aplicado.
- **Pacotes de terceiros.** POLYGON Icons (Synty) entra texturizado (o FBX que
  cita o PSD do atlas usa a primeira imagem exportada com o mesmo radical); BoZo
  2.2: rig Humanoid com redução a quatro influências e o idle convertido no
  estúdio.

## Validação

- Compilação e assinatura verificadas (certificado cfbd4391…); SHA-256 em
  `SHA256SUMS.txt`. Biblioteca nativa idêntica à do Astra Dev code 96.
- Host: nativa 1572/1572, GUI 126/126, estúdio 73/73, runtime de animação 84/84,
  Unity 24/24.
- Aparelho (POCO F7, Android 16): atualização sem desinstalar; Animator (code
  93), Play do carregar (code 95), POLYGON e BoZo (code 96) conferidos no canal
  de desenvolvimento com a mesma biblioteca nativa (docs/validacao/animator-2026-10-16,
  ik-continuidade-2026-10-16, pacotes-2026-10-16).

## Limites

- Não garante todo aparelho ou cena; sem medição sustentada de desempenho.
- Página de memória de 16 KB não suportada nesta distribuição.
- Conversão Humanoid mantém ≈5 cm de deslocamento vertical constante; Blend
  Tree aninhada e Sync timing do Animator ficam para prévias seguintes.
- Exportar APK continua experimental.
