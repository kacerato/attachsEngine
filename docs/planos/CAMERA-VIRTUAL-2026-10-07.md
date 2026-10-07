# Câmera virtual e Cérebro de câmera (bloco G, F023)

Dois componentes da família Câmera, subfamília Câmera virtual, avaliados por `runtime/scene_virtual_cameras.h` em cada quadro de Play: depois da física, de LateUpdate, dos percursos, tweens, constraints e de Acompanhar alvo, para ler as poses finais dos alvos.

| Componente | Id | Referência (Cinemachine 3.1, Unity 6000.0) | O que faz |
|---|---|---|---|
| Câmera virtual | `astra.camera.virtual` v1 | [CinemachineCamera](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineCamera.html) | Calcula pose e lente: posição (fixa, Seguir, Órbita), rotação (fixa, olhar para o alvo, rotação do alvo), desoclusão, inclinação holandesa e tremor |
| Cérebro de câmera | `astra.camera.brain` v1 | [CinemachineBrain](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineBrain.html) | Mora no objeto com Câmera; mostra a câmera virtual ativa de maior prioridade e mistura pose e lente na troca |

Referências dos comportamentos: [Orbital Follow](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineOrbitalFollow.html), [Deoccluder](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineDeoccluder.html), [Basic Multi Channel Perlin](https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineBasicMultiChannelPerlin.html).

## Contrato

- **Escolha da câmera ao vivo:** maior prioridade entre as câmeras virtuais ativas (objeto ativo na hierarquia, componente ligado). Empate: a ativada por último (reativar ou chamar Priorizar conta como ativação). Empate na largada: menor ID.
- **Pose crua × correções:** a câmera virtual grava no próprio objeto só a pose que ela decide (seguir/órbita + mira), como a CinemachineCamera faz com o transform. Desoclusão, inclinação e tremor entram apenas no estado entregue ao Cérebro e nunca realimentam o objeto.
- **Transição:** curva da câmera que entra (Corte, Suave, Linear, Entrada/Saída suave, Entrada/Saída brusca) ou a padrão do Cérebro. A primeira câmera entra em corte. Transição interrompida parte do quadro mostrado naquele instante; se a câmera de origem sai do ar, a mistura parte da última pose que ela mostrou.
- **Lente:** campo vertical, meia altura ortográfica, próximo e distante da câmera virtual são escritos na Câmera do Cérebro (a projeção continua sendo da Câmera). O resolvedor de render lê a mesma Câmera: nenhum caminho paralelo de render.
- **Órbita:** ângulo horizontal, vertical (com limites) e raio em torno do alvo rastreado. Com "Controlar pela entrada de olhar", a ação Olhar do mapa de entrada gira os ângulos só enquanto a câmera está ao vivo; o valor fica no componente, onde scripts e tweens o leem.
- **Desoclusão:** esfera (raio da câmera) ou raio de luz do ponto de mira até a câmera, pelo mesmo `ScenePhysics` das consultas, ignorando o corpo do alvo e obstáculos a menos da distância mínima do alvo. Aproxima na hora e volta à distância livre com o amortecimento ao liberar.
- **Tempo:** amortecimentos, tremor e transições usam o tempo de jogo; o Cérebro com "Ignorar escala de tempo" passa todos ao tempo real.
- **Autoria preservada:** o mundo de Play recebe as escritas; o documento autoral não muda.

Métodos (scripts pela fachada gerada; Conexão de evento chama `prioritize` e `snap`, ids 26 e 27): `prioritize`, `snap`, `is_live` (câmera virtual); `live_camera`, `blending` (Cérebro). Eventos (Conexão 17–21, todos com objeto para filtro): `camera_activated`, `camera_cut`, `blend_finished` (Cérebro); `activated`, `deactivated` (câmera virtual).

Regras de composição: o Cérebro exige Câmera e não convive com Acompanhar alvo, Olhar, Câmera virtual, Corpo físico nem Personagem no mesmo objeto; a câmera virtual não convive com Corpo físico nem Personagem.

