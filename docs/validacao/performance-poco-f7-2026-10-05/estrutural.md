# Desempenho estrutural — POCO F7 / Sponza / 2026-10-05

## Resultado e limite atual

**Atualização após reconexão:** este arquivo preserva a etapa anterior à perda
do ADB. A divisão espacial já foi medida, e M7 passou a ter economia real de
potência e imagem idêntica no aparelho. Resultados e limites atuais em
[continuacao.md](continuacao.md). Os estados de instalação e pendências abaixo
são históricos; não representam o pacote atualmente instalado.

O problema de FPS e aquecimento **não está encerrado**. A correção anterior de
empacotamento Debug → Release retirou custo indevido de desenvolvimento. Esta
etapa corrigiu medição, composição e visibilidade, e implementou uma candidata
de divisão espacial. Não houve ganho estrutural suficiente demonstrado nem
redução de potência demonstrada. O aparelho desconectou do ADB antes da coleta
da candidata. O app foi parado ao terminar a última comparação.

Plano e dependências: [PERFORMANCE-POCO-F7-2026-10-05.md](../../planos/PERFORMANCE-POCO-F7-2026-10-05.md).
Roadmap utilizado: `C:/Users/donod/Downloads/Astra_Roadmap_Desempenho_Graficos.md`.
Sequência: M0 → composição M4/M5 → geometria/visibilidade M2/M3 → M7 → aceite M8.
Os resultados anteriores de Debug/Release estão em `diagnostico.md`; não são
um A/B de mesma pose e não devem ser misturados com os números abaixo.

## Método

Mesmo aparelho, projeto, câmera explícita, viewport, resolução e opções dentro
de cada A/B/B/A. Perfil Release, target 120, escala fixa 0,75, DRS desativado
somente para eliminar variação de resolução da comparação. Não houve redução
entre A e B de triângulos de origem, AA, materiais ou qualidade de sombras.
O protocolo fixa parâmetros para medir; não muda as configurações persistidas
como solução de desempenho. Cada conjunto compara caminhos no mesmo APK.

Pose `[x,y,z,yaw,pitch]`: `[-6.101169,5.219586,-8.918053,0.6,0.45]`.
Contexto publicado: 411 desenhos/instâncias de origem, 35 materiais,
72 texturas, 3.749.344 triângulos. Baseline visível: 233 desenhos,
2.242.538 triângulos. Capturas: 2772 × 1280. Geometria importada passa a
reiniciar o contexto do profiler, sem incluir a carga como cena vazia.
O fingerprint interno de pacote é zero em projetos independentes, uma
limitação registrada; não é utilizado como prova de identidade do conteúdo.

Identidade externa preservada na coleta anterior:

- Cena: SHA-256 `02FA99D59FDE13BEDCD58FE5F0ABE78396AE9DCB2C4B1D61ECC24CDB90A4CD1A`.
- Rendering: SHA-256 `0A0E76B1456C0D2C36C7B944078073EC93810C05A4EC819A5176AA9588784928`.

As janelas contêm 600 frames; agregados usam médias das janelas completas.
Percentis reportados são o pior p95/p99 de janela, **não** percentis agregados.
FPS interno e FPS apresentado são métricas diferentes. Não agregar FPS de
apresentação quando `surface_continuous=false`.

## Comparações reais

| Comparação | GPU A/B (ms) | FPS interno A/B | Imagem | Decisão |
|---|---:|---:|---|---|
| Fusão isolada, `composition-ab` | 11,120 / 11,156 | 74,92 / 74,66 | Idêntica | Sem ganho demonstrado |
| Fusão + recorte, `composition-clipped-ab` | 11,319 / 11,002 | 73,06 / 75,28 | Um pixel, diferença máxima 2/255 | Melhora observada de 2,81% GPU, abaixo do critério de eficácia de 5% |
| HZB antes da correção, `hzb-editor-ab` | 10,891 / 10,773 | 76,26 / 77,00 | 16.227 pixels com diferença >3/255 | Resultado rejeitado por imagem incorreta |
| HZB corrigido, `hzb-corrected-ab` | 10,706 / 11,022 | 78,20 / 74,79 | Idêntica | Sem ganho; desativado por padrão |

Arquivos `comparison.json`, `windows.json`, `context.json`, `logcat.txt`,
`screen.png`, `surface.json[l]`, `thermal.jsonl` e bateria antes/depois
preservam os dados de cada rodada. O coletor original da fusão isolada teve
lacunas; na composição com recorte a rodada `04-A` teve lacuna. As quatro
rodadas do HZB corrigido têm apresentação contínua. Isso não invalida as
janelas Vulkan, mas impede afirmar um A/B completo de FPS apresentado nos
primeiros conjuntos. Não comparar médias entre conjuntos distintos como se
fossem o mesmo A/B: os estados térmicos iniciais mudaram.

