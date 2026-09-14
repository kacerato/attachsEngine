# Correção de fidelidade — 12/09/2026

O usuário rejeitou a primeira entrega visual. A referência continua sendo `proposta-ide.png`, excluindo a barra Android de horário/sinal/bateria. Não criar outro conceito para substituir a proposta já aprovada.

## Comparativo objetivo

| Área | Proposta aprovada | Primeira entrega | Correção nesta revisão |
|---|---|---|---|
| Cabeçalho | Marca e wordmark, busca, Play sólido | Pasta no lugar da marca, CODE, Play como adesivo | Marca real, wordmark, busca, Play em botão lima arredondado |
| Abas | Ícone, nome, fechar, Add, borda fina | Aba grande sem fechar/Add | Fechar ativo, Add, borda e sublinhado |
| Cor do código | Magenta, ciano, âmbar; linha neutra | Palavras reservadas e linha em lima | Paleta lexical distinta; fundo neutro da linha |
| Acessórios | Seis ações abaixo do console | Doze ações acima do status | Janela acessória abaixo do console; extras em menu |
| Arquivos | Navegação nomeada, seleção contida, ações inferiores | Navegação só por ícones, bloco oliva | Código/Arquivos/Console, trilho de seleção, Nova pasta/Add |
| Console | Hierarquia, bordas, chips semânticos, detalhe recortado | Faixas largas, filtros oliva, detalhe plano | Abas iguais, filtros por severidade, cartões e ações delimitadas |
| PNG | Recortes transparentes | Fundo carvão opaco e escurecido | Refazer alpha e corrigir conversão duplicada de cor |

## Decisão técnica

O renderer já tem distância analítica para retângulos arredondados, bordas internas, clipping, atlas raster e suavização. O problema não exige migrar a IDE para WebView/Compose: faltavam composição e uso consistente dessas primitivas. A edição continua em EditText/Canvas Android com texto e revisões pertencentes ao EditorCodeBuffer nativo. A barra acessória usa uma janela anexada separada para ficar depois do console sem cobrir suas interações.

Defeito confirmado no código: `ui_renderer.cpp` criava atlas sRGB, que decodifica a amostra; `astra_ui.frag::outputColor` voltava a decodificá-la. O atlas passa a UNORM, compartilhando valores sRGB codificados com os tokens; o shader faz a conversão única exigida pelo alvo. A mudança afeta os ícones da UI, não a iluminação ou as texturas da cena.

As seleções discretas usam cores de superfície explícitas: misturar lima translúcido em framebuffer linear não produz a mesma cor escura prevista no mockup. O recorte dos PNGs deve estar no canal alpha, nunca simulado pela cor do fundo.

Referências técnicas primárias consultadas:

- Android, [aceleração de Canvas e Views](https://developer.android.com/develop/ui/views/graphics/hardware-accel): suporte a desenho e retângulos arredondados pela GPU.
- Khronos, [amostragem de imagens](https://github.khronos.org/Vulkan-Site/spec/latest/chapters/textures.html) e [composição do framebuffer](https://github.khronos.org/Vulkan-Site/spec/latest/chapters/framebuffer.html): conversões sRGB e blending.

## Critério de entrega

Comparar capturas reais de código com teclado, arquivos e console com a proposta, com atenção a ordem, proporção, espaços, alinhamento, contornos, recortes e alpha. A captura deve mostrar dados reais; não criar logs, linhas ou arquivos falsos para preencher áreas vazias. O teclado pertence ao IME do usuário e sua aparência não é controlada pela Astra.

Esta revisão não encerra M06 inteiro nem equivale a aprovação visual do usuário. Registrar abaixo o resultado real da compilação, instalação e inspeção.

## Resultado e limites

- Compilação Android Debug passou; APK instalado por ADB no onyx 25053PC47G autorizado.
- Capturas reais: `docs/validacao/evidencias/ide-fidelity-20260912/`. Comparação navegável: `comparativo.html`.
- Código em retrato com IME real, status, console recolhido e seis acessórios visíveis na ordem aprovada. Menu mantém as ações removidas da barra.
- Menu extra também recebeu superfície, borda, raio e tipografia da IDE; abertura e fechamento conferidos no APK final.
- Árvore com navegação nomeada, nova pasta/Add, ícones sem fundo opaco e seleção contida.
- Diagnóstico real CS1002 provocado pela remoção temporária de `;` do script de validação GiroM06.cs. Compilação automática levou o console à aba Problemas. A edição é desfeita ao finalizar a inspeção.
- Os 12 PNGs publicados têm RGBA, extremos alpha 0–255 e quatro cantos transparentes. O renderer mostra os ícones sem as placas pretas da primeira entrega.
- A prévia de fonte é o conteúdo atual de um buffer já aberto; não é snapshot histórico do código que produziu o diagnóstico. Não há leitura síncrona de arquivos por frame.
- O agrupamento permanece automático; não foi inventado um botão de alternância sem armazenamento dos eventos individuais. Exportar ocupa esse espaço funcional nesta revisão.
- Não há identidade pixel a pixel: tipografia nativa, dimensões reais, quantidade de arquivos/logs, teclado do usuário e algumas silhuetas diferem da prancha. A composição foi corrigida e está disponível para comparação; aprovação visual permanece com o usuário.
