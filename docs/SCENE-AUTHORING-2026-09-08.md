# Autoria de cenas: correções e limites em 08/09/2026

## Problemas corrigidos

O botão de parar desenhava um quadrado, mas executava a mesma atribuição de estado que iniciava a simulação. Agora alterna execução/edição, mantendo o documento. O caminho do editor não inicializa nem recebe entrada do personagem da demonstração; seu joystick e contador não aparecem automaticamente.

Câmeras podem ser criadas pelo catálogo, partindo da vista atual. Sua posição e direção em execução vêm da transformação mundial da entidade, incluindo ancestrais ativos. Entre várias câmeras ativas, vence o menor ID estável. Sem câmera, a simulação conserva a vista de edição. Ainda faltam prioridade explícita, lente/projeção por componente e suporte a rotação de câmera em Z.

A projeção de linhas faz recorte homogêneo antes da divisão perspectiva. A cobertura da grade não troca abruptamente ao cruzar um limiar no horizonte; linhas subpixel são atenuadas. O editor usa fundo neutro, preservando o ambiente usado pela cena em execução.

A física de água deixa de construir espelhos espectrais CPU quando não existem corpos ativos e visíveis a consultar; não recalcula o mesmo campo ao final de cada quadro. Isso não estabelece uma meta de desempenho para cenas complexas.

## Seletor e hierarquia

O seletor usa catálogo único de operações implementadas, categorias (Básicos, Geometria, Água, Física), pesquisa sem distinção de acentos, seleção em linhas e ação Criar separada. Descrição aparece somente para a seleção. A pesquisa percorre categorias e tem paginação. A janela não apresenta componentes futuros como se estivessem disponíveis.

Entradas atuais: objeto vazio, câmera, cubo, chão, superfície de água, oceano, rio por pontos, caixa flutuante. Criar chão/cubo registra nome, escala e estado físico na mesma transação de criação. Água finita é enquadrada depois de criada. O oceano usa enquadramento local, evitando afastar a câmera pelo tamanho da malha extensa.

A hierarquia mantém operações existentes de reparentear preservando transformação mundial, ordenar, duplicar, excluir, renomear, visibilidade, seleção e desfazer/refazer. Acrescenta pesquisa com contexto dos ancestrais e expandir/recolher. Não é uma implementação completa de seleção múltipla, prefabs ou painel de componentes.

A interface nativa decodifica UTF-8 de forma compartilhada na medição, truncamento e desenho; o atlas inclui Latin-1 para português. Rótulos do editor e do lançador foram traduzidos. Nomes de arquivos, APIs e projetos existentes não são traduzidos automaticamente.

## Referências consultadas

- [Godot: nós, cenas e seletor](https://docs.godotengine.org/en/stable/getting_started/step_by_step/nodes_and_scenes.html): composição da árvore, pesquisa, seleção antes da criação.
- [Godot 4.4: CreateDialog](https://github.com/godotengine/godot/blob/4.4/editor/create_dialog.cpp): lista alimentada pelo registro de tipos, filtragem de tipos instanciáveis e organização visual em painéis. Fonte sob MIT; a implementação desta mudança é própria, sem incorporação de trechos.
- [Stride: adicionar entidades](https://doc.stride3d.net/latest/en/manual/game-studio/add-entities.html): autoria de entidades e organização no editor.
- [Stride: entidades e componentes](https://github.com/stride3d/stride-docs/blob/master/en/manual/stride-for-unity-developers/index.md): transformação da entidade e composição de comportamentos.
- [Wicked Engine: cena e componentes](https://github.com/turanszkij/WickedEngine/blob/master/Content/Documentation/WickedEngine-Documentation.md): cena como coleção de componentes e identidade compartilhada. Repositório MIT; não foi importada dependência.

O catálogo de criação do editor não substitui o registro de componentes em managed/Aether.Core/Serialization. Integrar esses contratos requer uma projeção explícita, não outro registro paralelo de componentes de gameplay.

## Validação

- 720 testes nativos passaram, incluindo parar pelo botão, preservar documento, câmera explícita/desfazer, busca na hierarquia, recorte extremo e fronteiras UTF-8.
- Compilações Android Debug e Release e testes Java passaram.
- No aparelho: cena isolada em files/validation-scene-foundation, criação de superfície de água via catálogo, executar sem joystick e parar retornando à hierarquia. Capturas em build/editor-host/scene-*-device.png.
- Durante essa cena simples, o contador do sistema Android registrou aproximadamente 58–60 FPS com alvo de 60. Não é um benchmark GPU isolado nem valida floresta, oceano completo ou carga de corpos físicos.

## Trabalho ainda necessário

Jogador composto por controlador, colisão, ações de entrada e joystick criado como nó; propriedades de câmera; ambientes e materiais como recursos independentes; colisores estáticos do cenário; integração do Inspector com o registro runtime; importação individual de recursos fora das bibliotecas de demonstração; seleção múltipla/prefabs na hierarquia. Chão é uma malha nesta entrega, não um colisor estático funcional. As limitações de óptica/efeitos e NoCode de WATER-AUTHORING.md continuam abertas.
