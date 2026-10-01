# Conexões de física 3D — 01/10/2026

[Contrato, referências e limites](../../../planos/PHYSICS-CONNECTIONS-2026-10-01.md).

A conexão tem produtor real Jolt, ação nativa e efeito na extração de luz, antes de TriggerEnter C#. O fluxo de criação, enum, receptor pesquisado por IME, Undo/Redo, arquivo, clone, filtro e receptor pendente são verificados no executável host. A recusa de configuração incompatível é registrada na rodada final de testes.

[Captura landscape](host-phone.png), [portrait](host-portrait.png) e [janela focada portrait](host-focused.png) vieram da UI executável pelo rasterizador host. Foram inspecionadas, não são conceitos gerados nem captura do aparelho. A janela focada estreita não foi qualificada como redesenho mobile. A conexão é a edição inicial da receita, com filtros em grupo separado e linha no viewport somente durante a edição. O novo ícone SVG integra o atlas real.

[Logs de host](host-tests.txt), [contratos](contracts.txt), [regressões](regressions.txt) e [C#](managed-tests.txt) separam os resultados. Filtros com zero testes selecionados não contam como validação. C#54/54, incluindo oito fixtures compiláveis juntas. A execução do callback no teste host usa a ponte falsa de script existente para observar a ordem; o evento físico e a ação/extração de luz são reais. A fixture C# independente deve provar a mesma ordem na CLR do aparelho.

[Manifest publicado](manifest.json): build final Android passou, assets foram comparados byte a byte, APK instalado e fixture enviada. Host final4/4; contratos, referência, geração, runtime e atlas passaram; Timer3/3 e prefab16/16. Android físico e PHYSICS CONNECTION PASS continuam pendentes enquanto a tela segura cobre o editor. Este pacote não conclui o universo P00–P20.
