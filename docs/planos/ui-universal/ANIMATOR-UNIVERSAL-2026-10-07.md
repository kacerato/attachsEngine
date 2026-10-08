# Animator universal — autoria, runtime e evolução

Revisão de 07/10/2026. Repositório: `kacerato/attachsEngine`. O bloco desta revisão é o workspace contextual do Animator e a alimentação opcional por movimento físico. O proprietário reativou o ADB em 08/10/2026 e pediu validação antes da entrega; o relatório de aceite separa resultados do host e do aparelho. Workspace e vínculos físicos aceitos: 9/9 cenários do host, SDK Release e POCO F7/Android 16; 717 quadros de mecanismo revisados, gestos e salvar/reabrir conferidos. [Relatório de aceite](../../validacao/animator-universal-2026-10-07/REPORT.md). Este documento não declara U03/U06/U08 inteiros encerrados.

## Decisão estrutural

O Animator pertence a um objeto e anima uma raiz visual independente. Nenhuma regra exige jogador, cilindro, cápsula, esqueleto humano ou câmera. Clipes importados com canais de transform servem para portas, atuadores, objetos, câmeras e hierarquias articuladas, além de personagens. Não confundir isso com animação de qualquer propriedade: curvas arbitrárias de material/UI/áudio ainda requerem o contrato de tracks descrito abaixo.

O grafo ocupa a superfície inteira quando as propriedades estão fechadas. Uma única gaveta alterna entre parâmetros e propriedades da seleção. Selecionar estado/transição abre seu contexto; fechar a gaveta devolve espaço ao grafo. Camadas continuam acessíveis acima dele. Rolagem revela os oito movimentos e oito eventos sem criar páginas de configurações separadas.

A direção A da referência visual fornecida pelo proprietário orienta os novos ícones: silhueta preenchida branca, recorte angular, pequeno acento verde. Estado, transição, parâmetros, camadas, mistura, vínculo, enquadramento e duplicação são recursos vetoriais/atlas reais. O conceito inicialmente gerado com personagem é uma hipótese anterior à correção de escopo; não representa a abrangência do sistema nem prova a implementação.

## Cadeia implementada neste bloco

```text
Documento / histórico / archive Animator v2
  → parâmetros tipados e referências a objetos
  → parâmetros manuais OU vínculo com física existente
  → grafo: estado, transição ordenada, camada, mistura, eventos
  → amostras de clipes
  → SceneAnimator / pose / canais de transform
  → renderização da hierarquia existente

ScenePhysics (último passo resolvido)
  → corpo OU DynamicBodyMotor OU Character
  → velocidade medida / apoio
  → valor do parâmetro vinculado

Estado vivo → grafo, valores, mistura e diagnóstico
```

O corpo de movimento e a raiz animada são referências distintas. A fonte `motion_source` participa do contrato de referências do componente; zero significa o objeto proprietário. A máscara da camada é uma subárvore existente. O Animator não cria solver, malha, câmera, input ou esqueleto alternativos.

### Autoria e interação

- Barra compacta com ícones e nomes curtos; parâmetros/propriedades sob demanda.
- Arrasto de nós com um passo de Undo/Redo; cancelar restaura o original. Pinça aumenta/reduz e desloca o grafo sem modificar a autoria; o segundo dedo cancela um arrasto de nó ainda não confirmado.
- Duplicação preserva clipes, mistura, eventos e propriedades, com identidade/nome novos. Conexões não são copiadas automaticamente: pertencem à topologia da camada.
- Transições podem subir/descer na ordem. O runtime examina primeiro Qualquer estado, depois as saídas do estado atual; dentro de cada grupo vence a primeira elegível. Reordenação não altera essa precedência entre grupos.
- Mistura 1D/2D desenha pontos, posição amostrada e peso usando as mesmas funções matemáticas do runtime. Não é editor de curvas nem manipulador de IK.
- Parâmetros manuais Float/Int/Bool podem ser ajustados durante Play; gatilhos podem ser disparados. Esses valores são runtime, não defaults autorais e não entram no histórico. Alterações estruturais permanecem bloqueadas em Play.

