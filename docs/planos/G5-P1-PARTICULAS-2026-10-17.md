# G5 · P1 — Sistema de partículas (entrega de 17/10/2026)

Pacote P1 do bloco G5 do plano `DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md` (§5 e G5).
Este documento separa **conceito**, **implementado** e **testado**; números de
aparelho só entram quando medidos.

## Referências estudadas (capacidade, não cópia)

| Capacidade | Referência oficial |
|---|---|
| Módulos Main, Emission, Shape, Color/Size/Rotation over Lifetime, Renderer | [Unity 6000.0 · Particle System modules](https://docs.unity3d.com/6000.0/Documentation/Manual/ParticleSystemModules.html), [Main module](https://docs.unity3d.com/6000.0/Documentation/Manual/PartSysMainModule.html), [Emission](https://docs.unity3d.com/6000.0/Documentation/Manual/PartSysEmissionModule.html), [Shape](https://docs.unity3d.com/6000.0/Documentation/Manual/PartSysShapeModule.html) |
| Min/Max (constante, curva, entre duas) | [Unity 6000.0 · ParticleSystem.MinMaxCurve](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/ParticleSystem.MinMaxCurve.html), [MinMaxGradient](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/ParticleSystem.MinMaxGradient.html) |
| Rajadas | [Unity 6000.0 · ParticleSystem.Burst](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/ParticleSystem.Burst.html) |
| Play/Stop/Pause/Clear/Emit, Stop Action | [Unity 6000.0 · ParticleSystem](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/ParticleSystem.html), [MainModule.stopAction](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/ParticleSystem.MainModule-stopAction.html) |
| Emitir na vista do editor, material embutido | [Godot 4.5 · Particle systems (3D)](https://docs.godotengine.org/en/4.5/tutorials/3d/particles/index.html) |
| Propriedades de material (centro brilhante, ruído, máscara, partícula suave) | Varredura do RPG VFX Bundle 5.0 (§5 do plano): só a lista de propriedades de `Blend_CenterGlow`/`Add_CenterGlow`; nenhum código ou textura do pacote |

A hierarquia da Unity (ParticleSystem + ParticleSystemRenderer + Material) não foi
copiada: renderer e material de partícula vivem no mesmo componente.

## Implementado

- **Componente** `astra.effects.particle_system` v1 (`native/scene/particle_system.h`),
  família nova **Efeitos**: 97 números, 9 booleanos, 22 listas refletidos pelo gerador
  de contratos (`tools/component_contracts/properties_particle_system.json`), 18 curvas
  e 4 gradientes Min/Max endereçados por id (`start_lifetime.max` …), até 4 rajadas
  refletidas (`burst_<n>_*`). Arquivo chave-valor com id estável; id desconhecido ou
  versão futura são recusados; ids ausentes ficam no padrão.
- **Simulação** (`native/renderer/particles/particle_simulation.*`): SoA, PCG32 por
  semente (manual ou derivada do objeto), curvas/gradientes cozidos em tabelas de 64
  amostras, 6 formas (esfera, hemisfério, cone base/volume, caixa volume/casca/arestas,
  círculo, aresta) com raio, espessura, ângulo, arco, deslocamento e direção
  aleatória/esférica; espaço local/mundo; gravidade; pré-aquecer; atraso; ordenação
  por distância/idade; meia precisão para cor HDR.
- **Mundo da cena** (`native/runtime/scene_particles.*`): reconcilia com o grafo
  (documento na prévia do editor, GameWorld no Play), teto por nível do aparelho
  (C 1.000, B 8.000, A 30.000, S 100.000) repartido sem alterar o valor autoral, um
  emissor por job no `core/job_system` acima de 1.500 vivas, ações ao parar (desativar,
  destruir, evento Parou).
- **Operações** Play, Stop, Stop e limpar, Pause, Clear, Emit, IsPlaying,
  ParticleCount, Time pelo `runtime/component_operations` (scripts e conexões) e
  fachada C# `Astra.Components.ParticleSystem` gerada.
- **Renderização** (`native/platform/android/instanced_particles.inl`,
  `rhi/shaders/particle_billboard.vert`, `particle_material.frag`): passe próprio
  depois da cena, billboard e billboard esticado, mistura e aditivo, atlas de
  texturas próprias (8 formas, 3 ruídos, 3 máscaras), rolagem de UV, distorção por
  ruído, emissão HDR, brilho central, partícula suave pela profundidade da cena,
  tamanho máximo na tela; timestamps de GPU próprios do passe.
- **Editor**: receita "Efeitos › Partículas" (cone já apontando para cima pela rotação
  da forma), ícone novo `component/particle-system` no atlas, inspetor com os módulos
  em ordem (cada valor sob o seu modo, curva/gradiente sob o valor), linhas de curva
  e gradiente que abrem os editores existentes e gravam no componente em um passo de
  histórico, linha de prévia com estado, vivas, teto e reiniciar.
- **Capacidades** em `core/engine_capability.h`: `render.particles.simulation`,
  `.billboard`, `.material` (Implemented), `.soft` (DeviceLimited), `.mesh`,
  `.trails`, `.collision`, `.compute` (Planned).

## Limites declarados do P1

- Textura do projeto como textura principal não existe ainda (vai com Texture Sheet, P2).
- Partículas desenham depois de toda a transparência da malha e não escrevem as
  máscaras de reatividade do TAA.
- O frame graph passa a guardar a profundidade da cena no modo cena
  (`particleDepthEnabled`), mesmo sem pós dedicado; com pós dedicado (caso comum) nada muda.
- Meia resolução no nível C, compute, LOD de emissor: P5.

## Testado

### Host (Windows, MinGW)

- `aether_particle_tests`: 10/10 — registro e contrato de propriedades, ida e volta do
  arquivo (curvas, gradientes HDR, rajadas, identidade), recusa de versão futura/id
  desconhecido/valor fora do domínio, determinismo por semente (bytes iguais), taxa,
  rajadas com ciclos/probabilidade/atraso/repetição, Play/Stop/Pause/Clear/Emit e fim,
  6 formas dentro do volume, curvas/gradientes ao longo da vida, ordenação, teto por
  nível repartido, texturas determinísticas e sem costura.
- `aether_curve_gradient_family_tests`: 6/6, incluindo
  `particle_editor_creates_edits_curve_saves_and_previews` (receita Efeitos › Partículas,
  curva de Tamanho no tempo pelo editor de curvas em um passo de histórico, desfazer,
  salvar/reabrir, prévia emitindo).
- Fachada C# gerada compila (`dotnet build managed/Astra.Scripting`, 0 aviso).
- Android arm64 (clang, `-Werror`): `aether_android` compila.

### POCO F7 (Astra Dev versionCode 99, 17/10/2026, estado térmico 0)

Fluxo feito pela tela, sem script: projeto novo `ParticulasG5` → Criar › Efeitos ›
Partículas (já emite na prévia) → editado até virar **fogo** (vida 1,1 s, velocidade
1,8, tamanho 0,9, gravidade −0,25, taxa 45/s, cone 12°/0,25 m, Cor no tempo com
gradiente branco→laranja→vermelho editado no editor de gradiente, Aditivo, textura
Chama, emissão 2,5, ruído Nuvens rolando −1,2/s com recorte 0,7 e distorção 0,15,
brilho central 1,5, partícula suave 0,4 m) → segundo sistema editado até virar
**faísca** (rajada de 40–60, vida 0,7 s, velocidade 6, gravidade 1,2, cone 40°,
Billboard esticado 0,05 s, Aditivo, textura Faísca, tinta âmbar, emissão 4) → app
reiniciado abrindo o projeto: os dois voltam com os valores → Play executa → 9
desfazer restauram o estado após a bancada. Capturas em
`docs/validacao/evidencias/g5-p1-particulas-20261017/`.

Custo medido pela linha `[Particles]` do logcat (timestamps próprios do passe na GPU;
CPU = simulação + montagem do quadro, thread do editor), ~10 amostras de 2 s cada,
câmera parada, perfil do aparelho com teto 8.000. Não é o protocolo A/B/B/A da bancada:

| Cena | Vivas | GPU do passe | CPU (sim + montagem) |
|---|---|---|---|
| Faíscas pequenas (esticadas, aditivo) | 1.000 | 0,24–0,36 ms (mediana ~0,26) | ~0,02–0,03 + 0,05–0,10 ms |
| Faíscas pequenas | 8.000 | 0,83–0,96 ms (mediana ~0,87) | ~0,07–0,24 + 0,33–0,67 ms |
| Fogo autorado (quads grandes, ruído, suave) | 50 | 0,18–0,24 ms | ~0,01 + 0,01 ms |
| Fogo com 1.000 partículas grandes | 1.000 | 3,1–3,3 ms | ~0,02 + 0,06–0,18 ms |

Leitura: o passe tem custo fixo de ~0,2 ms (carregar/guardar cor e profundidade num
render pass próprio) e o resto é preenchimento. 1.000 partículas pequenas cabem no
orçamento do nível C (≤ 1,5 ms); 1.000 partículas grandes não — é exatamente o
caso do P5 (meia resolução no nível C, LOD/tamanho na tela). Nada disso foi medido
num aparelho de nível C.

### Não testado

- Validação Vulkan (o APK é release, sem camada de validação).
- TAA/ampliador temporal ligado junto com partículas (máscaras de reatividade não escritas).
- Script C# chamando Play/Stop/Emit no aparelho (coberto só pela fachada gerada e pelas operações no host).
