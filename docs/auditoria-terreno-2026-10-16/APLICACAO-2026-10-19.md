# Aplicação da prévia — D07, 19/10/2026

Continuação dos 71 itens, sem alterar o universo ou as propriedades comprometidas.
O usuário reautorizou ADB: implementar primeiro, validar no aparelho depois.

## Mudança

O commit síncrono e o job assíncrono de terreno passam a capturar a revisão
imutável da prévia. Quando a receita serializada coincide e a malha está completa,
compartilham esse resultado sem regenerar campo ou malha. Caso contrário,
`updateTerrain` recebe a revisão anterior para reutilizar chunks compatíveis.
Prévias que contêm apenas campo não são consideradas malhas prontas.

O job mantém sua própria referência; cancelar/trocar a prévia da UI não invalida
o resultado em processamento. Permanecem as verificações de cancelamento,
projeto, época de cena, revisão do documento e alvo antes da publicação.
GLB, importação e publicação agregada continuam existindo. Não é conclusão de
D07, arena GPU por chunk ou sincronização localizada.

Não foram reduzidas texturas, geometria, propriedades nem controles de qualidade.
Referência oficial do D07: Unity 6000.0,
[Mesh.SetVertexBufferData](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Mesh.SetVertexBufferData.html).
Essa referência fundamenta atualizações de geometria; o cache é implementação
própria da Astra, não reprodução do algoritmo interno da Unity.

## Integração Android

O primeiro build identificou ausência da dependência `:export-core` no app e do
callback `onProjectExport` no shell. A dependência foi conectada e o callback
abre a Activity existente com nome e caminho do projeto escolhido. Não é uma
nova implementação do exportador. A compilação Java passou após a correção.

## Evidências

- Primeira rodada: 57/57 cenários host, incluindo reuso síncrono/assíncrono e
  equivalência exata dos vértices, normais, UVs e pesos na aplicação regional.
- Rodada final: **58/58 cenários passaram**. O cenário de soltura inicialmente
  pressupunha que o arrasto não acrescentava amostra na ponta. A hipótese estava
  errada: o sampler descarrega o trecho residual ao soltar. O teste final usa
  um toque estacionário com prévia concluída, cuja receita efetivamente coincide;
  o arrasto mantém sua amostra final e pode usar atualização regional.
- APK 106 compilado: `0.3.1-dev.terrain.20261019.1`, ARM64, pacote
  `dev.aether.editor.terrain`, 116.331.828 bytes. Assinatura v2/certificado anterior,
  alinhamento ZIP, ELF idêntico à saída nativa, assemblies e 397 ícones conferidos.
  SHA-256: `b01ab536badb57c8feb85d62eb2c14d120212b674e61f20bfe46a786ae525f71`.
  Arquivo local: `build/artifacts/Astra-Terreno-106-arm64.apk`.
- Instalado por atualização via ADB no POCO F7: `Success`, identidade 106
  confirmada pelo Android. Shell iniciado, processo presente; buffer de crash
  consultado vazio. **Não é aceite do renderer nem do terreno**: keyguard indica
  `showing=true`/`mInputRestricted=true`. Foi solicitado desbloqueio ao usuário.
  Toque, aparência, reedição e desempenho no aparelho continuam pendentes.
- Compilação com um worker Gradle e pool nativo de um compilador. O APK não
  passou no critério agregado ELF de 16 KB; não declarar essa compatibilidade.
- Logs locais: `build/terrain-evidence/apply-cache-20261019.log`,
  `apply-cache-final-20261019.log`, `android-build-20261019.log`,
  `android-build-retry-20261019.log`, `device106-apk-audit.json`,
  `device106-identity.log` e `device106-crash-buffer.txt`.

O primeiro bloco de 28 itens continua incompleto. Materiais/PBR extensíveis,
mapas independentes, altura-volume, GC/checkpoints, vizinhança e demais lacunas
do aceite permanecem obrigatórios.