Na comparação HZB corrigida, as temperaturas de bateria por rodada foram
39,9→40,1; 40,1→41,0; 41,0→41,3; 41,3→42,0 °C. O sinal de pressão térmica
permaneceu `none` nas janelas. Isto não demonstra resfriamento ou potência.
`dumpsys powerstats` não trouxe dados; sensores sysfs de corrente/clock negaram
acesso. Nenhum valor em watts foi inferido.

## Mudanças executáveis

- Medição: launcher encaminha a pose para a câmera real; profiler identifica
  a publicação da cena e informa contagens reais. Coletor de apresentação
  separado da consulta térmica; análise rejeita apresentação com lacunas.
- Composição: UI e pós final compatível compartilham render pass; pós limitado
  ao viewport físico e clear convertido para saída sRGB quando necessário.
  TAA com histórico, FSR1 intermediário e caminhos incompatíveis preservam
  a implementação anterior. Timestamps de UI podem ser atribuídos ao pós
  no passe fundido; custo de UI zero no contador não significa trabalho zero.
- HZB: produtor reconstruído após importação, projeção consistente com Vulkan,
  faces próximas/distantes conservadoras, viewport/rotação, invalidação e
  reutilização estática. Movimento/deformação fazem fail-open. A versão errada
  não foi promovida. A versão corrigida removeu 21 desenhos, mas só 48
  triângulos: os limites amplos das malhas pesadas impedem economia útil.
- Candidata espacial: usa o `SpatialRenderChunks` existente, com até 2048
  triângulos por bloco opaco; mantém todos os triângulos e a identidade editável
  do objeto. Limites locais são transformados pela matriz atual, incluindo
  escala, espelhamento e shear conservador. Culling frustum por bloco e envio
  indireto em lote reaproveitam material e `firstInstance` do objeto.
  Transparência, água, impostores, buffers deformados e LOD alternativo ficam
  no caminho original. Buffers entram no allocator/lifecycle real; falta de
  capacidade ou orçamento mantém o desenho original. Custo extra de índices,
  preparação e CPU precisa ser medido. **Opt-in de diagnóstico, ainda sem
  aceite de execução no aparelho.**

## Build, testes e pacote

- 32 testes direcionados existentes de chunks, GPU-culling e HZB passaram
  em executável host usando os fontes reais. Eles protegem preservação de
  triângulos/limites e regressões de projeção; não comprovam integração Vulkan.
- Build host completo parou por `test_bulk50.cpp.obj` grande demais no
  assembler MinGW. Não registrar suíte completa aprovada.
- Android `:app:assembleRelease --offline --max-workers=4` passou para a
  candidata final. APK não-debuggable, arm64, sem biblioteca de validation
  layer. A assinatura continua a chave de desenvolvimento existente,
  compatível com atualização local; não é assinatura de publicação comercial.
- APK candidato: `build/performance-poco-f7-20261005/structural-candidate.apk`.
  SHA-256 `C93853486A77D801FD7FB41390A2A3D172E4B9AC01D7DA306C3CDC74397A4B29`.
  Certificado SHA-256 `9dd308380ab1606fa4e328a1d62fb9edda5abdf6cf7e5696bc8ccbf7c83bc66e`.
  Não instalado: telefone ausente do ADB na etapa final.
- Último APK instalado/testado (HZB corrigido): SHA-256
  `DA44CCA7BF8BE04C69F655E69BE8A3D31D1307667161EF5C070E1400DBD71BB9`.
- O APK foi compilado do workspace atual, que já continha mudanças de outras
  tarefas. A assinatura de fontes registra os arquivos desta alteração;
  não representa uma revisão Git limpa nem valida as outras funcionalidades.

## Aceite que falta

Reconectar o POCO por ADB, instalar o candidato sem apagar projeto e executar:

```powershell
./tools/measure-editor-composition.ps1 -OutputDirectory docs/validacao/performance-poco-f7-2026-10-05/spatial-geometry-ab -SpatialComparison -Seconds 60
```

A divisão espacial precisa passar mesma-pose A/B/B/A, segunda vista,
movimento/seleção/transformação, Play/Stop e reabertura. Registrar GPU/CPU,
triângulos enviados, memória e imagem. Promover somente se o ganho superar
o ruído sem piorar caudas ou lifecycle. Depois executar pelo menos 20 minutos
na mesma cadência e condições térmicas, com sinal de consumo válido quando
disponível. FPS maior isolado não encerra a queixa de aquecimento.
