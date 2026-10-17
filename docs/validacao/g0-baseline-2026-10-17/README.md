# G0: linha de base no POCO F7 (17/10/2026)

Plano: `docs/planos/DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md`, bloco G0.
APK: Astra Dev `dev.aether.editor.u07` 0.3.0-dev.98-g0b (versionCode 98), profileable,
instalado com `install -r` sem apagar dados. Coletor: `tools/measure-g0-baseline.ps1`.

## Condições

- Aparelho 25053PC47G (POCO F7), superfície 2772×1280, ADB sem fio.
- **Carregador ligado e aparelho quente (bateria 43,9–45,8 °C)**, por decisão do
  proprietário. A API térmica relatou `none` o tempo todo, mas uma rodada anterior,
  com o aparelho esfriando, mostrou queda de clock de ~50% sem aviso. Estes números
  valem para o estado aquecido; não comparar com medições a frio.
- Ordem S/C/C/S, 40 s por rodada, resolução dinâmica desligada, qualidade travada.
- Nível C forçado = `aether.quality_preset=c` + `aether.reference_resolution=1600x720`
  (escala 0,5698, interna 1579×729). Mede o custo relativo no POCO; **não é o FPS de
  um celular fraco**.
- Vila Astra em Play (câmera do player). Sponza em Edit, pose fixa
  (-6,101; 5,220; -8,918; yaw 0,6; pitch 0,45), reuso estático do editor desligado.

## Resultado (ms; FPS apresentado)

| Cena / nível | FPS | GPU quadro | CPU processo | Opaco | Pós | Sombra | EASU | RCAS | Skinning | UI |
|---|---|---|---|---|---|---|---|---|---|---|
| Vila S (r1) | 18,58 | 50,72 | 23,49 | 23,09 | 18,05 | 5,53 | 1,06 | 1,24 | 1,35 | 0,36 |
| Vila C (r2) | 29,89 | 31,08 | 18,51 | 9,87 | 6,05 | 5,53 | 6,28 | 1,28 | 1,67 | 0,35 |
| Vila C (r3) | 30,44 | 30,58 | 17,48 | 9,86 | 6,04 | 5,38 | 6,26 | 1,20 | 1,44 | 0,35 |
| Vila S (r4) | 18,43 | 50,93 | 23,77 | 23,34 | 18,05 | 5,42 | 1,03 | 1,24 | 1,45 | 0,36 |
| Sponza S (r1, aquecimento) | 31,18 | 31,18 | 4,86 | 16,61 | 14,49 | 0 | 0 | 0 | 0 | 0 |
| Sponza C (r2) | 40,74 | 22,11 | 4,54 | 8,15 | 13,90 | 0 | 0,01 | 0 | 0 | 0 |
| Sponza C (r3) | 40,94 | 22,12 | 4,41 | 8,14 | 13,90 | 0 | 0,01 | 0 | 0 | 0 |
| Sponza S (r4) | 41,29 | 22,71 | 4,54 | 12,12 | 10,53 | 0 | 0 | 0 | 0 | 0 |

Repetição A/A: Vila S 18,58/18,43, C 29,89/30,44 (< 2%). Sponza S r1 descartada
(primeira abertura: caches e streaming); C 40,74/40,94.

## Leitura

1. **Pós-processamento é o maior alvo junto com o opaco**: 18 ms na Vila e 10,5–14 ms
   na Sponza. O shader único de pós (`post_process_common.glsl`) faz SSAO, bloom,
   FXAA, neblina e tonemap por pixel. Isso move a divisão do pós (G2) para o início
   do G1.
2. **CPU da Vila em 17,5–23,8 ms**: com 11 personagens animados e scripts, a CPU sozinha
   impede 60 FPS. Na Sponza (sem personagens) a CPU fica em 4,5 ms. Próximo passo:
   simpleperf no APK profileable (já instalado).
3. **Opaco** 23 ms (Vila S) e 12 ms (Sponza S): continua grande, mas o pós e a CPU vêm antes.
4. **Ressalva do nível C na Vila**: a política resolveu `preset=c`, mas o projeto define
   eixos de sombra/ambiente explicitamente e esses eixos vencem o preset (regra da
   política). A Vila em "C" manteve 4 cascatas @4096/25 taps; só a escala mudou. Na
   Sponza, sem eixos autorais, o C aplicou 1 cascata @768, ambiente hemisférico e pós sem bloom.
5. **Ressalva do EASU**: 6,3 ms no POCO porque a saída é 2772×1280. Num celular 720p a
   saída tem cerca de ⅓ dos pixels; esse custo não se transfere ao nível C real.
6. **Pós maior no C que no S na Sponza (13,9 × 10,5 ms)**: não explicado; investigar
   no G1 (candidatos: reconstrução/ampliação feita no próprio passe de pós na saída nativa).

## Projetos usados

- Vila Astra: cópia de trabalho `output/ui-play-export/vila-project/` enviada com `.astra/`.
- Sponza: Intel Sponza Base Scene (`main_sponza.zip`, 3,99 GB, baixada em 17/10/2026 de
  https://cdrdv2.intel.com/v1/dl/getContent/830833). Licença em `credits_license.txt`:
  uso pessoal/educacional e comercial limitado a marketing; **não distribuir no APK nem
  no AstraDocs**. Enviados ao aparelho só os 73 arquivos que o glTF referencia (1,9 GB);
  importada pela UI (Arquivos › importar pasta): 155 nós, 405 malhas, 29 materiais,
  72 texturas ASTC 6×6 limitadas a 2048 px, LOD gerado, 24 luzes e 6 câmeras não
  importadas. A autoria antiga de player/luz (`sponza_scene_author`) dependia das
  identidades da importação de 05/10 e não foi reaplicada.

Arquivos brutos por rodada: `vila/vila-astra/NN-T/` e `sponza/sponza/NN-T/` (logcat,
política, janelas, superfície, captura de tela, bateria e térmica antes/depois).