## Editor

- Criar → Básicos → **Câmera virtual**: órbita no objeto selecionado com mira e desoclusão, prioridade 10. Na mesma transação de histórico, se a cena não tem Cérebro, ele entra na câmera da cena (ou nasce uma "Câmera principal" com Cérebro). Desfazer remove tudo junto.
- Inspector: cartão de estado em ambos — na edição, o resumo (modo, mira, prioridade, alvo ausente em aviso); no Play, "Ao vivo"/"Em espera" e, no Cérebro, a câmera ao vivo, a de origem e a barra de progresso da transição.
- Viewport: frustum da lente da câmera virtual; selecionada, linhas até os alvos e o anel onde a órbita vai circular.
- Ícones novos no atlas: `component/virtual-camera`, `component/camera-brain`.

## Diferenças da referência

| Aspecto | Astra | Classificação |
|---|---|---|
| Comportamentos em componentes irmãos (Follow, OrbitalFollow, RotationComposer, Perlin, Deoccluder) | Propriedades de um só componente, com modos | Adaptação explícita |
| Standby Update (Never/Always/Round Robin) | Todas as câmeras ativas avaliadas a cada quadro (Always) | Adaptação explícita |
| Órbita Three Ring, recentralização, amortecimento por eixo | Esfera com raio único; amortecimento único | Pendente |
| Blend Hint (esférico/cilíndrico), Custom Blends por par | Interpolação linear de posição e esférica de rotação; curva por câmera que entra | Pendente / adaptação |
| Estratégias do Deoccluder (preservar altura/distância) | Só "Pull Camera Forward" | Pendente |
| Perfis de ruído | Perlin de 6 canais com amplitude e frequência | Adaptação explícita |
| Canais (Channel Mask), câmera física, lente com Dutch no Cérebro | Um Cérebro por câmera, sem canais; Dutch na câmera virtual | Pendente |
| Câmera virtual pela câmera da vista do editor (Solo/preview) | Não implementado | Pendente |

## Validação (07/10/2026)

- Host (`build/api-host`, MinGW): 10 testes `virtual_camera_*`/`camera_brain_*` — corte para a maior prioridade e lente na Câmera e no resolvedor de render; transição linear na metade aos 0,5 s com eventos `camera_activated`/`activated`/`deactivated` e `blend_finished`; transição interrompida parte do quadro mostrado; horizonte nivelado no meio de uma mistura entre olhares diferentes; empate e Priorizar; Seguir com amortecimento exponencial, Encaixar e Guinada do alvo; Órbita pela entrada só ao vivo e limites; desoclusão contra parede real do Jolt e retorno amortecido; tremor sem realimentar o objeto e congelado com escala zero, liberado com "Ignorar escala de tempo"; regras de composição e identidade de Conexão. Suíte nativa 1441/1445 (as 4 falhas anteriores conhecidas); suíte gerenciada 521/0.
- Android arm64: `aether_android` compilado; APK release.
- Aparelho (projeto `CameraVirtual-20261007`, sonda `VirtualCameraProbe.cs`): órbita ao vivo em corte, parada pela parede a 3,11 m do alvo; troca para a câmera aérea por prioridade com transição, eventos do Cérebro e da câmera virtual, pose exata e lente de 45°; volta à órbita — PASS (`docs/validacao/evidencias/virtual-camera-20261007/`). O app publicado (assinatura de distribuição, versionCode 5) não aceita atualização pelo APK local; a validação usou um pacote lado a lado (`dev.aether.editor.validacao`), sem tocar o app nem os projetos do usuário.
- Interface: captura no aparelho do Inspector (aba Geral com prioridade e alvos; cartão "Rastreia Jogador"; em Play, "Em espera" enquanto a aérea está no ar), do anel da órbita e do frustum na viewport. A primeira captura levou à reordenação (alvos na aba Geral, modo antes dos valores).
