# IDE — revisão de fidelidade visual

Pedido: comparar a primeira implementação com a proposta aprovada, corrigir composição, recortes arredondados e fundos dos PNGs. ADB autorizado. Não representa conclusão integral de M06/M11.

## Entrega

Android Debug compilado e instalado no onyx / 25053PC47G, pacote `dev.aether.editor`.

APK: `android/app/build/outputs/apk/debug/app-debug.apk`.
SHA256: `FD2D7347A01F6B213E87C9EF2051CA5E1B74CAFE59673BABCDC214E03DBC467A`.
Log da compilação: `build/m06-fidelity-build.log` — BUILD SUCCESSFUL.

- Marca/wordmark, Play sólido, abas com fechar e Add.
- Cores lexicais magenta/ciano/âmbar, linha corrente neutra.
- Seis acessórios em superfície Android separada abaixo do console; menu compacto com ações adicionais.
- Árvore com navegação nomeada, trilhos, seleção escura, Nova pasta/Add.
- Console com abas iguais, filtros semânticos, cartões, detalhe, prévia real de buffer aberto e Abrir fonte/Copiar.
- Atlas UNORM com conversão sRGB única no shader; produção dos PNGs com alpha e descontaminação do fundo antes de reduzir.

## Evidências focais

Capturas `evidencias/ide-fidelity-20260912/`:

- `codigo.png`: retrato, C# real, IME real, status e console acima da barra acessória.
- `arquivos.png`: árvore real de M06Authoring0912, navegação e ações inferiores.
- `console.png`: CS1002 real na linha 7 de Scripts/GiroM06.cs, detalhe e prévia.
- `menu.png`: menu adicional com tema próprio, recorte arredondado e tamanho compacto no APK final.

A primeira versão do menu customizado usava o token da subjanela acessória, provocando `BadTokenException`. Corrigido para o token do decor da Activity, mantendo a posição da barra. Abertura/fechamento inspecionados no APK final e buffer de crash sem nova ocorrência após a instalação final. O script de validação permaneceu salvo com o ponto e vírgula restaurado.

A remoção temporária do ponto e vírgula foi feita pelo editor visível, provocou compilação automática e a aba Problemas. Abrir fonte retornou ao código. O botão Desfazer restaurou o ponto e vírgula, confirmado pela leitura do arquivo salvo. Não foi injetado diagnóstico fictício para produzir a captura.

Inspeção dos 12 PNGs: todos RGBA, extremos alpha 0–255 e quatro cantos alpha zero. Inspeção visual no dispositivo confirmou ausência das placas pretas. As tentativas image_gen que produziram resíduos não alimentam o catálogo; a extração opt-in do fundo da folha original está em `tools/pack-icon-atlas.py`.

Não foi executada a suíte ampla. Busca, exportação e clipboard conservam os mecanismos da revisão anterior; a evidência anterior não é apresentada como repetida nesta revisão. A restauração após morte do processo e a cobertura de todos os IMEs não foram provadas.

## Comparação e limites

[Comparativo navegável](../design/m06-ide-v2/comparativo.html) e [decisão técnica](../design/m06-ide-v2/FIDELIDADE.md).

As capturas de código/árvore/console foram feitas na mesma revisão nativa; o último APK acrescenta o menu Android com tema próprio. A aparência do teclado pertence ao sistema. Tipografia, algumas silhuetas e densidade de dados reais ainda diferem do estudo; não declarar equivalência pixel a pixel. A aprovação visual pertence ao usuário.
