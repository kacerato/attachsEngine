# Distribuição pública Astra 0.3.1 — 16/10/2026

Pedido: continuar a fila até o fim; o APK público entra como bloco próprio depois
das entregas de 15–16/10 (estúdio v2, Criar › Personagem, Biblioteca, correções).

## Pacote

- Público: Astra, `dev.aether.editor`, `0.3.1-preview.20261016`, versionCode 29.
  Arquivo `Astra-0.3.1-preview.20261016-android-arm64.apk`, 114.511.451 bytes,
  SHA-256 `b8897fab84ae37065a45d5512a532e42c903c27b5c2c24b70b70cacceba5a4fa`.
- Assinatura: V2, certificado `cfbd4391d3d9f63f91901dbc22886ec6544de503d7e599543c39a68facd4ef6b`
  (a mesma chave de distribuição das prévias 0.2.0 a 0.3.0).
- Fonte: `main` no commit `02744aa9e037f6e4fb519dd38b797aab409d52cc`, sem
  alterações locais (o único diff do working tree era de fim de linha; os
  arquivos do fluxo Exportar APK, antes não rastreados, foram commitados nesse
  commit porque o build.gradle.kts já dependia do módulo `:export-core`).
- Biblioteca nativa `libaether_android.so`: SHA-256
  `419711aee2a413b0ec1f5007ef01f1d9fc85bf2959fac64f9e8519336de2d92b`, idêntica à
  do Astra Dev code 92 instalado (puxado do aparelho e comparado).
- Build: `tools/release/build-public-apk.ps1` (novo no repositório; lê a senha da
  chave pelo DPAPI sem exibi-la). Badging: package `dev.aether.editor`,
  versionCode 29, label Astra, arm64-v8a, minSdk 26, targetSdk 35.

## Aparelho (POCO F7, Android 16)

- `adb install -r` sobre a 0.3.0: versionCode 28 → 29, `firstInstallTime` de
  07/10 preservado, quatro projetos preservados (teste, U07 Laboratório de
  Controle, CameraVirtual-20261007, Navegacao-20261009). Capturas `pub29_*` em
  `build/device-rig`.
- Dentro do pacote público: projeto U07 aberto e Play executado (personagem,
  raposa, joystick, HUD de posse/estado).
- Astra Dev codes 87–92 com a mesma biblioteca nativa: bateria completa do
  estúdio pela interface, Criar › Personagem, Biblioteca no trilho Projeto, Play
  do carregar com as duas mãos (`docs/validacao/estudio-v2-2026-10-16`).
- Fluxo Exportar APK (Codex, 11/10): abre em Identidade e Assinatura (criar ou
  importar chave .p12, guardar cópia, Gerar APK); não foi concluído de ponta a
  ponta no aparelho nesta rodada. Entra como experimental nas notas.

## Host

- Nativa 1571/1571, GUI 126/126, estúdio 73/73 (binários linkados com o lld).
- Suíte gerenciada não reexecutada nesta distribuição.

## Publicação

- Release `astra-android-2026.10.16` em kacerato/AstraDocs com APK, SHA256SUMS.txt
  e release-manifest.json (tamanhos conferidos pelo `gh release view`).
- AstraDocs: `data/release.json` = manifesto; nota `2026-10-16-apk-preview-0-3-1`
  (release, distribuída); capítulo `versoes/preview-0-3-1`; catálogo
  `snapshot-2026-10-16` extraído do commit do APK (95 componentes, 1403
  propriedades, 414 tipos de API; o `sync:snapshot` passou a restaurar o projeto
  C# antes de compilar, por causa do Roslyn); Download, Header, Home, PageTitle e
  Catalog apontando para o recorte novo. Estado do deploy registrado no commit das
  docs.

## Limites

- Não garante todo aparelho ou cena; sem medição sustentada de desempenho.
- Página de memória de 16 KB não suportada.
- BoZo, galeria POLYGON e Blend 2D direcional ficam para prévias seguintes.
