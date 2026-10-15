# Estúdio de animação v2 e Biblioteca como gerenciador — proposta e execução

Data: 15/10/2026. Origem: revisão do proprietário após o code 81 — "validar com
teste de host é fácil; no editor é outro. Montar uma animação hoje é difícil
para quem não sabe nada e sem praticidade; o problema não é o número de
funções, é como são aplicadas. A aba Biblioteca não faz sentido: botão de
importar solto, abre tudo na tela. Não gosto de texto informativo."

NÃO IREI SER SIMPLISTA NO DESIGN.

## 1. O que a captura real mostra (Astra Dev code 77–79, Kyle Tutorial)

Estúdio (`v77_c`, `v78_a`): abre com a aba Chave e um cartão "Canal · Posição
X" de uma trilha que o usuário não escolheu; o modo pose está desligado; a
coluna da direita é uma pilha de cartões com texto de estado ("Toque uma chave
na timeline…", "Arraste o alvo na ponta selecionada", "Desligado · toque para
ligar"). Para posar: aba Pose → "Modo pose" → tocar um osso → ferramenta →
arrastar → barra "Gravar chave / Cancelar". Criar clipe: "Novo" → escolher
"Quaternion / Euler / Progressivo" antes de existir qualquer pose. Quatro abas
(Chave, Rig, Pose, IK) escondem o essencial e expõem o avançado no mesmo nível.

Biblioteca (`v79_b`): lista plana de 23 itens com rótulos "Converter /
Diagnóstico / Abrir" em texto; "Importar pasta" como botão de texto no
cabeçalho; filtros por ciclo de toque; o diagnóstico vira uma página de texto.
"Recursos importados" é outro espaço com outra lista. Importar modelo mora no
"+" da Hierarquia; importar pasta mora na Biblioteca; Unity entra pela
Biblioteca; textura/HDRI entram pelo Ambiente. Quatro portas para a mesma ação.

## 2. Princípios (extraídos, não copiados)

UMotion 1.29 (Pose Editor / Clip Editor): posar é o estado normal; auto-key
"Generate"; chaves-mestras por quadro; lista de propriedades animadas com ícone
do tipo e seleção sincronizada com o osso; canais do osso selecionado como
campos editáveis com cor de "modificado"; transporte com início/fim/velocidade.
Unity 6000.0 Animation window: gravação (record) liga o auto-key e marca
em vermelho o que muda; a timeline só mostra as propriedades do objeto.
Godot 4.3 AnimationPlayer: a chave fica ao lado da propriedade no Inspector —
quem nunca animou entende "este valor, neste tempo".

Regras para a Astra em toque:
1. **Zero texto de estado.** Estado vira cor, ícone, badge ou controle
   desabilitado. Diagnóstico vira um chip com ícone no lugar onde o problema
   está (linha do osso, chave, cartão).
2. **Posar é o padrão.** Abrir o estúdio = tocar um osso e arrastar. Auto-key
   sempre ligado (modo "gerar"); soltar o gizmo grava e pulsa a chave na régua.
   Nada de "Gravar/Cancelar" por gesto: Desfazer é o cancelar.
3. **Um lugar para cada coisa.** Esquerda: ossos (árvore) com ícone de
   "animado/modificado". Centro: vista + timeline (régua, chaves-mestras,
   linhas do osso selecionado). Direita: *o osso selecionado* — canais
   Posição/Rotação/Escala com valor e botão de chave por canal (Godot), e abaixo
   as ferramentas de pose (espelhar, copiar/colar, repouso, fantasma, IK).
   Rig e export saem do fluxo principal (menu ⋮ do estúdio).
4. **Criar é uma ação, não um formulário.** "Animar" no menu de contexto do
   objeto e "Novo clipe" no estúdio pedem só o nome; duração 2 s e rotação
   quaternion são padrão; Euler/progressivo ficam no menu ⋮ do canal.
5. **Importar é universal.** Um botão na barra superior (ícone importar) abre o
   menu de contexto com: Modelo (GLB/FBX), Pasta de pacote, Textura ou HDRI,
   Unity (.anim/.prefab/.controller). A Biblioteca é o gerenciador do que já
   entrou: trilho de tipos à esquerda, cartões ao centro (ícone do tipo, nome,
   pacote, badge de estado), detalhes à direita com ações em ícone. "Recursos
   importados" e "Biblioteca" viram um só espaço com dois filtros de origem.

## 3. Estúdio v2 — layout e controles

```
┌ barra do estúdio: ← | nome do clipe ▾ | ⟲ ⟳ | ⏮ ◀ ▶ ▶ ⏭ | 1.52 s | ⋮ ┐
│ ossos        │ vista 3D (gizmo do osso, coluna de ferramentas)  │ osso    │
│ ▸ Root ●     │                                                   │ Posição │
│   Ribs ●     │                                                   │ x y z ◆ │
│   Neck       │                                                   │ Rotação │
│   Head ●     ├───────────────────────────────────────────────────┤ x y z ◆ │
│   L_Shoulder │ régua 0 ─ 0.5 ─ 1.0 ─ 1.5 ─ 2.0 ─ 2.5 ─ 3 s        │ ───────  │
│   L_UpperArm │ ◆ ◆   ◆     ◆   ◆  chaves-mestras                  │ ⟷ ⎘ ⎗ ↺ │
│   …          │ Head·Rotação  ◆      ◆                             │ ghost IK│
└──────────────┴────────────────────────────────────────────────────┴─────────┘
```

- **Árvore de ossos:** ponto verde = tem chaves; ponto âmbar = modificado sem
  chave (impossível com auto-key, mas vale para camadas mudas); toque seleciona
  e enquadra sob demanda (toque duplo). Busca por nome no cabeçalho.
- **Vista:** coluna de ferramentas Mover/Girar/Escalar/Enquadrar/Órbita/IK,
  como hoje; o nome do osso vira um chip sobre o gizmo, não uma faixa.
- **Timeline:** régua com arrasto; linha de chaves-mestras (toque seleciona
  todas do quadro; arrasto move); linhas do osso selecionado (Posição, Rotação,
  Escala) com chaves; toque longo numa chave abre o menu de contexto (tempo,
  valor, tangente, copiar, excluir). Zoom por pinça.
- **Painel do osso (direita):** canais com valor e **botão de chave** por
  canal/propriedade (estado: vazio = sem chave; cheio = chave neste tempo;
  contorno = chave em outro tempo). Abaixo, ferramentas de pose como grade de
  ícones: espelhar edição, copiar pose, colar pose, pose de repouso, fantasma,
  ossos visíveis, IK na ponta, fixar no intervalo. Biblioteca de poses como
  chips. Sem cartão de texto.
- **Menu ⋮ do estúdio:** Camadas, Rig, Root motion, Bake, Exportar GLB,
  Consolidar, Configurações do clipe (duração, taxa, loop, modo de rotação).
- **Abrir:** menu de contexto do objeto → "Animar" (cria "Clipe 1" se não há
  clipe; senão abre o último). Inspector › Animação › Clipes › "+" idem.

## 4. Biblioteca v2 — gerenciador

```
┌ Biblioteca                      🔍 buscar…                  [⤓ Importar ▾] ┐
│ Tipos        │ cartões                                   │ detalhe        │
│ ● Todos  23  │ ▣ PickUp.anim        UMotion · Clipe  ✓   │ ícone + nome   │
│ ◻ Modelos 3  │ ▣ Idle.anim          UMotion · Clipe  ↻   │ estado (badge) │
│ ◻ Clipes  4  │ ▣ Robot Kyle.fbx     UMotion · Modelo ✓   │ ações: ícones  │
│ ◻ Materiais  │ ▣ CubeMaterial.mat   UMotion · Material ⚠ │ dependências   │
│ ◻ Texturas   │ …                                          │ diagnóstico ⚠  │
│ ◻ Prefabs    │                                            │ como chips     │
│ ◻ Pacotes    │                                            │                │
└──────────────┴────────────────────────────────────────────┴────────────────┘
```

- Badges: ✓ pronto (nativo), ↻ converter, ⚠ diagnóstico, ⏳ preparando.
- Ações no detalhe como botões de ícone com rótulo curto: Abrir, Converter,
  Instanciar, Vincular, Reimportar, Manter local / Receber origem.
- O botão **Importar** é o mesmo da barra superior (universal).
- "Recursos importados" vira o filtro de origem "Projeto" dentro da Biblioteca;
  a aba antiga sai do menu Cena.

## 5. Execução por blocos (cada um com preview no host e captura)

E1. Estúdio: posar por padrão + auto-key + chaves-mestras + painel do osso
    com botões de chave + árvore com estado; retirar cartões de texto; "Animar"
    no menu de contexto; "Novo" pedindo só o nome. Rig/IK avançado/Bake/Export
    vão para o menu ⋮.
E2. Timeline: toque longo na chave abre menu de contexto; pinça para zoom;
    linhas do osso selecionado.
E3. Importar universal na barra superior (menu de contexto) e Biblioteca com
    trilho de tipos, cartões com badge e detalhe com ações em ícone; união
    com Recursos importados.
E4. Revisão dos plugins (UMotion, Final IK, BoZo, POLYGON) com o novo filtro:
    só entra o que tem fluxo completo no editor.

Hipótese visual: gerada por imagem a partir das capturas (ver pasta
`docs/planos/ui-universal/estudio-v2/`). A imagem não é evidência; cada
controle desenhado foi confrontado com modelo, serialização e ação antes de
codificar.

Referências: UMotion Pro 1.29p04 manual local (`PoseEditor`, `Animation`,
`Channels`, `Dopesheet`, `AnimatedPropertiesList`, `Playback`); Unity 6000.0
https://docs.unity3d.com/6000.0/Documentation/Manual/animeditor-AnimatingAGameObject.html;
Godot 4.3 https://docs.godotengine.org/en/4.3/tutorials/animation/introduction.html.
