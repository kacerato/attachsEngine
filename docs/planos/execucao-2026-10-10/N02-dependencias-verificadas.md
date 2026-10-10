# N02 — dependências verificadas antes dos canais gerais

Estado: investigação de arquitetura; nenhuma capacidade N02 é declarada entregue
por este documento. N01 e N07 continuam em implementação paralela.

## Fontes de decisão

- [Godot 4.5 Animation](https://docs.godotengine.org/en/4.5/classes/class_animation.html):
  atualização contínua, discreta e captura têm contratos distintos.
- [Godot 4.5 AnimationMixer](https://github.com/godotengine/godot/blob/4.5-stable/scene/animation/animation_mixer.cpp):
  resolver alvos, acumular valores e aplicar a composição são fases diferentes.
- [Unity 6000.0 EditorCurveBinding](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/EditorCurveBinding.html):
  endereço de propriedade distingue caminho, tipo e propriedade, inclusive
  referências discretas. Isso não autoriza converter IDs de 64 bits em floats.

## Sistemas existentes que devem ser reutilizados

`scene/components.h` contém identidade local da instância, versão do tipo,
descritores, mutabilidade e invalidações. `ComponentNumber::tweenable` é opt-in;
`runtime/game_world.h::numberTweenable` já verifica setters, visibilidade,
capacidade e invalidações incompatíveis com edição frequente. ComponentTriple
agrupa canais numéricos; não possui outro setter. Bool/enums/referências ainda
precisam de uma declaração explícita de elegibilidade animável.

`SceneGraph::cloneSubtree` e Components preservam IDs locais das instâncias ao
clonar objetos e remapeiam referências. Um binding relativo ao dono pode usar
essa identidade em outra instância compatível, desde que valide tipo/versão e
ambiguidade. Não é preciso inventar IDs globais para cada campo de um prefab.

FOV da Camera, intensidade/cor de Light e tweens numéricos já possuem consumidores
reais. Os onze parâmetros de material por slot em `scene/mesh_renderer.h` usam
setters que também ativam `material.enabled`. Restauração deve conservar o valor
e a autoridade desse override sem sobrescrever o restante do componente.

## Lacunas estruturais

O compositor `runtime/scene_animation` usa chave `(object << 2) | AnimationPath`
para TRS/morph. Acrescentar um enum Property=4 causaria colisão de identidade.
Os canais gerais precisam de chave tipada própria ou trilhas paralelas, mantendo
o caminho rápido TRS/morph e compartilhando timeline, serialização e journal.

`GuiNode` tem opacidade em poses de movimento/transição, mas não uma opacidade
autoral base independente. Alterar `motion.from.opacity` com motion desativado
seria uma propriedade sem efeito. UI opacity exige ampliar o modelo e a composição
real de `GuiRuntime`, endereçando canvas/componente e GuiNodeId. SceneGui possui
cópias runtime do documento; não se deve confundir recurso autoral com essa cópia.

Clipes compilados são imutáveis; referências de propriedade não podem guardar
ponteiros para componentes removidos. Lifecycle precisa resolver por identidade,
diagnosticar versão incompatível e liberar/restaurar apenas propriedades possuídas.
Falha de um binding deve ser prevalidada antes de publicar um lote parcial.

## Autoridade e fase

Em `editor/editor_play_scene.h`, GUI avança antes de Update; animação ocorre antes
da física; LateUpdate ocorre depois; tweens numéricos ocorrem depois de LateUpdate
e antes de constraints/câmeras. Portanto não existe uma ordem implícita na qual
animação sempre ganhe de scripts/tweens. N02 precisa declarar posse, precedência,
restauração e inspeção de conflitos, preservando os contratos existentes.

`SceneAnimatorGraphs::parameter` já oferece parâmetros reais por dono/instância.
Drivers devem reutilizá-los e compilar um DAG limitado, detectando ciclos e
múltiplos escritores antes de executar. Remap/curva/clamp/combine não justificam
Any/Map genérico nem alocação por nó em cada frame.

## Caminho vertical necessário

Catálogo tipado elegível → binding versionado → curvas contínuas ou chaves
discretas exatas → compilação → posse/composição → consumidor → restauração →
editor/SDK → persistência e remap → cenário real. As transformações temporais,
clipboard e bake precisam acompanhar os novos tipos; mostrar linhas que essas
operações ignoram não fecha a implementação.

Aceite do bloco: Light intensity, material color, Camera FOV e UI opacity,
driver de parâmetro de velocidade, salvar/reabrir e duplicar/remapear instância,
alvo removido e ciclo recusados sem aplicação parcial. Pacote e dispositivo
seguem gates separados; ADB somente quando liberado no fim do plano inteiro.

## Revisão arquitetural após regiões AECLIP 5

Fontes oficiais consultadas novamente em 10/10:
[tutorial de autoria Godot 4.5](https://docs.godotengine.org/en/4.5/tutorials/animation/introduction.html)
e o mixer 4.5-stable citado acima. A Godot separa caches de Transform e Value,
retém identidade do objeto e subcaminho, acumula os valores e só depois aplica.
O editor cria uma trilha pela propriedade selecionada, sem exigir que o usuário
digite um endereço interno. Astra deve preservar esse princípio no catálogo
tipado e no seletor contextual, mantendo sua identidade visual.

O caminho rápido TRS/morph não será convertido em um mapa genérico. Canais
contínuos reaproveitam AnimationCurve; valores discretos precisam chaves exatas
para bool/enum/ObjectId/AssetGuid. IDs de 64/128 bits não podem passar por float.
O endereço precisa de binding de objeto relativo ao dono, instância local de
componente, ID/versão do tipo e ID da propriedade. GuiNodeId e identidade de
elementos de coleção devem ser preservados quando usados.

O catálogo deverá partir de ComponentSchema/ComponentType e das propriedades
que possuem setter e consumidor por frame. ComponentNumber::tweenable já fornece
essa prova para números; bool, enum e referências precisam opt-in equivalente,
sem abrir automaticamente propriedades que reconstruam física ou recursos.
Cor é um grupo linear de canais existentes, não um segundo setter concorrente.

Há dois cuidados de ownership: Material setters ativam override do slot; ao
restaurar, é preciso recuperar sua autoridade sem restaurar o componente inteiro
e apagar alterações de outro sistema. SceneGui mantém cópias runtime por lease;
animar um documento compartilhado alteraria outras instâncias. O consumidor de
UI precisa resolver o lease pela identidade do Canvas e escrever somente sua
cópia runtime. Preview usa cópia isolada, como o preview de pose atual.

Drivers usarão um DAG tipado com orçamento de nós/arestas, ordem determinística,
diagnóstico de ciclo e múltiplos escritores. A entrada de parâmetro vem do
SceneAnimatorGraphs real. A aplicação deve explicitar a ordem em relação a
Update, física, LateUpdate e tweens; ela não será inferida da ordem da UI.

Esta seção registra decisões e dependências, não anuncia implementação N02.

`resources/animation_driver.{h,cpp}` é um candidato isolado para a matemática
do DAG, fora dos alvos de build e dos menus. Compila IDs em ordem determinística,
recusa ciclos/fontes inválidas, limita 512 nós/256 fontes/128 saídas/8.192 chaves,
e avalia Source/Constant/Add/Multiply/ScaleBias/Clamp/Remap/Curve sem alocar no
sucesso. Falha preserva as saídas. Sintaxe estrita foi conferida. Não constitui
driver utilizável até conectar bindings, consumidor, persistência, SDK e editor.
