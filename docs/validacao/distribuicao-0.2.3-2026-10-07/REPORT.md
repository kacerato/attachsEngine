# Distribuição pública 0.2.3 (blocos G, H e I parte 1)

Pedido: atualizar o APK público e as docs com câmera virtual, mixer de áudio e Animator.

## Pacote

- Público: Astra, dev.aether.editor, 0.2.3-preview.20261007, versionCode 12, do commit
  af1d99812940b2c73564bae2c86fb83046529578 sem alterações locais (duas mudanças alheias
  ao release ficaram guardadas fora do build e voltaram depois).
- Assinatura v2 com a chave de distribuição (~/.astra/signing, senha DPAPI lida só no
  processo de build): certificado cfbd4391d3d9f63f91901dbc22886ec6544de503d7e599543c39a68facd4ef6b,
  o mesmo das prévias.
- APK 103.391.754 bytes, SHA-256 b4301d7aadb164cf38fb2ca756a0f09cbecb5ab2c3742250672f22d11306eccc.
  Biblioteca nativa d7750dd109fb2f16056dc1b3da9eff6e117df1337dfbaf2e620090c35f470ec5, idêntica
  à da instalação Astra Dev (versionCode 11) onde mixer e Animator foram aceitos.
- Exemplo embutido inalterado (U07 Laboratório, 21 arquivos); sem chaves/senhas no pacote.

## Aparelho (POCO F7, Android 16)

- `adb install -r`: versionCode 7 → 12, primeira instalação preservada (10:30:29),
  lista de projetos idêntica antes e depois.
- Shell público abre; aceite da câmera virtual executado dentro do pacote público:
  órbita/corte/desoclusão, troca com transição e lente, volta — PASS.
- Mixer de áudio e Animator: aceites no Astra Dev com a mesma biblioteca nativa
  (docs/validacao/evidencias/audio-mixer-20261007 e animator-20261007).

## Publicação

[Release pública](https://github.com/kacerato/AstraDocs/releases/tag/astra-android-2026.10.07-hi)
com APK, SHA256SUMS e manifesto; digest do asset no GitHub igual ao local. AstraDocs 850aa0f:
data/release.json, página de Download, nota distribuída 2026-10-07-apk-preview-0-2-3 e
capítulo Versões → Astra 0.2.3. Build do portal aprovado, 0 links com erro, deploy Vercel
concluído e domínio servindo a 0.2.3 (Download, /releases/latest.json e a página nova,
conferida também em 375 px sem rolagem lateral).

## Limites

Sem garantia para todo aparelho/cena e sem medição de desempenho sustentado. Áudio só WAV.
Animator sem sub-máquinas, interrupção de transição, camadas aditivas e root motion.
