# Evidências de tags — 28/09/2026

Projeto isolado O1Tags0928 no dispositivo 25053PC47G, Android, landscape
2772×1280. Capturas reais da implementação; nenhuma imagem conceitual.

- `android-atribuir.png`: seletor contextual, dois objetos selecionados e
  contagem de uso na cena.
- `android-multi-undo.png`: desfazer restaura Parent sem tag e Child com Alvo;
  o Inspector mostra valor misto e refazer disponível.
- `android-multi-redo.png`: refazer aplica Alvo aos dois objetos.
- `android-play-pass.png`: execução após salvar, encerrar e reabrir o projeto.
  A fixture contém apenas objetos lógicos; o fundo sem geometria é esperado.
- `android-stop-parent.png` e `android-stop-child.png`: Stop recupera Parent
  inativo e Child com Alvo, descartando as mudanças feitas pelo script.
- `android-tags-pass.log`: somente mensagens do cenário, sem logs gerais do aparelho.
- `host-atribuir.png` e `host-projeto.png`: prévias renderizadas em 1100×510.
- Logs de testes: 128 casos nativos distintos (há filtros sobrepostos),
  26 C# e 10 Java. `host-csharp-tests.log` contém os 16 AstraBehaviorTests;
  os demais grupos C# estão nos arquivos world e archive.

Consulte [a entrega](../../planos/O1A-TAGS-2026-09-28.md) para contratos,
referências, limitações, hash do APK e próximos passos.
