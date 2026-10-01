# Forças contínuas e validação de UV1 — 30/09/2026

## Capacidade entregue

`ConstantForce` é um componente de autoria e execução com força e torque nos eixos do mundo e locais. O mesmo descritor publica doze números, quatro vetores e o interruptor Ativo para arquivo de cena, presets, prefab, edição em Play e fachada C#. Não escreve Transform: o Jolt integra o movimento e publica a pose pela autoridade física existente.

O fecho de dependências requer Corpo físico no mesmo objeto, e impede removê-lo isoladamente. Ativo requer movimento Dinâmico; a montagem física recusa estático/cinemático com mensagem contextual. Desligado permite manter esse rascunho. A receita `physics.propulsion` cria Corpo dinâmico + Colisor caixa + Força local Z de 4 N, massa 1 kg e gravidade desligada, como um comando de Undo/Redo. Não possui malha visual inventada.

Após FixedUpdate e seu ponto seguro, `ScenePhysics::advance` lê os valores atuais dos corpos existentes antes de **cada** passo de 1/60 s. Vetores locais usam o quaternion atual do corpo nativo, sem escala autoral. Mundo e local somam; massa e inércia são consumidas pelo solver. Não multiplique a força pelo delta: isso a converteria incorretamente antes de o integrador trabalhar. Força/torque zero não acordam corpos; valores não zero os acordam pela ponte física existente. Sem cache de componente com lifetime frágil, alocação ou varredura dos schemas no subpasso. Custo proporcional aos corpos já montados, até o limite vigente de 1.024 corpos.

O gizmo fornece seta da força resultante e eixo de torque com anel distinto; seu tamanho visual é limitado, sem representar Newtons como metros. A Inspeção reúne vetores XYZ em Mundo e Local, usando os controles, reset e endereços reais. O ícone `component/constant-force` está no atlas do produto.

## Lightmap: validar a dependência geométrica

O caminho externo de lightmap indireto já existente passa a recusar também **sobreposição de área em UV1**. `scene/lightmap_uv.h` usa recorte de triângulos em precisão dupla; arestas/vértices compartilhados continuam válidos. Detecta coordenadas não finitas/fora de 0..1, área degenerada e faces sobrepostas, independentemente do winding. A auditoria fica armazenada por geometria canônica na publicação do recurso; o renderer lê somente o resultado pronto.

Limites explícitos: 65.536 triângulos por desenho, 1.000.000 pares de caixas no sweep, área mínima de triângulo UV de 1e-12 e tolerância de interseção de 1e-14. Estourar orçamento produz `AnalysisLimit`, sem declarar sucesso. Layouts não confirmados não recebem amostragem de lightmap, e a aba exibe o motivo concreto. Pico auxiliar próximo de 4 MiB no limite por desenho; nenhuma comparação de triângulos entra no frame loop. Geometrias compartilhadas reutilizam a auditoria.

Esta etapa não cria unwrap, atlas ou bake, não certifica padding texel/mips nem sobreposição entre instâncias/submeshes distintos. O produtor externo continua responsável pela irradiância RGB linear, padding e correspondência com a geometria/luzes. A GPU Android ainda precisa de validação física com ADB autorizado.

## Referências concretas

- [Unity 6000.0 — Constant Force: propriedades e Inspector](https://docs.unity3d.com/6000.0/Documentation/Manual/class-ConstantForce.html): distinção entre vetores mundo/local e dependência Rigidbody.
- [UnityCsReference, branch 6000.0 — ConstantForce](https://github.com/Unity-Technologies/UnityCsReference/blob/6000.0/Modules/Physics/ScriptBindings/ConstantForce.bindings.cs): contrato público estudado; implementação Astra própria na ponte Jolt existente.
- [Jolt 5.3.0 — BodyInterface](https://jrouwe.github.io/JoltPhysicsDocs/5.3.0/class_body_interface.html): força acumulada e ativação. A ponte Astra libera BodyLockWrite antes de ActivateBody, preservando ownership e evitando lock recursivo.
- [Unity 6000.0 — Lightmap UVs](https://docs.unity3d.com/6000.0/Documentation/Manual/LightingGiUvs.html): UV1/TEXCOORD1 separado, charts e padding; adaptação para mapas externos por slot.

## Evidência host

Três cenários de Força constante passaram: integração F/m por um segundo com rotação local e torque real; receita → gizmo → Undo/Redo → salvar/reabrir → simular → inativar; recusa de movimento incompatível. Auditoria UV1 passou os casos de aresta compartilhada, winding invertido, área sobreposta, face duplicada, degeneração, NaN, fora de atlas e orçamento. O cenário integrado de lightmap externo, persistência e publicação de payload continuou passando. Capturas do editor usam UI executável host; não provam iluminação GPU ou toque físico Android.