### Vínculos opcionais

| Fonte | Tipo | Resultado |
|---|---|---|
| Script/manual | Float, Int, Bool, Gatilho | Contrato existente de scripts e valor padrão |
| Velocidade no plano | Float | Norma XZ da velocidade resolvida |
| Velocidade vertical | Float | Componente Y, preservando sinal |
| Apoio no chão | Bool | Estado de apoio do Character/Motor |
| Velocidade total | Float | Norma XYZ da velocidade resolvida |

Corpos comuns fornecem velocidade, mas não um estado universal de apoio. Escolher Apoio nesses corpos mostra diagnóstico e produz false. Character usa velocidade relativa ao apoio; o Motor subtrai velocidade da plataforma quando apoiado. Assim, plataforma carregando um objeto parado não exige fingir intenção de caminhada.

Float vinculado permite escala entre −100 e 100 e resposta exponencial entre 0 e 10 segundos; 0 é imediato. Bool não é suavizado. O filtro usa tempo físico escalado. Fonte inexistente, inativa, mundo incorreto ou física parada zera o vínculo e expõe diagnóstico. Setter manual de parâmetro vinculado é recusado, evitando duas autoridades competindo pelo mesmo valor.

**Ordem real:** o pipeline atual avalia animação antes do passo físico daquele quadro. A leitura corresponde ao último passo físico concluído, podendo apresentar um quadro de atraso. Esta revisão não reposiciona scripts/animação/física e não anuncia root motion.

### Persistência e compatibilidade

Payload do componente sobe de 1 para 2. V1 migra para fonte Manual, escala 1, resposta 0 e corpo igual ao proprietário; não altera gráficos antigos para movimento automático. V2 salva fonte, resposta, escala e referência de corpo. Leitores antigos não conhecem v2: guardar cópia de cenas antes de abrir/salvar no Dev. Defaults de parâmetros e posições de nós são autoria; valores ao vivo, navegação e seleção são estado temporário.

Limites continuam explícitos: 32 parâmetros, 4 camadas, 24 estados e 48 transições por camada, 8 movimentos e 8 eventos por estado, 4 condições por transição. Não são promessas de escala ilimitada.

## Roadmap universal, por cadeias completas

### A. Locomoção e contato — U03/U06

Fechar modos de solo/ar, agachar e dimensões dinâmicas com teto/overlap, degrau, inclinação, snap e apoio móvel nos dois motores existentes. Corpo e colisão preservam a geometria autorada; primitiva é uma opção de colisão, não uma conversão obrigatória. Cada modo precisa de estado medido, propriedade efetiva, transição de autoridade, persistência, inspector e cenário com obstáculos. A câmera usa o sistema de colisão existente, independente do Animator. Prioridade: bloqueio por parede, salto/ar e plataforma, antes de multiplicar presets.

### B. Animação geral e reutilização — U08

Recurso de controller reutilizável com identidades estáveis e overrides por instância; duplicação/arquivo/importação/prefab/reimport preservam ligações. Submáquinas e estados aninhados exigem caminho estável e debug de navegação. Interrupção de transição exige pose capturada, regras de prioridade e consumo único de gatilhos/eventos. Camadas aditivas exigem pose de referência, quaternion relativo e máscaras; não basta adicionar enum.

Clipes precisam de trim, rate, loop, nome, ciclo, marcadores e curvas com edição/preview/save/reimport. Timeline/sequencer exige tracks tipados, bindings persistentes, scrub determinístico, relógio, cancelamento e restauração; integra transform, morph, propriedade suportada de material/UI/áudio/câmera e eventos, mediante consumidores reais. Não aceitar `Any` ou strings sem validação como sistema universal.

### C. Corpo, rig e aparência

