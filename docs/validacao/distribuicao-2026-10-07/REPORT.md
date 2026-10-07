# Distribuição pública U07 e duas instalações Android

Pedido: publicar o APK final nas docs/site, manter somente público e desenvolvimento
e explicar os próximos blocos. A remoção da validação antiga foi autorizada pelo
proprietário após a proposta de guardar os projetos.

## Resultado

- Público: Astra, dev.aether.editor, 0.2.2-preview.20261007, código 7. APK idêntico
  ao já instalado/validado: 102.909.341 bytes, SHA
  61f7d8d85581dc090ad906e865b2e96c4ff974bf14cab4b5202806c0a2075936.
- Dev: Astra Dev, dev.aether.editor.u07, 0.2.2-dev.20261007, código 8. Build Release
  concluído em 1m08s, instalação Success, nome/versão conferidos no App Info real,
  shell/lista de projetos aberto, hash instalado igual ao APK:
  8aec18b0b1a0a5fec7c3fa4f829b73d941c85b9b870f918bd37deb57a2f445f1.
- As duas bibliotecas nativas têm SHA
  ca59be419bd18110bd478e3f00c5eefbbb1233168dccf2bd728d51f32b8a7175.
  A mudança nova é identificação/empacotamento; não foi repetido o aceite de
  movimento por não haver alteração do consumidor nativo. O aceite anterior
  continua histórico e vinculado aos fontes/APKs daquela revisão.
- Backup da validação antiga: 129 arquivos, 181.446.011 bytes copiados; todos os
  SHA locais iguais aos remotos. Pasta local fora do repositório:
  C:/Users/donod/Downloads/Astra-validation-backup-20261007.
  Inclui os projetos externos completos, não promete exportar preferências privadas
  inacessíveis do Android. Apenas após conferir o backup foi removido
  dev.aether.editor.validacao; o helper u07.test também foi removido.
- pm list packages confirma somente dev.aether.editor e dev.aether.editor.u07.

## Publicação

[Release pública](https://github.com/kacerato/AstraDocs/releases/tag/astra-android-2026.10.07-u07)
com APK, SHA256SUMS e manifesto sanitizado; digest do asset no GitHub igual ao local.
Tag/commit das docs ddaf80b0a8452fe41029ab68ab626b3e852e8543. Fontes privados,
evidências internas, backup e chaves não foram enviados ao portal/Release.

Download, guia U07, primeiro contato, HTML/Markdown/JSON e nota nova distribuída
foram atualizados no AstraDocs. Metadados corrigem os 8/117/200 anteriores para
12/121/527, sem reescrever notas históricas. Build aprovado, 114.598 links sem erro,
Download/guia/atualizações em 390/768/1440 px sem overflow/erro de cliente; capturas
examinadas. O estado real da produção será confirmado no domínio antes da entrega.

## Próximos blocos

1. U03/U06/restante U08: apoio/compostos, degraus/agachar/plataformas, modos físicos
   específicos, aparência/retargeting/root motion/IK e autoria de transições.
2. U10/R0–R12: UI componível, estilos/interação/layout/texto/IME/bindings/coleções,
   composição espacial, acessibilidade e exportação; roadmap original preservado.
3. Modelagem visual completa: topologia/operações/UV/materiais/normais/instâncias/
   prefab/reimportação/colisão; U11 físico não equivale a ProBuilder visual completo.
4. U12/U13/U14: SDK seguro, reprodução/rede e custo/performance/thermal medidos.

Autoria física U01/U02/U04/U05/U09 e U11 físico conservam seus aceites bounded.
Não há declaração de engine completa. Convenção de canais: android/CHANNELS.md.
