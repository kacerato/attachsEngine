# Execução rumo ao primeiro editor Android

Registro de execução de 28/08/2026. A autoridade de escopo e de aceite continua
em `PLANO-ENGINE-MOBILE.md`; este documento não cria fases ou gates alternativos.

## Sequência e estado

| Ordem | Referências do plano principal | Entrega verificável | Estado desta execução |
|---|---|---|---|
| 1 | 2.1.4, 2.1.6, 2.1.7; PoC-E | Estabilizar os caminhos gráficos existentes e a evidência de compressão | Correções abaixo implementadas e validadas; não encerra os itens completos |
| 2 | Parte 3.3; 1.4; 2.1–2.5 | Cena e componentes alimentam recursos e lotes de renderização; criar/remover/mover atualiza a imagem | Primeira integração implementada: cubo/checker, matrizes completas e fixture Android; recursos múltiplos e editor pendentes |
| 3 | 3.1; 3.4.6 | UI retida/GPU, layout, tokens, áreas seguras e roteamento de toque | Pendente |
| 4 | 3.2; 3.3; 3.4.1 | Câmera, seleção, hierarquia e gizmos com resolução de conflitos de gestos | Pendente |
| 5 | 1.4; Parte 4.4; 3.4.2 | Inspector pelos metadados, edição por comandos e undo/redo integrado | Pendente de integração |
| 6 | Parte 7.4; 3.5.4 | Persistência de cena, identidades/referências estáveis e recuperação | Pendente de integração |
| 7 | 3.6.1–3.6.5 | Play em processo separado, IPC, apresentação no viewport e isolamento de falhas | Pendente |

O primeiro fluxo com cubos é um teste de integração, não substitui o M2
(renderer completo e medido) nem o M3 (cena, luzes, materiais, prefab, Play e
usabilidade). A implementação não deve introduzir uma UI alternativa em Java,
um segundo modelo de propriedades ou Play sem isolamento como solução final.

## Lote 1: correção do renderer existente

O commit herdado `d56c992` já habilitava validation layers e corrigia o recurso
sampled-image update-after-bind e os semáforos por imagem de apresentação.
Esta execução preserva essas mudanças e os arquivos ainda não integrados
`native/rhi/pipeline_cache.*`/alteração de `descriptor_cache.h`.

- A negociação de bindless exige as quatro sub-features usadas pelo layout e
  limita a tabela pelos seis limites aplicáveis de samplers, imagens, recursos
  por estágio e pools. Consulta ausente/limite zero seleciona fallback.
- `descriptorBindingUpdateUnusedWhilePending` não é solicitado: o layout não
  o utiliza e anteriormente ele era habilitado sem consultar suporte.
- O renderer respeita `EnabledPaths.bindless`. O caminho conservador usa um
  descritor combinado convencional e fragment shader sem array runtime ou
  nonuniform indexing; mantém instancing, textura e depth.
- `aether.force_descriptor_fallback` desabilita os recursos no **VkDevice**,
  não só um ramo do desenho. Permite testar esse contrato no Adreno disponível;
  não equivale à validação de desempenho/driver num aparelho real de perfil C.
- Os descritores dos dois caminhos têm ownership/destruição explícitos. Não
  foram adicionados trabalhos por entidade nem alocações no loop de frame.

A instância continua pedindo Vulkan 1.1; o caminho de extensão é conservador.
Não se infere habilitação core 1.2 a partir apenas da versão física da GPU.
Tabelas globais de buffers/samplers e classificação completa por GPU continuam
fora do escopo já implementado de 2.1.4/2.1.7.

## PoC-E: evidência reproduzível, sem falso positivo

- Buffer ASTC inclui `TRANSFER_SRC`; há dependências compute→transfer e
  transfer→host. Conferido contra os contratos de
  [cópia Vulkan](https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyBufferToImage.html).
- Submissão, espera, reset e finalização dos comandos são verificados; staging
  pertence ao mesmo objeto de cleanup, inclusive nos retornos por falha.
- Timestamp exige suporte de fila, período/delta válidos e trata wrap dos bits
  válidos. Medição ausente é indisponível (`null` no relatório), nunca 0 ms.
  O intervalo deve caber em uma volta do contador, conforme o contrato do helper.
- Corpus RGB opaco combina blocos sólidos, rampas e checker intrabloco. A
  comparação cobre **todos os canais de todos os texels**, não uma amostra.
- `succeeded` exige tempo válido **e** comparação dentro da tolerância estrutural
  de 24/255. Isso não certifica qualidade visual para fotos, normais, alpha/HDR.
- A opção `aether.astc_probe` dispara um worker com device/fila exclusivos,
  usando a mesma implementação de inicialização. Não disputa a fila Vulkan do
  shell nem bloqueia a thread de eventos durante o teste. É diagnóstico opt-in,
  não o importador assíncrono de produção. A espera GPU não oferece cancelamento;
  o encerramento do processo aguarda o worker enquanto o driver responde.

### Resultados em aparelho

Xiaomi 25053PC47G / SM8735 / Adreno, Android 16:

