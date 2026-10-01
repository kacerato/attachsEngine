# Controles de Timer — ABI28 / Timer4

Referência: [Godot 4.5 Timer](https://docs.godotengine.org/en/4.5/classes/class_timer.html) e [implementação oficial 4.5](https://raw.githubusercontent.com/godotengine/godot/4.5/scene/main/timer.cpp). Princípios extraídos: início independente do autostart, pausa por instância, restante observado e parada sem timeout. A Astra preserva `autoStart=true` para compatibilidade com cenas Timer1–3; o default da Godot é diferente. Não implica paridade completa com Timer.

## Caminho real

Inspector / GameTimer C# → ABI28 → ScriptBridge → SceneTimers da sessão → GameWorld → timeout nativo / TimerElapsed. O Inspector usa o mesmo scheduler. Comandos não passam pelo espelho de edição autoral. A identidade inclui objeto e instância; o serviço recusa componente removido, destruição pendente e sessão encerrada.

Timer4 acrescenta `auto_start` persistente; versões1–3 migram com início automático. Estado de execução, pausa e restante não são serializados. `Start(seconds=0)` usa o intervalo configurado; intervalo positivo entre0,05 e3600s altera somente a instância runtime. Reiniciar preserva pausa local; Stop zera restante e não emite timeout. Pause/Resume preservam restante. A pausa editorial interrompe também o relógio não escalado. `Running` descreve timer armado, não garante avanço enquanto objeto/componente/relógio estiver impedido.

A ABI retorna estrutura16 bytes com flags e restante double. Callback obrigatório e versão28 estrita impedem SDK antigo de executar silenciosamente. C# expõe State/Start/Stop/Pause/Resume em GameTimer; snapshots e operações respeitam thread e sessão da SceneAdapter.

## Editor e aceite

NÃO IREI SER SIMPLISTA NO DESIGN.

Na Inspeção durante Play, expandir a instância oferece restante, progresso e três ações compactas: iniciar/reiniciar, pausar/retomar e parar. Usa ícones existentes do atlas; não introduz conceitos ou ícones novos. Fora de Play esses controles não aparecem. Checkbox de autostart participa de Undo/Redo e arquivo; comandos de execução preservam autoria. A antiga menção a uma workspace Agenda de timers foi corrigida: ela não existe no código.

Aceite host: desligar autostart → Undo/Redo → salvar/reabrir → Play parado → iniciar → pausar/retomar → parar → sair de Play sem mudar autoria. Scheduler: pausa preserva restante; one-shot conclui antes do callback; Stop não dispara; comando em Start de script funciona; callback retido após Stop é recusado. Quatro testes direcionados passaram, além das regressões registradas em [evidências](../validacao/evidencias/timer-controls-abi28-20261001/README.md).

## Limites

Capturas são do rasterizador host executável, não do telefone. APK instalado e fixture enviada; aparelho bloqueado por keyguard seguro. TIMER CONTROL PASS na CLR Android e interação física ainda não observados. Não encerra P06 nem o atlas P00–P20. Conexões gerais, UI de gameplay e qualificação dos dispositivos permanecem no plano.
