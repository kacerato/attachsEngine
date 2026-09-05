# G0 — zero medido da cena oceânica

- **Etapa:** G0 de `PLANO-AGUA-GLOBAL-FISICA-GRAFICOS.md`
- **Data:** 05/09/2026
- **Aparelho:** Xiaomi `25053PC47G`, painel 1280×2772, modos 60/90/120 Hz
- **Meta declarada pelo usuário:** 120 FPS com escala de render 1,0

Este documento é evidência, não plano. Todo número aqui tem PID, janela, escala
efetiva, alvo de apresentação e estado térmico ao lado, porque sem isso duas
rodadas não são comparáveis.

---

## 1. Dois defeitos de instrumentação encontrados antes de qualquer número

Ambos invalidavam silenciosamente as capturas anteriores.

### 1.1 O painel anulava a opção de lançamento

`applyRuntimeControls` sobrescrevia `policy.dynamicResolution.enabled` com o valor
do painel Android, cujo padrão é ligado. A opção `aether.disable_dynamic_resolution`
era aceita, registrada na política inicial e **desfeita no primeiro snapshot de
controles**. A resolução dinâmica então caía para o piso de 0,50.

É por isso que toda captura anterior do oceano — inclusive as que reportaram
"120 FPS" — registrou `render_scale 0.500`. Não era decisão de qualidade nem
limite térmico: era um override de diagnóstico sendo revogado sem aviso.

Correção: um override de lançamento vence o painel, e o log passa a dizer a
escala efetiva **e a origem dela**:

```
[RuntimeControls] scale=1.00(launch) dynamic=0(launch)[1.00,1.00] shadows=3 ...
```

### 1.2 `aether.target_fps` é float e estava sendo enviado como int

```
W Bundle: Key aether.target_fps expected Float but value was a java.lang.Integer.
          The default value NaN was returned.
```

O extra era descartado e o alvo caía para o padrão. `tools/measure-ocean.ps1`
agora **falha a rodada** quando o logcat contém qualquer rejeição desse tipo,
em vez de publicar o número.

---

## 2. Por que os números antigos não podem ser comparados com os novos

Duas rodadas do mesmo binário, mesma pose, mesma escala 0,5:

| Rodada | Painel | Alvo aceito | GPU média | FPS |
| --- | --- | --- | ---: | ---: |
| PID 17800 (18:39) | 60 Hz | rejeitado (NaN) | 14,21 ms | 60,06 |
| PID 26685 (18:47) | 60 Hz | 120 Hz | 12,63 ms | 60,05 |

E com o painel em 120 Hz o mesmo trabalho cai para 8,44 ms. **Tempo de GPU não é
invariante de clock:** com ADPF ativo, o alvo de apresentação e a taxa do painel
mudam a frequência pedida ao GPU, então o mesmo frame leva tempos diferentes.
Qualquer comparação A/B precisa fixar painel, alvo e estado térmico — não só a
cena. Isso vira regra permanente (critério E6 do plano).

---

## 3. Zero medido — escala 1,0, painel 120 Hz

Pose travada (`aether.lock_camera`), 600 frames por janela, última janela de cada
rodada, `thermal_pressure=none`, `game_mode=2` (GameBooster ativo nas duas).

| | **Analítico** | **Espectral (FFT)** |
| --- | ---: | ---: |
| PID / janelas | 30350 / 10 | 30354 / 9 |
| Escala de render | **1,000** | **1,000** |
| FPS apresentado | **113,00** | **106,64** |
| GPU média | 7,816 ms | 8,438 ms |
| GPU p95 | 8,805 ms | 9,165 ms |
| CPU processo p95 | 2,169 ms | 2,314 ms |
| `gpu_water_simulation_ms` | 0,001 ms | 0,585 ms |
| `gpu_opaque_ms` | 6,707 ms | 6,196 ms |
| `gpu_post_ms` | 1,108 ms | 1,663 ms |
| Triângulos visíveis | 149.504 | 149.504 |
| Draws | 4 | 4 |

### Leitura

1. **A escala 1,0 é viável.** O oceano roda a 113 FPS analítico e 106,6 FPS
   espectral em resolução nativa. A tese anterior de que só cabia a 0,5 era
   consequência do defeito 1.1, não do custo.
2. **Falta pouco para 120.** O orçamento é GPU p95 ≤ 7,333 ms; estamos em
   8,805 (analítico) e 9,165 (espectral). O déficit é **1,47 ms** e **1,83 ms**.