Retargeting requer descrição de rig, pose de referência, orientação/escala e diagnóstico de ossos ausentes. Root motion requer extração de trajetória e submissão ao motor/solver, preservando colisão e dono do transform; não pode mover o nó renderizado à margem da física. IK/constraints requerem execução antes/depois de pose com ownership explícito, alvos editáveis, gizmos, limites, máscaras e ordenação. Contato de pés exige apoio real, fase do clipe e correção limitada, não offsets de um personagem de demonstração. Aparência combina materiais/morphs/anexos com slots persistentes e reimport preservando overrides.

### D. Ferramentas e SDK

Configuração tipada de bindings e controllers via SDK, recursos compartilhados, override/reload e diagnóstico por instância. Hoje scripts existentes leem parâmetros vinculados e dirigem parâmetros manuais; a configuração aninhada de fonte/resposta/escala é feita pelo editor/serialização. Busca/filtros do grafo, seleção múltipla, copiar grupo/conexões, menus contextuais, favoritos, zoom para seleção e curva de transição devem nascer de operações de documento, não de estado descartável de UI. Debug deve mostrar origem de valor e razão da transição, integrando eventos sem duplicar log por quadro.

### E. Escala e qualidade

Medir muitas instâncias, rigs e clipes no Release; cachear geometria de mistura por revisão do controller, evitar reconstrução de vetores no hot path, política de atualização/LOD/culling coerente com eventos e autoridade física. Profiler precisa separar avaliação, sampling, skinning e CPU/GPU. Rede/determinismo/replay exigem contrato próprio de estados, gatilhos, relógios e snapshots; não são consequência de ter um grafo.

Cada pacote acima fecha modelo → consumidor → propriedades → persistência → lifecycle → editor → criação → debug → cenário de aceite. Recursos pesquisados não contam como implementados.

## Referências concretas e adaptação

- Unity 6000.0 [Animator Controller](https://docs.unity3d.com/6000.0/Documentation/Manual/class-AnimatorController.html), [transições](https://docs.unity3d.com/6000.0/Documentation/Manual/class-Transition.html), [misturas](https://docs.unity3d.com/6000.0/Documentation/Manual/class-BlendTree.html) e [camadas](https://docs.unity3d.com/6000.0/Documentation/Manual/AnimationLayers.html): seleção determina propriedades; parâmetros alimentam comportamento; ordem de transições é contrato. Não copiar docks fixos para mobile.
- Godot 4.5 [AnimationTree](https://docs.godotengine.org/en/4.5/tutorials/animation/animation_tree.html) e [código do editor de máquina de estados](https://github.com/godotengine/godot/blob/4.5-stable/editor/animation/animation_state_machine_editor.cpp): seleção de nó/transição envia o recurso contextual ao inspector; movimento confirmado usa Undo/Redo; reprodução tem estado separado da autoria.
- Unity 6000.0 [Animator.SetFloat com damping](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Animator.SetFloat.html) e [Rigidbody.linearVelocity](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Rigidbody-linearVelocity.html): parâmetro suavizado e movimento físico são contratos distintos. A adaptação Astra liga explicitamente os consumidores existentes e documenta seu filtro exponencial e sua ordem de atualização, sem alegar equivalência matemática com o damping da Unity.
- Tutorial oficial [Unity, Creating and configuring blend trees](https://learn.unity.com/tutorial/3-4-creating-and-configuring-blend-trees/unity-version?version=2019.4), versão 2019.4: referência de fluxo localizada/consultada. Não houve análise quadro a quadro do vídeo nesta revisão; não usar como prova de comportamento observado.

## Aceite do bloco

Grafo de objeto geral → selecionar → duplicar → Undo/Redo → salvar/reabrir → rolar todas as propriedades → pinça sem alterar autoria. Em runtime, clipes reais alteram pose; corpo comum e ambos os motores fornecem medição/apoio; ausência de fonte produz diagnóstico; setter concorrente é recusado. Capturas são feitas pelo executável nativo com atlas real. Build/host e aparelho são gates separados; relatório em `docs/validacao/animator-universal-2026-10-07/` registra resultados finais, sem transformar build em aceite Android.
