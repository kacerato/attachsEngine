# Prefab seletivo e restrições — decisões e evidência

NÃO IREI SER SIMPLISTA NO DESIGN.

O fluxo de comparação já ocupava uma superfície ampla temporária. Esta rodada usa essa rota para comparar base salva, instância e fonte, mantendo o retorno ao objeto. Não acrescenta um dock permanente nem comprime mais o viewport. A skill [avoid-ai-design](C:/Users/donod/.agents/skills/avoid-ai-design/SKILL.md) foi aplicada no perfil inside-design-system: tokens Astra preservados, densidade e consequências das ações explícitas.

## Caminho de uso

Objeto → comparar fonte → diferença → Reverter aqui ou Aplicar fonte. Origem Local, Herdado ou Conflito permanece junto ao endereço. Aplicar publica somente esse endereço na fonte e nas instâncias que o herdam; Reverter atua nesta instância. Herdado oferece receber, sem uma falsa ação de publicação. Conflito oferece Usar fonte aqui ou Substituir fonte, com explicação explícita antes do toque. A API recebe overwriteConflict somente na segunda escolha.

O backend fornece applyable e applyReason; indisponibilidade por estrutura, aninhamento, fonte ausente ou contrato inválido não é inferida pelo desenho. A interface conserva a comparação depois de uma falha de operação. Comparações antigas bloqueiam ações até Ler fonte. O relatório da operação é mostrado pela sessão; o estado editorial não é fonte de verdade da publicação.

Adicionar componente → buscar Constraint → escolher restrição → Fonte/Peso → Ajustes. Position, Rotation, Scale e Aim usam o schema e os setters existentes. Fonte e habilitação compartilham uma linha; Peso conserva a segunda, para as decisões essenciais permanecerem próximas. Eixos XYZ compartilham uma linha, com widgets booleanos individuais, seleção múltipla e reset de cada canal. A busca continua mostrando propriedades individuais. O seletor Fonte reserva espaço para a lupa e abre o picker real. Fonte obrigatória ou incompatível usa referenceReadiness; diagnósticos em Play vêm do SceneConstraints publicado pela sessão, sem diagnóstico cenográfico. A implementação Astra desta rodada usa uma fonte e canais de mundo; não promete a lista de fontes ponderadas nem Activate/Lock da Unity.

Cinco ícones próprios foram desenhados no pipeline SVG/raster/atlas: vínculo e translação, arco de rotação, expansão de escala, alvo de mira e força contínua. As identidades são component/position-constraint, rotation-constraint, scale-constraint, aim-constraint e constant-force. O atlas passou de 201 para 206 entradas.

Força constante preserva quatro vetores refletidos. As primeiras capturas revelaram quatro abas com nomes cortados e o vetor principal escondido pelo Ativo duplicado. A correção visual usa Mundo/Local como dois destinos: força e torque juntos em cada espaço. Cada vetor usa o editor XYZ existente; nenhum número novo é criado. Ativo permanece no checkbox real do cabeçalho e continua disponível na busca individual. Busca ociosa vira lupa na nota de dependência; o picker e os endereços numéricos permanecem reais. As capturas posteriores ao último build confirmam força e torque juntos, sem paginação, com títulos Força (N) e Torque (N·m) legíveis. O espaço de referência vem da aba, evitando redundância nos títulos.

## Referências observadas

- [Unity 6000.0 — Override prefab instances](https://docs.unity3d.com/6000.0/Documentation/Manual/PrefabInstanceOverrides.html): overrides locais têm precedência sobre alterações da fonte; distinguir origem evita interpretar uma instância preservada como erro de propagação.
- [UnityCsReference 6000.0 — PrefabOverridesTreeView](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Editor/Mono/Prefabs/PrefabOverrides/PrefabOverridesTreeView.cs): identidades de objetos/propriedades, tipos de mudanças estruturais e aplicabilidade são separados da seleção. A Astra usa endereços próprios e preflight do backend.
- [Unity 6000.0 — Position Constraint](https://docs.unity3d.com/6000.0/Documentation/Manual/class-PositionConstraint.html), [Rotation Constraint](https://docs.unity3d.com/6000.0/Documentation/Manual/class-RotationConstraint.html), [Scale Constraint](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ScaleConstraint.html) e [Aim Constraint](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AimConstraint.html): fonte, influência, offsets e canais são relações de transformação, não simples campos de navegação.
- [UnityCsReference 6000.0 — PositionConstraintEditor](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Editor/Mono/Inspector/PositionConstraintEditor.cs): configuração agrupa offsets e canais. A Astra mantém controles refletidos e compacta XYZ por significado, sem copiar densidade de mouse.
- [Unity 6000.0 — Constant Force](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ConstantForce.html): forças/torques em espaço de mundo e relativo ao corpo. A UI separa o espaço de referência, conservando força e torque no mesmo contexto.
- [Creating And Editing Prefabs in Unity — Gregory Osborne, 9:00](https://www.youtube.com/watch?v=cbjRdGzgjVc&t=540s): trecho reproduzido e pausado no navegador na rodada anterior; transcrição 8:23–9:24 também lida. Inspector retém Capsule Collider removido e Add Component abaixo da composição. Esta rodada reutiliza essa observação para distinguir origem e alteração estrutural; não afirma ter observado novamente nem identificar a versão precisa do vídeo.

## Validação

Após build host consolidado pelo agente principal, quinze estados foram capturados e inspecionados em 853×394 e 1200×700. Todos registraram zero fontes ausentes e instâncias descartadas. Estados no telefone registraram zero recortes; os três recortes nas imagens amplas pertencem ao viewport. Prefab mantém comparação completa e ações de 40 pixels no telefone; Fonte/Peso e eixos/deslocamento cabem juntos nos respectivos grupos. Aim ocupa duas páginas de Ajustes; busca de axis oferece os canais individualmente. O catálogo mostra os quatro ícones próprios e reconhece o tipo já anexado. Fonte ausente aparece como obrigatória.

Diretório: `docs/validacao/evidencias/inspector-components-20260930/`, nomes terminados em `-wave2.png`. Capturas documentam código executável e fixtures com schemas reais. Este agente não executou testes de pointer/mixed/Undo; esses comportamentos são consolidados nos testes funcionais do agente principal. Não há declaração de validação visual no aparelho e nenhum ADB foi executado. O estado saudável em Play só pode vir do diagnóstico runtime; as capturas de authoring não provam a execução da constraint.

![Prefab seletivo, conflito](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/prefab-selective-conflict-wave2.png)
![Constraints, influência](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/constraint-influence-wave2.png)
![Constraints, ajustes](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/constraint-adjustments-wave2.png)
![Constraints, catálogo](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/constraint-catalog-wave2.png)
![Força constante, Mundo](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/constant-force-wave2.png)
![Força constante, Local](C:/Users/donod/Downloads/atchengine/docs/validacao/evidencias/inspector-components-20260930/constant-force-local-torque-wave2.png)
