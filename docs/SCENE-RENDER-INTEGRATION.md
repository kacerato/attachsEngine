# Cena → renderer: primeira integração Android

## Escopo e decisão

Fatia de integração de 28/08/2026, subordinada a
`PLANO-ENGINE-MOBILE.md`: serialização/reflexão 1.4, instancing 2.5.3
e fundação do viewport 3.3. Não encerra M2/M3 nem implementa editor visual.

Problema: a PoC-A transmitia posição XY/cor, com posicionamento especial e
rotação no shader. Isso não representa transforms de uma cena.

Decisão: `Aether.Rendering` extrai o ECS para um buffer do chamador.
Não colocar MeshRenderer no Core nem carregar uma segunda cópia de Core
para hospedar módulos. O APK publica Rendering como raiz framework-dependent,
com Core e Scene no mesmo contexto de carga. A PoC-A permanece separada.

## Contrato

- `MeshRenderer`: ResourceId da malha/material, tint RGBA e Enabled.
  Registro explícito no ComponentRegistry, nome estável e schema 1.
- ResourceId mantém seu Guid persistente. O registro reconhece Guid como folha
  de 16 bytes e texto usa formato canônico D. O enum ganhou um valor no final;
  valores antigos e formatos de cena não foram renumerados.
- `TransformSystem.TryGetWorldMatrix`, no Core, multiplica matrizes locais
  pela cadeia de ancestrais. Preserva shear causado por escala não uniforme,
  sem alterar a propagação TRS/physics já existente. Falha para hierarquia
  inválida, pai morto, ciclo ou profundidade excessiva.
- `RenderInstance` ABI 1: matriz column-major (64 bytes), tint (16), EntityId
  transitório índice/versão (8), stride **88**. Não é formato de persistência.
- `Extract` valida todos os objetos habilitados antes de escrever. Retorna
  count no sucesso; buffer insuficiente informa count necessário na API C#.
  Recursos desconhecidos, transforms inválidos e tint não finito/transparente
  produzem erro explícito. Nenhuma substituição silenciosa de recursos.
- O destino é emprestado, não retido. Acesso exclusivo pela thread de
  eventos/render; não permite mutação estrutural concorrente. Custo O(N ×
  profundidade), duas passagens e zero alocação após aquecimento.

## Integração e lifecycle

Uma chamada C++→C# preenche o buffer persistente por frame. O backend usa
matriz/tint como atributos de instância, depth e perspectiva LH; mantém
bindless e fallback sem descriptor indexing.

`ScenePreview` é uma fixture de diagnóstico, não API de edição. Cria dois
cubos, um deles filho de outra entidade. O runner move/rotaciona/escala o pai,
remove o filho e cria outro objeto; as contagens apresentadas são 2 → 2 → 1 → 2.
IDs de entidades removidas não sobrevivem como instâncias renderizadas.

Initialize é idempotente: recriar surface/pipeline não reinicia o World.
Shutdown ocorre no encerramento da sessão. Falha de extração apresenta um
frame limpo e registra erro, sem desenhar dados antigos ou abandonar o
semáforo adquirido. O profiler informa a quantidade real, não 5.000.

## Reprodução

```powershell
./android/gradlew.bat -p android :app:assembleDebug :app:assembleRelease :app:lintDebug --offline --console=plain
./tools/generate-embedded-shaders.ps1 -ShaderName scene_preview -Stages vert -Check
./tools/validate-android-scene.ps1
```

O runner instala com `-r`, preserva dados e exige aparelho desbloqueado.
Testa ambos os caminhos, exige Khronos validation ativa, verifica o Ping de
Core, as quatro etapas e uma retomada mantendo PID, câmera e fingerprint do
buffer enviado à GPU. O fingerprint FNV-1a é calculado somente nos checkpoints,
não por frame; é diagnóstico CPU, não readback GPU.

Capturas completas são preservadas e comparadas. Overlays OEM pertencem ao
Android, não à cena: desigualdade exige revisão visual e aparece como
`visualReviewRequired` no relatório. Não se recorta nem mascara a imagem.
`-RequireIdenticalScreenshot` torna a igualdade um gate obrigatório.
Logs, hashes e capturas são locais.
Ao terminar deixa aberta a cena estática, com órbita touch disponível.

