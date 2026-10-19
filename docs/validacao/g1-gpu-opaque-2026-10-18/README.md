# G1: atribuição do passe opaco na GPU (Vila Astra, nível S), 18/10/2026

POCO F7 (Adreno 825), Astra Dev 103 (`0.3.0-dev.103-gt`), Play, 40 s por rodada.
Névoa resolvida ligada, como em produção. Rodada = `tools/measure-g0-baseline.ps1`
com `-GpuIsolation 0..5`, na sequência 0→5 e depois 5→0, para cancelar a deriva.

O aparelho esquentou durante a série: FPS do modo completo 31,9 na 1ª rodada e
19,0 na 12ª. Por isso a comparação usa **opaco ÷ pós da mesma rodada**. O pós não
muda com nenhum modo de isolamento, então a razão cancela o clock. O modo completo
deu 1,610 na ida e 1,612 na volta.

| Modo (`aether.gpu_isolation`) | opaco/pós (ida, volta) | Média | Parte do opaco |
|---|---|---|---|
| 0 completo | 1,610 · 1,612 | 1,611 | — |
| 1 sem normal map | 1,383 · 1,455 | 1,419 | 11,9% |
| 2 sem IBL especular | 1,543 · 1,540 | 1,542 | 4,3% |
| 3 só cor base | 0,788 · 0,725 | 0,756 | 53,1% (todo o material e a luz) |
| 4 sem luzes pontuais | 1,374 · 1,374 | 1,374 | 14,7% |
| 5 sem sombra direcional | 1,281 · 1,287 | 1,284 | 20,3% |

Rodada 1 (completo, aparelho frio): GPU 28,0 ms = opaco 13,8 + pós 8,6 + sombra 3,4
+ FSR 1,35 + skinning 0,7 + UI 0,2. Resolução cheia, 2772×1280 (escala 1,00 no S),
77–105 draws, 180–250 mil triângulos, 8 luzes pontuais acesas.

## Leitura

- 47% do opaco é geometria, rasterização e cor base em 3,5 MP. Não é atacável no
  shader; resolução e LOD são decisões de política.
- **Sombra direcional (20%)**: nas cascatas próximas do S são 9 buscas PCF 2×2 de
  hardware. A soma equivale exatamente a 4 `textureGather` com pesos (1−f, 1, 1, f)
  por eixo. Implementado; verificação numérica em `gather-equivalencia.py`
  (diferença máxima 1e−15 em 20.000 pontos).
- **Luzes pontuais (15%)**: o laço percorre as 8 luzes em todo fragmento. Agora
  existe uma máscara por desenho, calculada na CPU pela esfera de cada luz e de cada
  desenho; é candidata, desligada por padrão (`aether.punctual_light_mask`).
- Normal map (12%) e IBL (4%) ficam para depois.
- O FSR1 com escala 1,00 vem da configuração do projeto. O EASU em 1:1 custa
  0,63 ms (2%), abaixo da régua de 5%.

Pendente: A/B/B/A das duas mudanças no aparelho. O POCO saiu do ADB antes do A/B.