3. **O provedor espectral custa +0,62 ms de GPU média**, quase todo em
   `gpu_water_simulation_ms` (0,585 ms). O raster não piorou; a diferença de
   `gpu_opaque_ms` entre as duas colunas está dentro do ruído entre rodadas.
4. **`gpu_opaque_ms` é o alvo real.** 6,2–6,7 ms num cena de 4 draws e 149 mil
   triângulos é custo de fragmento de água em tela cheia, não de geometria.
5. **Pós custa 1,1–1,7 ms** para AA e nitidez em 2772×1280. É o segundo maior
   item e não depende da água.

### O que este zero **não** prova

- Pose única e travada. Não há rota de movimento, mergulho nem horizonte raso.
- Sem soak: 9–10 janelas de 10 s com pressão térmica `none` do início ao fim.
- `game_mode=2` esteve ativo nas duas rodadas. Serve para comparar entre si;
  não representa um aparelho sem GameBooster.
- A atribuição por passe dentro do render pass permanece colapsada pelo GPU
  tile-deferred: `gpu_opaque_ms` inclui o subpass da água. Separar água de céu
  exige A/B com o recurso removido, não um timestamp a mais.

---

## 3.1 O clock do aparelho move o resultado em 30% dentro da mesma sessão

Quatro rodadas intercaladas do **mesmo APK**, mesma pose, mesma escala, pressão
térmica `none` em todas, separadas por cerca de 40 s cada:

| Rodada | FPS | GPU média | `gpu_opaque_ms` | `gpu_post_ms` |
| --- | ---: | ---: | ---: | ---: |
| ab-wide-1 | 69,22 | 12,549 ms | 9,102 | 2,420 |
| ab-narrow-1 | 69,76 | 12,519 ms | 9,098 | 2,419 |
| ab-wide-2 | 87,67 | 9,739 ms | 7,174 | 1,887 |
| ab-narrow-2 | 89,20 | 9,743 ms | 7,184 | 1,888 |

Entre a primeira e a segunda rodada o mesmo trabalho ficou **22% mais barato**,
sem mudança de código, de cena ou de estado térmico reportado. Somando a
captura da seção 3 (8,44 ms), o intervalo observado para o mesmo binário e a
mesma pose vai de 8,4 a 12,5 ms.

**Consequência metodológica:** nenhum número absoluto de GPU deste aparelho vale
como comparação entre rodadas separadas. Só o par intercalado dentro da mesma
rodada mede uma mudança. Toda otimização daqui em diante precisa desse formato.

## 3.2 Formato da textura de inclinação: neutro

O par intercalado acima é exatamente o A/B de RGBA32F contra RG16F. A diferença
em `gpu_opaque_ms` é de 0,004 ms na primeira rodada e 0,010 ms na segunda — ou
seja, **nenhuma**. As quatro buscas por pixel não estavam limitadas por banda.

A mudança fica assim mesmo, justificada por memória e não por velocidade:
192 KiB em vez de 768 KiB para três cascatas de 128². Alegar ganho de quadro
aqui seria ler ruído de clock como resultado.

## 4. Déficit e para onde ele vai

Para 120 FPS sustentados em escala 1,0 é preciso tirar ~1,8 ms do p95 espectral.

| Alvo | Custo hoje | Ação | Etapa |
| --- | ---: | --- | --- |
| Amostragem de inclinação por fragmento | neutro (medido) | RG16F entregue: −576 KiB, sem ganho de tempo | G1.3 |
| Pós-processamento | 1,108–1,663 ms | rever AA e nitidez em resolução nativa | G7 |
| Simulação espectral | 0,585 ms | fila assíncrona; hoje grava na fila gráfica | G1.7 |
| Sombra na água | dentro de `gpu_opaque_ms` | 9 taps por pixel de água próxima, sem projetor de sombra no oceano aberto | G3 |

---

## 5. Reprodução

```bash
pwsh tools/measure-ocean.ps1 -Name minha-rodada -ResolutionScale 1.0 -TargetFps 120 -SpectralWater -LockCamera
```

O script grava `logcat.txt` e `summary.json` em `build/android-validation/<nome>/`,
com contexto térmico antes/depois, a linha de política resolvida e todas as
janelas. Ele aborta se o Android rejeitar qualquer extra de lançamento.
