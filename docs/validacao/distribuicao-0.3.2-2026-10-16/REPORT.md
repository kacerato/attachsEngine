# Distribuição pública Astra 0.3.2 — 16/10/2026

Pedido: fechar a fila (Humanoid, Animator 2D + camadas sincronizadas, BoZo, POLYGON,
APK público). O público entra como bloco próprio depois das entregas do dia: Animator
(code 93), continuidade do IK em Play (code 95), pacotes POLYGON e BoZo (code 96).

## Pacote

- Público: Astra, `dev.aether.editor`, `0.3.2-preview.20261016`, versionCode 30.
  Arquivo `Astra-0.3.2-preview.20261016-android-arm64.apk`, 114.520.719 bytes,
  SHA-256 `769f5bb57edd4fce8d2055d4fa1376b004466b4f683da76bcb02cc665de85b50`.
- Assinatura: V2, certificado `cfbd4391d3d9f63f91901dbc22886ec6544de503d7e599543c39a68facd4ef6b`
  (a mesma chave das prévias 0.2.0 a 0.3.1).
- Fonte: `main` no commit `cddc7a4599ae434e32e8aeca39f7eb4a98b32799`, sem alterações
  locais no que entra no pacote (o working tree só tinha diferenças de fim de linha em
  `native/editor/*` e arquivos de documentação não rastreados).
- Biblioteca nativa `libaether_android.so`: SHA-256
  `2102da62e20e0cf4dc5182138d56ea840a9650f931f8e13f20482a967ddae99d`, idêntica à do
  Astra Dev code 96 (mesmo build do gradle; o script público só reempacotou e assinou).
- Build: `tools/release/build-public-apk.ps1 -VersionCode 30 -VersionName 0.3.2-preview.20261016`
  (senha pelo DPAPI, nunca exibida). Badging: package `dev.aether.editor`, versionCode 30,
  label Astra, arm64-v8a, minSdk 26, targetSdk 35.

## Aparelho (POCO F7, Android 16)

- `adb install -r` sobre a 0.3.1: versionCode 29 → 30, `firstInstallTime` de 07/10
  preservado, quatro projetos preservados (teste, U07 Laboratório de Controle,
  CameraVirtual-20261007, Navegacao-20261009). `aparelho/pub30-projetos.png`.
- Dentro do pacote público: U07 aberto e Play executado (personagem, raposa, joystick,
  HUD de posse/estado). `aparelho/pub30-u07-play.png`.
- Astra Dev codes 93–96 com a mesma biblioteca nativa:
  `docs/validacao/animator-2026-10-16` (Modo 2D e Sincronizar com pela UI),
  `docs/validacao/ik-continuidade-2026-10-16` (Play do carregar sem pelve oscilando nem
  cotovelo virando; projeto do usuário limpo das sobras de receita),
  `docs/validacao/pacotes-2026-10-16` (POLYGON texturizado na cena, BoZo BodyRig com
  o idle Humanoid no estúdio).

## Host

- Nativa 1572/1572, GUI 126/126, estúdio 73/73, runtime de animação 84/84 (dois
  cenários novos de IK), Unity 24/24 (BoZo e Kyle com verdade de referência UMotion).
- Suíte gerenciada não reexecutada nesta distribuição.

## Publicação

- Release `astra-android-2026.10.16-r2` em kacerato/AstraDocs com APK, SHA256SUMS.txt
  e release-manifest.json (tamanhos conferidos pelo `gh release view`).
- AstraDocs (commit 574d61a): `data/release.json` = manifesto; nota
  `2026-10-16-apk-preview-0-3-2` (release, distribuída); capítulo
  `versoes/preview-0-3-2`; catálogo `snapshot-2026-10-16b` extraído do commit do APK
  (95 componentes, 1403 propriedades, 414 tipos de API). Segunda release no mesmo dia
  ganhou rota própria com sufixo de letra: `sync-snapshot.mjs` e `export-api/Program.cs`
  passaram a aceitar `snapshot-AAAA-MM-DD[a-z]`. Notas do dia já no feed público:
  Animator, IK, Humanoid, pacotes; a nota da release e a página Download trocam para
  0.3.2 quando o deploy da Vercel terminar (conferir em `/updates/feed.json` e
  `/download/`).

## Limites

- Não garante todo aparelho ou cena; sem medição sustentada de desempenho.
- Página de memória de 16 KB não suportada.
- Conversão Humanoid mantém ≈5 cm de deslocamento vertical constante contra o UMotion;
  Blend Tree aninhada e Sync timing do Animator ficam para prévias seguintes.
- Exportar APK continua experimental.
