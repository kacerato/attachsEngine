# Play da Sponza: sombreamento opaco, 2026-10-05

Implementação M3/M5 do roadmap, com ganho real de GPU na câmera inicial do player. Não altera a cena, a configuração do projeto, resolução de produção, geometria, normal maps, iluminação, sombras, pós ou a cadência de simulação. Não é aceite de 120 FPS nem conclusão do trabalho térmico.

## Diagnóstico e decisão

No POCO F7, isolamentos diagnósticos intercalados (Full / sem normal / sem IBL especular / base color / Full), sempre em Play na mesma pose e extensão interna, deram GPU média de 18,34 / 16,41 / 18,46 / 9,60 / 18,50 ms. BaseColorOnly conserva geometria/cobertura e evita PBR/iluminação; não isola perfeitamente custo de vértices e não é modo de produção. O resultado aponta custo relevante de fragmentos/materiais. A remoção de IBL não ajudou nesta vista. Arquivos reais em `isolation/`.

O shader compartilhado continha `discard` para alpha mask, blend e cross-fade mesmo nos sólidos que não utilizavam nenhum deles. A implementação cria uma especialização não recortável, compartilhando os shaders, descritores, materiais, buffers e render pass existentes. O driver pode eliminar esses testes antes de compilar o shader. Não força `early_fragment_tests` sobre shaders que ainda descartam pixels.

A seleção consulta materiais efetivos (incluindo overrides autorais) e o registro de instância atual. Dither diferente de zero, blend, alpha mask, água ou impostor mantêm toda a fase sólida na família original. Isso conserva a ordem front-to-back e a mesma submissão direta/indireta; transições de LOD voltam automaticamente ao caminho com recorte. As famílias de cobertura e transparência permanecem com seu contrato original. Pipelines especiais e variantes usadas pelo pacote são destruídos/recriados junto ao renderer; não há novo render target.

A chave diagnóstica `aether.opaque_no_clip=false` preserva o baseline no mesmo APK. O padrão final é ativado. `opaque_no_clip` no contexto identifica a família; `opaque_no_clip_frames` nas janelas do APK final comprova quantos frames realmente a usaram, excluindo a reutilização estática do editor.

## Comparação inicial A/B/B/A

APK da candidata: SHA-256 `6E8563BE8A6B78AA1F3CC93F4340FA16A937DAAD91E25F2B4FAFB7965451F250`. Quatro capturas SurfaceFlinger de 30 segundos, todas contínuas. Escala interna fixa 0,75 (960×2079 com transformação da superfície para 2772×1280), GPU completa, mesma câmera inicial, 2,44 milhões de triângulos submetidos no contexto, filtro de sombras solicitado 25, sem M7 em Play. Esta extensão é uma condição compartilhada de bancada, não uma redução aplicada às configurações do projeto. O bloqueio diagnóstico de qualidade não desativa a proteção térmica do Android.

| Medida | A: original | B: sólidos sem recorte |
| --- | ---: | ---: |
| GPU média | 18,5769 ms | 16,0166 ms |
| Pior p95 de janela GPU | 19,4691 ms | 16,6480 ms |
| FPS apresentados, rodada 1/2 | 47,98 / 48,24 | 56,97 / 56,48 |
| Frames nativos coletados | 3.600 | 4.800 |
| Pixels diferentes nas duas capturas B | — | 0 de 3.548.160 |

Redução de GPU de 13,7823%; aumento de FPS interno de 17,0265%. Os percentis são o pior percentil de janela, não percentis agregados. Quantidades distintas de janelas resultam de frames mais rápidos na mesma duração de captura. Logs/contextos/janelas/SurfaceFlinger em `opaque-abba/`; resultados em `opaque-comparison.json`; a captura `opaque.png` é idêntica às capturas A e B originais.

O ADB reiniciou durante a segunda rodada; a captura manteve sobreposição dos timestamps e continuidade. O evento não é escondido: o extrato da saída está preservado em `adb-event.txt`. O teste não inventa timestamps para cobrir lacunas.

## Referências e adaptação

