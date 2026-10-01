# Conclusão de tween — 01/10/2026

[Contrato e referência Godot4.5](../../../planos/TWEEN-CONNECTION-2026-10-01.md). [Manifest do APK](manifest.json).

Host: conexões de tween3/3, controles2/2, espera/retarget/autoridade1/1, relógio/escala5/5, TimerConnection3/3, conexões de física3D4/4 e2D3/3, schemas/API gerada13/13 e atlas5/5. C#54/54 incluindo doze fixtures compiladas juntas. Não executada suíte inteira. A falha inicial de schema indicou SDK gerado desatualizado; regenerado pelo exporter real de aether_tests, o contrato passou13/13.

Teste de execução usa GameWorld/SceneTweens reais. Pose final e ativação precedem a observação; dois receptores Light de composições clonadas chegam à extração real de luzes. Clone remapeia referência interna; v1/v2 migram desconectadas. Cancelar/loop infinito não emitem; reiniciar rearma uma execução; receptor removido gera falha explícita e teardown limpa estado. UI testa receita inteira como um Undo, enum, picker, busca por IME, histórico e arquivo.

[Portrait](host-portrait.png) mostra ação e receptor juntos; [landscape](host-phone.png) conserva paginação em duas páginas. [Play concluído](host-play.png) mostra Conexão aplicada sobre resultado real. [Receptor removido](host-missing.png) executa destruição antes da conclusão e mostra Receptor indisponível / Objeto ausente. Capturas da UI executável pelo rasterizador host foram inspecionadas; não são imagens conceituais ou prova no telefone. Ícone novo integra SVG/PNG/atlas229. Referência no viewport é contextual à instância em edição.

O runtime do aparelho continua distinto da prova host. Fixture TweenConnectionProbe verifica pela CLR: referência, receptor inicialmente inativo, pose final, ativação e ausência de término ao cancelar; compilar a fixture não prova sua execução. Keyguard seguro impede observar TWEEN CONNECTION PASS e edição física. Instalação/SDK/atlas e envio de projeto são registrados separadamente no manifest.

34 schemas/33 fachadas/57 receitas/229 ícones; ABI29/cena16/prefab3/Timer4/TransformTween3. Não encerra P06 nem o atlas P00–P20.
