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

Ver a seção de evidências no relatório da sessão; esta seção é atualizada com
números reais do aparelho quando medidos.