- [Godot 4.5, GPU optimization](https://docs.godotengine.org/en/4.5/tutorials/performance/gpu_optimization.html): distinguir custo de vértices e fragmentos, compartilhar shaders por configuração e tratar opacos/transparência separadamente. Aplicação: isolar custo sem promover modos degradados e especializar apenas o contrato sólido comprovado.
- [Khronos Vulkan Samples, specialization constants](https://docs.vulkan.org/samples/latest/samples/performance/specialization_constants/README.html), documentação consultada em 2026-10-05: permitir eliminar caminhos mortos na criação do pipeline. Aplicação: constante de cobertura e variantes materiais existentes, sem copiar APIs ou UI de outra engine.
- [Arm, GDC 2017, Achieving Console Quality Games on Mobile](https://developer.arm.com/-/media/Files/pdf/graphics-and-multimedia/GDC2017/Achieving-Console-Quality-Games-on-Mobile-GDC217.pdf): ausência de discard como condição para otimizações de profundidade/oclusão. É princípio de pesquisa em Mali, não alegação sobre a implementação Adreno; o ganho reportado foi medido no POCO F7.

## Limites do aceite

Comparação inicial estática em Play, sem reutilização de frames. Ela não representa uma rota completa ou endurance térmico. O aparelho esteve conectado ao carregador; temperaturas de bateria de 41,9–43,2 °C não provam redução de potência ou de aquecimento. Potência à mesma cadência e teste prolongado em movimento continuam pendentes. 120 FPS exige menos de 8,33 ms por frame sustentável; a GPU ainda excede esse orçamento.

A segunda candidata evita a leitura do mip mais grosso do mapa MR quando seu peso no resultado é exatamente zero. O sample fino é avaliado antes do novo branch para preservar derivadas; a faixa de mistura mantém os dois samples. Tem consumidor real via especialização, mas permanece opt-in (`aether.full_detail_sampling`) porque o ganho medido foi pequeno/variável. Não faz parte do caminho padrão promovido.
## Build e proteção de regressão

O build Release Android final passou e foi instalado como atualização, preservando o projeto. APK em `build/play-opaque-20261005/play-opaque-final.apk`, SHA-256 `52F5F9305160D27FD25338558C586D4F926B0EB045C5A7672E1394E4008E29E8`. As famílias embarcadas afetadas foram regeneradas com glslc -O e validadas com spirv-val (NDK 27.1.12297006).

`tools/validate-opaque-shaders.ps1` compila os caminhos bindless e fallback, congela a especialização e verifica o SPIR-V executável: a família original mantém três OpKill; a sólida tem zero em ambos os caminhos. Isso protege contra a introdução futura de um discard fora das guardas. Não é prova do código de máquina do driver nem um aceite de todas as cenas/GPUs. Resultado em `shader-validation.json`.

A bancada da segunda candidata (`sampling-abba/`, APK `C319A1ACC7A6F76169F033954CD29AEBC97A0457AF6D90F5D0678121C3DE9E5D`) mantém a família opaca ativa em A e B. GPU de 16,2293→16,0199 ms, redução observada de 1,2900%; pior p95 de janela de 16,7527→20,2930 ms. Imagens idênticas, mas ganho pequeno/variável e sem aceite de performance. Ela permanece desativada por padrão. Não somar seus percentuais ao ganho do opaco.

## Comparação em resolução nativa, APK final

A/B/B/A com escala fixa 1,0 (1280×2772 no recurso, transformado para display 2772×1280), mesmos gráficos/pose, dois mil e quatrocentos frames nativos por variante e quatro capturas apresentadas contínuas de 30 segundos. `full_detail_sampling=false` nas duas variantes.

| Medida | A: original | B: sólidos sem recorte |
| --- | ---: | ---: |
| GPU média | 25,3725 ms | 21,0754 ms |
| Pior p95 de janela GPU | 26,9164 ms | 24,1756 ms |
| FPS apresentados, duas rodadas | 35,88 / 36,25 | 42,84 / 43,02 |
| Frames efetivos na família nova | 0 / 2.400 | 2.400 / 2.400 |
| Pixels diferentes nas duas capturas B | — | 0 de 3.548.160 |

Redução de GPU de 16,9361%; aumento de FPS interno de 19,3195%. Nesta condição de bancada, a família nova melhora o resultado, mas os FPS continuam abaixo de 60. Não apresentar o resultado de escala 0,75 como FPS em resolução nativa. Dados em `native-abba/`, `native-comparison.json` e `native.png`. O teste de famílias por janela valida 600 frames por janela B e zero em A, com zero reutilização de editor em Play.

## Sessão normal e preservação

Após instalar o APK final, Play foi aberto somente com profiling e autostart, sem bloqueio diagnóstico de qualidade ou escala. A captura SurfaceFlinger contínua de 30 s registrou 55,54 FPS, mínimo de janela móvel de 1 s de 51 FPS. A política adaptativa já existente mudou durante a sessão para escala 0,75 e filtro solicitado 9; por isso esse número é observação de uso normal, não prova isolada do ganho estrutural nem FPS de resolução nativa. A prova do ganho continua sendo A/B/B/A com escala/filtro iguais entre variantes.

Na janela nativa do contexto final de Play: 600 frames no caminho sólido, zero reutilizados pelo editor, isolamento Full e amostragem MR experimental desativada. Foram exercitados joystick, olhar, Saltar e Stop; capturas reais de Play, interação e retorno ao editor foram inspecionadas sem os artefatos anteriores de margem/viewport. Esta conferência curta não substitui uma rota determinística de endurance.

Hashes da cena (`10F98AC40F3F4B5CB02CFD960059498BB57FBF8828F9F3BB8AA719961ED468BC`) e das configurações (`0A0E76B1456C0D2C36C7B944078073EC93810C05A4EC819A5176AA9588784928`) são iguais antes/depois. O SHA-256 do base.apk instalado coincide com o APK final. ThermalService terminou com status 0 e IsStatusOverride=false. O editor foi reaberto sem extras diagnósticos. Dados e capturas em `default-acceptance/`.