| Verificação | Resultado |
|---|---|
| Bindless com Khronos validation ativa | PASS, sem VUID/Validation Error no log coletado |
| Fallback com descriptor indexing desabilitado no device | PASS, sem VUID/Validation Error no log coletado |
| ASTC 4096×4096, todos os 16.777.216 texels RGBA | PASS, erro máximo 7/255 |
| GPU encode, APK debug | 2,829844 ms |
| Tempo total do probe, incluindo inicialização, recursos, upload e comparação | 2.393,012655 ms |
| Lifecycle normal | PASS: 3 retomadas e mudança de configuração, PID preservado |

**2,83 ms não é o tempo de importação.** A medição total acima é de um probe
debug e não deve ser usada como benchmark de importador de produto. O shell
continua renderizando durante o diagnóstico; os números não são comparação
controlada entre dispositivos ou builds.

Evidência: `build/android-validation/rendering-20260828-140330/report.json`
e logs por caminho; lifecycle em
`build/android-validation/stabilization-lifecycle-20260828/report.json`.
APK debug medido: SHA-256 `066339B9D662939241C953BD15DBCF3D1CBA66897B319421308D050FEFD05460`.
Os artefatos locais identificam o binário medido; não são provas de builds futuros.

Repetição no APK debug final: `stabilization-final-debug-20260828/report.json`,
SHA-256 `2CDFCE18AA073A7BF1560266EEC2C3FCDAD016D2F953EEBC70085D831800673E`.
Ambos os caminhos passaram novamente; ASTC GPU **2,268906 ms**, probe total
**2.223,441510 ms**, erro máximo **7/255**. A variação entre rodadas não é
evidência de otimização: não houve mudança no algoritmo/corpus.

Entrega no aparelho: APK **release**, assinado com a chave local de debug
somente para teste, preservando dados. `stabilization-release-verified-20260828/report.json`
passou com 1.000 frames e uma retomada, PID 8395 preservado. O app ficou aberto
em modo normal, sem probe/fallback forçado. A tentativa anterior em
`stabilization-final-release-20260828/` falhou antes da instalação por nome de
APK incorreto (`assembleRelease` produz `app-release-unsigned.apk`); não foi
falha do runtime e seu relatório foi preservado.

### Reprodução

```powershell
./android/gradlew.bat -p android :app:assembleDebug --offline --console=plain
./tools/validate-android-rendering.ps1
./tools/validate-android-shell.ps1 -SkipInstall -PreserveAppData -LifecycleCycles 3 -ExerciseConfigurationChange -AllowScreenshotDifference -KeepAppRunning
```

O primeiro runner exige APK debug e aparelho manualmente desbloqueado. Instala
com `-r`, não limpa dados, registra logs/JSON e restaura o app sem as opções de
diagnóstico. Não reinicia soak de 30 minutos. A regressão local desta execução
é 159 testes C++ e 486 C#, sem falhas; shaders passam em `spirv-val` e o build
Android debug/release e lint foram executados.

## Integração cena/render — lote 2

Contrato implementado em `Aether.Rendering`, com MeshRenderer serializável,
GUIDs de recursos, matriz afim completa e ABI de 88 bytes. A PoC-A permanece
separada. Testes cobrem remoção/recriação, hierarquia, validação sem escrita
parcial, serialização e zero alocação. A cena tem um único par cubo/checker
disponível; não é um renderer de materiais completo.

Arquitetura, reprodução, limitações e a referência visual de esfera 8K/16K
proposta pelo usuário estão em `SCENE-RENDER-INTEGRATION.md`. O próximo
bloco de interface continua sendo UI retida/GPU; a esfera de materiais
pertence às entregas de recursos/PBR/IBL da Fase 2, sem antecipar o aceite M2.

## Esfera de materiais — lote 3

Implementada a referência proposta, como sample padrão do launcher:
esfera UV indexada (36.480 triângulos), material PBR GGX/Burley/Fresnel,
normal mapping, IBL especular pré-filtrada e albedo/normal/ARM 8192² reais.
ASTC 6×6 com 14 mips, fallback RGBA8 1K e residência inicial por quota/capability.
Upload generalizado no RHI; import/bake somente offline; worker com cancelamento
e ownership exclusivo durante carga. Recursos e parâmetros têm IDs/JSON v1.

Testes: 506 C#, 165 C++, 47 ferramentas Android e 6 do import; shaders,
debug/release/lint aprovados. `material-final-20260828/report.json` passou nos
três caminhos em Android com Khronos validation. Regressões PoC-A e cubos:
`material-regression-poca-20260828/` e `material-regression-cube-20260828/`.
HOME durante carga cancelou o worker e retomou sem mudar PID.

2.4.2/2.4.4/2.4.6 são **parciais**, não concluídos. Sem multiscatter, SH/probes,
sombras, pós completo ou Inspector. O próximo bloco de integração continua
sendo UI retida/GPU e edição conectada à cena; não ampliar para efeitos soltos.
Contratos, memória, licenças, reprodução e evidências: `MATERIAL-PREVIEW.md`.
