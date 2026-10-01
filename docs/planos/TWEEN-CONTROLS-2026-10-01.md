# Controle de TransformTween — ABI29

Referências: [Godot4.5 Tween API](https://docs.godotengine.org/en/4.5/classes/class_tween.html), [source oficial4.5](https://raw.githubusercontent.com/godotengine/godot/4.5/scene/animation/tween.cpp) e [workflow de animação4.5](https://docs.godotengine.org/en/4.5/tutorials/animation/introduction.html). A referência separa pausa, continuidade e cancelamento; alerta sobre escritores concorrentes. A Astra usa seu componente autorável e avaliador existente, sem copiar o modelo efêmero da Godot. Reiniciar o componente é suportado na Astra. Não há paridade com sequências, subtweens ou propriedades arbitrárias da referência.

## Dependências e execução

TransformTween2 autorado → SceneGraph/arquivo → GameWorld → SceneTweens → transformação local → extração runtime. C# → SceneAdapter → ABI29 → ScriptBridge → o mesmo SceneTweens. Serviço existe antes do Start dos scripts e é desligado após Stop. Objeto + instância identificam o componente; a Astra permite um TransformTween por objeto. Não foi alterada essa multiplicidade.

Comandos0–4: snapshot, reiniciar, cancelar, pausar, retomar. Reiniciar captura a pose local corrente na próxima avaliação, reseta atraso/elapsed e preserva pausa local. Cancelar interrompe escrita e preserva pose; retomar não revive cancelado/concluído. Pausa conserva elapsed. Snapshot24 bytes contém status, elapsed, pausa, enabled e activeInHierarchy. Não é progresso derivado fictício. Layout, reserved, operação e lifetime são validados no nativo; C# verifica sessão/thread/identidade.

O avaliador mantém canais, atraso, easing, destino relativo, pingpong e ciclos já existentes. Física, animação, constraints e PathFollow conservam precedência conforme a política existente: o tween não toma autoridade física nem oculta outro escritor. Estado runtime nunca é serializado; propriedades continuam no componente2. Nenhum novo schema, fachada, receita ou ícone:34/33/56/228.

## Interface

NÃO IREI SER SIMPLISTA NO DESIGN.

As ações permanecem próximas à instância expandida, na categoria Tempo. Pausar vira retomar conforme o estado real. O alvo de toque codifica índice de componente e ação; o roteamento conserva o proprietário do Inspector tocado e evita o espelho de edição autoral. Diagnóstico da faixa consulta a instância exibida. Isso é necessário para componentes precedidos por outros e para janelas focadas; `expandedNative` do Inspector principal não é fonte segura para essas ações.

Usa conceitos e ícones runtime existentes. Captura executável e resultados dos cenários devem constar em [evidências](../validacao/evidencias/tween-controls-abi29-20261001/README.md). Não substituem interação física Android.

## Aceite e limites

Manual → pausar → reiniciar → pose/elapsed imóveis → retomar → posição mudar → cancelar → posição preservada → reiniciar → concluir no destino. Comandos em Start e depois de Stop, componente removido e mundo diferente são verificados separadamente. A fixture TweenControlProbe verifica a cadeia CLR/ABI/pose quando executada no aparelho.

O pacote permanece restrito a transformação local. Tween genérico por PropertyId, sequência/paralelo, callback de término autorável e qualificação física continuam em P06. Não encerra P06/P00–P20.
