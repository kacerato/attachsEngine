# TransformTween / ABI29 — 01/10/2026

[Contrato e referências](../../../planos/TWEEN-CONTROLS-2026-10-01.md). [Manifest instalado](manifest.json).

Filtro nativo `tween`:10/10; contém cinco testes diretamente ligados a tween, incluindo três cenários novos de controles/lifetime, e cinco seleções incidentais porque o filtro procura substring também em `between`. Não são dez capacidades de tween. Os testes protegem transformação real, espera, destino relativo, pingpong, retarget, cancelamento, autoridade física, pausa/retomada pela ABI29, Start de script, after-Stop, remoção de componente, mundo distinto e Inspector principal/focado.

Regressões: TimerControls4/4, TimerConnection3/3, PlayTimer2/2, PlayTimeScale5/5, ABI2/2, schemas13/13, atlas5/5, conexões3D4/4 e2D3/3, áudio/caminhos/perfis/captura pela ABI1/1 cada. C#54/54 incluindo onze fixtures compiladas juntas e layout24 bytes. Não foi executada suíte completa. Após ajustar apenas a faixa visual, o filtro tween10/10 passou novamente.

`tween-initial-tests.txt` ficou vazio por crash do teste inicial: ele tentou criar dois componentes em um objeto sem respeitar `allowMultiple=false`. Corrigido para dois objetos reais, sem alterar multiplicidade da engine. Logs finais são `*-tests.txt`; logs `*-initial-tests.txt` preservam investigação anterior. A implementação permite um TransformTween por objeto.

[Antes](host-portrait-before.png) mostra diagnóstico truncado pelas três ações. [Conceito gerado](design-concept.png) propôs separar estado e ações; é hipótese visual, não implementação. [Portrait posterior](host-portrait.png), [landscape](host-phone.png) e [pausado](host-paused.png) são capturas da UI executável, com estado/elapsed calculados pelo avaliador real. Foram inspecionadas: estado agora cabe, decorrido e ações ficam juntos, propriedades continuam paginadas na paisagem; não acrescenta painel permanente nem controles decorativos. Ícones existentes de runtime foram reutilizados. Layout geral estreito do editor continua uma limitação distinta deste pacote.

APK final compilado, SDK e atlas conferidos byte a byte, instalação e fixture registrados no manifest. Keyguard seguro impede interação/observação da CLR no telefone; TWEEN CONTROL PASS e qualificação visual física não foram observados. C# compilável e pose host não equivalem a prova Android.

ABI29, cena16, prefab3, Timer4, TransformTween2;34 schemas/33 fachadas/56 receitas/228 ícones. Não encerra P06 nem P00–P20.
