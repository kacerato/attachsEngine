# G6 — Importação dos pacotes (RPG VFX Bundle e UniStorm)

Bloco G6 do plano `DESEMPENHO-GRAFICOS-VIVOS-2026-10-17.md` (§ G6). Entrega em 25/10/2026.

Licença: o dono tem licença company dos dois pacotes. Os pacotes são importados no projeto dele
pela Biblioteca; nenhum asset, código ou shader dos pacotes entra no repositório, no APK ou no
AstraDocs. Os testes usam fixtures sintéticas escritas aqui; o relatório publicado tem só contagens.

## Referências estudadas (capacidade, não cópia)

| Capacidade | Referência |
|---|---|
| Particle System por módulo, Render Mode Mesh, Render Alignment, 3D Start Size/Rotation, Scaling Mode, Collision World | [Unity 6000.0 — Particle System modules](https://docs.unity3d.com/6000.0/Documentation/Manual/ParticleSystemModules.html), [Renderer module](https://docs.unity3d.com/6000.0/Documentation/Manual/PartSysRendererModule.html), [Collision module](https://docs.unity3d.com/6000.0/Documentation/Manual/PartSysCollisionModule.html) |
| Partícula de malha | [Godot 4.5 — GPUParticles3D Draw Passes](https://docs.godotengine.org/en/4.5/tutorials/3d/particles/index.html) |
| Linha | [Unity 6000.0 — Line Renderer](https://docs.unity3d.com/6000.0/Documentation/Manual/class-LineRenderer.html) |
| Laser | [Physics.Raycast](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Physics.Raycast.html) + Line Renderer |
| Projétil | [Collider.OnCollisionEnter](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Collider.OnCollisionEnter.html) |
| Instanciar ao colidir | [MonoBehaviour.OnParticleCollision](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.OnParticleCollision.html) |
| Som repetido | [AudioSource.PlayOneShot](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/AudioSource.PlayOneShot.html) |
| Formato dos arquivos | [Unity — Format of text serialized files](https://docs.unity3d.com/6000.0/Documentation/Manual/FormatDescription.html) |

## G6a — RPG VFX Bundle 5.0 (Hovl Studio)

Universo: 217 prefabs, 1187 Particle Systems, 211 materiais, 12 shaders Hovl, 30 FBX, 15 scripts.

### O que a importação passou a ler

- **Particle System (198) + Renderer (199)** → `resources/unity_particle_import.cpp`, módulo a módulo:
  Principal, Emissão (taxa, distância, até 4 rajadas), Forma (todas as primitivas; a frente da
  Unity vira −Z local, como o objeto refletido pede), Cor/Tamanho/Rotação no tempo (com eixos),
  Folha de textura, Velocidade/Limite/Força/Ruído, Herdar velocidade, pela velocidade, Colisão
  (Planos e Mundo), Sub-emissores, Luzes, Trilhas e o Renderer (Billboard, Esticado, Malha,
  Nenhum, alinhamento, ordenação, tamanho máximo, materiais). Curvas e gradientes viram as curvas
  da Astra (radianos→graus, sRGB→linear, z refletido). Sem consumidor: perda listada no prefab.
- **Light (108)** → Luz (unidade Engine, a intensidade da Unity Built-in); **AudioSource (82)** →
  Fonte de áudio (WAV convertido na hora); **Rigidbody/Rigidbody2D e colisores primitivos** → corpo
  e colisor; **LineRenderer (120)** → Linha.
- **Materiais Hovl** (por GUID do .shader) → Material Partícula Unlit da Astra (imagem principal,
  cor, mistura ou aditivo) mais o que o componente guarda (brilho, rolagem, ruído, brilho central,
  partícula suave). Distortion não tem consumidor: o sistema simula sem desenhar (perda listada).
- **Scripts** (pelo nome do .cs): `AutoDestroyPS` → Ação ao parar = Destruir; `ProjectileMover(2D)`
  → Projétil; `ParticleCollisionInstance` → Instanciar ao colidir; `EGA_EffectSound` → Som
  repetido; `Hovl_Laser` → Laser. Outros scripts: perda listada, o prefab abre sem eles.
- **Cascata**: converter um prefab converte os materiais, WAVs, prefabs de impacto e as malhas FBX
  das partículas que ele usa, cada um com receita e histórico próprios.

### O que a engine ganhou para isso

- Sistema de partículas v4: Render Mode **Malha** (Quadrado, Cubo, Esfera, Cilindro ou Malha do
  projeto) e **Nenhum**; **Alinhamento** Vista/Mundo/Local/Velocidade; **Tamanho 3D** e **Rotação 3D**
  (com eixos no tempo); **Modo de escala**; **Colisão Mundo** (raio da física do Play, teto por sistema).
- Instância de partícula 48 → 64 bytes (orientação snorm16, altura e profundidade); pipeline de malha
  própria no passe de partículas (`particle_mesh.vert`), quadrado orientado no billboard.
- Componentes novos: **Linha**, **Laser**, **Projétil**, **Instanciar ao colidir**, **Som repetido**,
  com ícones novos no atlas e receitas em + › Efeitos e + › Áudio.

## G6b — UniStorm 5.3.1 (só dados)

- 32 Weather Types → **presets do Clima** (tipo, intensidade, neblina, vento, relâmpago, taxa de
  chuva/neve, superfícies) e, quando o tipo usa som WAV, a Fonte de áudio em laço na mesma receita.
- 3 Cloud Profiles → **perfis de Ambiente** com só as Nuvens (cobertura, densidade, cirros, opacidade).
- Partículas de chuva/neve do pacote: prefabs convertidos pelo caminho do G6a. Sons: WAV pela cascata.
- Scripts e shaders do UniStorm não são importados.

## Cobertura e aceite

Ver `docs/validacao/g6-importacao-2026-10-25/README.md` (contagens da ferramenta
`aether_unity_coverage`, testes de host e prancha Unity × POCO).
