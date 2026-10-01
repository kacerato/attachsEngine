# Tempo de simulação — entrega ABI24

Entrega aplicada a uma parte de P06. Não fecha P06 nem P00–P20. O registro continua com **32 schemas e 31 fachadas C#**: não foi contado tipo novo pela existência de APIs ou fixtures. Relógio global, seleção de relógio de Timer/TransformTween, migrações v1→v2, interface Tempo e SDK nativo/gerenciado estão conectados. Contratos, diferenças e referências oficiais versionadas estão em [API-TIME](../../../planos/API-TIME-2026-09-30.md).

## Resultados

Build host aether_tests/aether_ui_preview passou. **13/13 testes nativos dirigidos**: clock/editor 5, Timer 2, Tween 1, ABI de path/animação/física2D 3, AudioVoice 1 e geração de fachada 1. Os logs individuais estão nesta pasta. Os casos usam GameWorld, Jolt, SceneTimers e SceneTweens reais; a captura dos callbacks gerenciados nos casos nativos usa FakeRuntime e não prova execução CLR.

**52/52 testes Astra passaram**, zero falhas ou pulados. [Log](managed-astra.txt). Build gerenciado final terminou com zero avisos e erros. TimeProbe também compilou pelo ProjectCompiler real; foi corrigido o uso de Component nullable na primeira tentativa, sem mudar o cenário de aceite. O teste antigo de animação esperava ABI18 e foi atualizado para ABI24, preservando a avaliação real de clips.

`:app:assembleDebug` final passou em 1 minuto, após os builds nativos anteriores. [Log](android-build.txt). APK: **264340670 bytes**, SHA-256 **22567EDD29C4B476045A0190659368E4C1F70B0DBB8745F95F143F1DD9F4364A**. A DLL em assets/dotnet/Astra.Scripting.dll coincide byte a byte com a saída gerenciada do build Android, SHA-256 **3ECB7308FDC572AB0E1A74E8D2D53B2DC15D5619F21B2003B70D9752523FFE11**. [Manifesto de hashes e resultados](manifest.json); write_manifest.py verifica as correspondências antes de publicá-lo.

O [projeto Time-20260930](../../../../tests/fixtures/time/README.md) foi produzido pela ferramenta de autoria, lido da publicação e reserializado com igualdade exata. A cópia de TimeProbe no projeto coincide com a fonte atual. Os dois Timers têm identidade persistente, modo e intervalo autorados. Nenhuma execução do probe no aparelho foi obtida.

## Interface executável

NÃO IREI SER SIMPLISTA NO DESIGN.

Capturas usam o renderer de UI nativo executável com rasterização de host, sem Vulkan e sem dispositivo. Não são imagens conceituais. [Tempo em execução](play-time.png) ocupa o lugar de Passo; [tablet pausado](play-time-tablet.png) restaura Passo. [Celular vertical](play-time-portrait.png) mantém Pausa/Tempo/Stop legíveis, sem reduzir o viewport para abrir outro painel. A rasterização vertical registra seis primitivas recortadas; os três controles estão íntegros, e não se atribui cada primitiva apenas pelo PNG. As demais capturas registram zero descartes, glifos ausentes e recortes.

[Timer](timer-unscaled.png) e [Tween](tween-time-mode.png) mostram a escolha de relógio nos seus grupos reais, com consumidor e API correspondentes. Reutilizam os ícones e as superfícies existentes. Nomes longos conservam truncamento contextual anterior; controle, valor e navegação permanecem visíveis. O fundo do viewport não prova renderização de cena.

## Fronteira de evidência

ADB não apresentou dispositivos na consulta final. Não houve instalação, execução CLR Android, captura Android, prova visual de shaders sob escala, audibilidade ou medição thermal. A rota independente EditorWaterPlay conserva seu relógio legado e não expõe este controle; o tempo do renderer do GameWorld foi conectado, mas não foi qualificado visualmente no aparelho. O arquivo autoral e o histórico não recebem Scale, que é estado de sessão.

As famílias de UI de jogo, animação avançada, mundo 2D visual, navegação, partículas, terreno, composição avançada de prefabs e distribuição do roadmap continuam com seus estados anteriores; estes resultados não as qualificam. Nenhuma feature ausente entrou no menu e nenhum pacote integral foi encerrado por quantidade de nomes. Alterações locais anteriores preservadas; sem commit ou push.