Sem extras, o launcher continua abrindo a PoC-A. A cena usa
`--ez aether.scene_preview true`; mutações automáticas exigem também
`--ez aether.scene_validation true`. Não há editor/menus escondidos.

## Evidência desta execução

- **501 testes C#**, zero falhas/skips, com interop nativo obrigatório;
  **159 testes C++** e **47 verificações das ferramentas Android**.
- APKs debug/release e lint passam; os shaders da PoC, fallback e preview
  passam na validação SPIR-V e reprodução dos headers.
- `build/android-validation/scene-snapshot-20260828/report.json`: ambos os
  caminhos apresentaram as quatro etapas; PID, matrizes, IDs, tint e câmera
  preservados após a retomada. Khronos validation ativa, sem VUID/crash.
- APK debug SHA-256:
  `DF89B93CD7E4309CFB35E8E701EF91F43039D6D4880E3F14867372963251AFA7`.
  Aparelho Xiaomi 25053PC47G, Android 16, Adreno.
- Revisão das imagens: a cena é idêntica nos dois caminhos. Antes/depois,
  todas as diferenças estão no indicador OEM à esquerda: bindless **1.820
  pixels**, retângulo (0,64)–(36,274); fallback **2.683 pixels**, retângulo
  (1,64)–(46,274). Capturas 2772×1280; região dos objetos sem diferenças.
  As duas capturas finais, bindless/fallback, têm o mesmo SHA-256.
  O relatório conserva `resumeImageIdentical=false`, corretamente: revisão
  visual não transforma desigualdade da tela inteira em igualdade.
- Regressão da PoC-A preservada:
  `scene-benchmark-regression-20260828/report.json` passou em bindless/fallback.
  O probe ASTC voltou a passar (2,796510 ms GPU, 2.166,274322 ms total, erro
  máximo 7/255 em todos os texels). Não é medição de performance da nova cena.
  O APK debug permaneceu instalado para inspeção, com dados preservados;
  os APKs debug/release foram compilados, mas este release novo não foi
  instalado nem validado em hardware.
- Tentativas anteriores foram preservadas: `scene-integration-20260828`
  teve gestos de câmera durante a coleta; `scene-integration-repeat-20260828`
  e `scene-integration-final-20260828` falharam no gate de screenshot completa
  por diferenças no indicador OEM. Não houve correção no renderer para
  contornar esses pixels; a evidência automatizada de estado passou a ser
  distinta da revisão visual do display.

## Limites do lote 2 e evolução no lote 3

Registro histórico do lote de cubos. A esfera PBR 8K foi implementada depois,
no lote 3, e é agora o sample padrão do launcher: ver `MATERIAL-PREVIEW.md`.
Os limites de iluminação abaixo descrevem apenas a fixture de cubos, mantida
por `aether.scene_preview=true`; PoC-A usa `aether.poc_a=true`.

- Um único lote de cubo/checker embutidos; capacidade diagnóstica de 256.
  Ainda não há catálogo de malhas GPU, agrupamento de vários pares, importação
  de modelos, culling ou crescimento dinâmico do buffer.
- Câmera diagnóstica: FOV 60°, near 0,1, far 100, órbita a distância 6.
  CameraComponent, pan/zoom e controles completos seguem pendentes.
- Material opaco sem iluminação: não é PBR, não contém sombras/reflexos.
- Persistência validada em testes de World; UI de save/load, Inspector,
  comandos de edição e Play isolado não fazem parte desta entrega.
- Os testes pequenos de extração não comprovam orçamento de uma cena grande.

O usuário propôs uma **esfera de teste de materiais**, com texturas 8K/16K,
como referência visual. Isso fica associado ao renderer de materiais do
plano, especialmente **2.4.2 (PBR)**, **2.4.4 (IBL)** e **2.4.6
(pós-processamento)**; não foi implementado nesta integração.

A sequência coerente é malha de esfera com normais/UVs, recursos de material,
iluminação/PBR verificável e então mapas de alta resolução com mipmaps,
compressão e orçamento. Uma textura RGBA8 8192² ocupa 256 MiB sem mipmaps;
16384² ocupa 1 GiB. Isso é apenas um mapa, antes dos buffers de importação.
A resolução residente deve respeitar capabilities e memória; não substituir
qualidade de iluminação por aumento indiscriminado de resolução. A esfera
será uma referência complementar, não substituta do critério M2 de cena
equivalente à Sponza, luzes/sombras, FPS e energia medidos.
