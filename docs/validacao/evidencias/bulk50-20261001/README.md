# Evidências do pacote 50

- `native-scenarios.log`: primeira tentativa; falhas levaram às correções de cache
  por objeto/instância e da representação de rotação.
- `native-scenarios-final.log`: 4/4 cenários completos passaram.
- `native-body-final.log`: revalidação direcionada após guard de mundo no solver.
- `managed-abi.log`: 1/1 contrato de layout/callback passou.
- `spring-response.png` / `body-simulation.png`: capturas do editor executável
  host, 1200×560; propriedades reais, grupos, fontes e ícones do atlas.
- `spring-icons-concept.png`: hipótese de ícones criada por geração de imagem.
  Os ícones implementados são os SVG/PNG/atlas do acervo `assets/astra-visual`.
- `device-scene.png`: editor real com a antiga trava de rotação de bancada ativa.
- `device-play.png` / `device-inspect.png`: Play em paisagem após remoção da trava,
  HUD de aceite. O enquadramento da câmera inicial foi incorreto; não usar essas
  capturas como prova de acabamento visual dos objetos. Estados dos três
  seguidores foram medidos pelo script real.
- `device-script.log`: READY/PASS de C# em Android, aceitando as 16 entradas novas
  e observando/modificando componentes consumidos em runtime.
- `package-manifest.json`: contagem, hashes do APK/SDK/atlas e limites da evidência.

Build Android: `build/bulk50-android-package.log`, assembleDebug bem-sucedido.
Build do SDK e probe: zero avisos/erros. Nenhuma suíte completa foi executada.
A ida e volta de orientação da IDE ficou sem aceite físico por solicitação do
usuário para encerrar a bancada; implementação compilada é diferente de prova
no aparelho. O projeto de aceite está editável em `build/acceptance/Bulk50-20261001`.
