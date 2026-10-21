# Separação de câmera e scripts publicados — 21/10/2026

Revisão de código em validação local; não é um APK distribuído nem aceite Android.
O proprietário pediu continuar sem aparelho em21/10. ADB suspenso. A revisão privada
111 `.20261021.4` já estava instalada antes dessa suspensão; seu desempenho em
movimento ainda não foi medido. O teto30/50 não comprova throughput.

## Câmera de autoria e câmera do jogo

Referência: [Unity6000.0 Game view](https://docs.unity3d.com/6000.0/Documentation/Manual/GameView.html),
consultada21/10/2026. Game view apresenta a saída das câmeras da aplicação; não deve
herdar silenciosamente a câmera de navegação autoral. A Astra encontrava essa
herança no Android, no GameView dos scripts e no preparo do Canvas de mundo.

A revisão exige uma Camera runtime válida para apresentar geometria3D. Sem ela,
a simulação continua; GameView conserva dimensões/DPI para Screen, mas câmera0 e
frustum inválido impedem projeções/rays fictícios. Drawstates runtime ficam ocultos
e sem sombras. Cada extração nova restaura visibilidade quando a câmera retorna;
não há câmera artificial nem alteração persistente do projeto.

No renderer Android, ausência seleciona clear preto+UI, usando o mesmo acquire,
fence, semáforos, submit e present. Esse caminho pula ambiente, streaming, LOD,
culling, skinning, água, sombras,3D, pós e HZB. Um binding neutro do atlas de fonte
mantém o descritor UI válido sem depender de depth transient ou de subpasses da
água. O retorno restaura view/sampler/dimensões reais e invalida caches. Cópias
de quads de mundo preparadas antes de um script remover a câmera são descartadas;
ScreenCanvas permanece.

Referência: [Unity uGUI2.0.0 Canvas](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/class-Canvas.html).
Screen Space Overlay funciona sem câmera. Canvas de mundo depende de projeção
válida; perda da câmera cancela sua captura e diagnóstico transitório desaparece
na recuperação. O aviso de ausência existe somente no Play do editor, após a UI
do jogo; o Player final mostra apenas a apresentação do próprio jogo.

Migração de uso: projetos que dependiam da vista autoral no Play devem criar ou
ativar uma Camera na cena. A ferramenta existente de alinhar câmera com a vista
permite enquadrar o jogo explicitamente. Stop conserva a navegação de autoria.

## Player carrega publicação de scripts, sem compilação autoral

Desativar autocompilação no Player revelou um bloqueio real: abrir o projeto
limpava o catálogo nativo, mas preparar ScriptBehavior exigia catálogo Current
antes de o runtime conseguir carregar o assembly publicado.

O bootstrap agora tenta uma vez por raiz/época, antes do Play: LoadPublished lê
a geração existente e seu schema; a sessão publica esse catálogo validado.
Não descobre/compila fontes, não cria uma geração e não executa ferramentas ou
análise de linguagem por frame no Player. Binding e execução do runtime continuam
ativos. Falha encerra autostart com diagnóstico, preservando arquivos.

O pacote já conserva `.astra/code/current` e a geração com Project.dll, Project.pdb,
schema.json e compiler.id, excluindo bin/obj. O loader exige o MVID exato do SDK de
scripting. Após mudança do SDK, reconstruir/publicar scripts no editor antes de
exportar com esse APK/template; uma publicação incompatível deve ser recusada.
Um QA sem ScriptBehavior não valida esse fluxo.

## Evidência e limites deste checkpoint

- 27 cenários managed de Behavior passaram, incluindo compilação/publicação real,
cópia apenas da geração para outra raiz, remoção das fontes e execução de callbacks.
- 3 testes Java de export-core passaram, incluindo roundtrip dos cinco arquivos de
publicação e exclusão de bin/obj.
- 312 modos Vulkan anteriores passaram com suas comparações especializadas/genéricas;
a mesma execução acrescentou3 readbacks de clear/UI/câmera e2 cenários de depth com
dois subpasses, em2,19s com cache aquecido. Não somar isso como novos modos Terrain.
- A fixture de GPU valida operações de attachments/layout/cópia e pixels de overlay;
não instancia UiRenderer Android, swapchain real ou binding depth real do app.
- A suíte nativa Player/Terrain e o preview executável estão em compilação serial.
Ainda faltam resultado nativo, captura real da UI revisada, build/auditoria do novo
APK, Player exportado e validação Android de câmera/descritores/lifecycle/scripts.

Fontes de evidência locais em `build/terrain-evidence/terrain111-player-*` e
`terrain111-camera-absence-gpu-*`; não publicar logs ou fontes privadas de plugins.
As próximas Atualizações públicas continuam pendentes do fechamento da entrega.
