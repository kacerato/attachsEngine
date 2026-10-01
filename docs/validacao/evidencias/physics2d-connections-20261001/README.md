# Conexões de física 2D — 01/10/2026

[Contrato e referências Godot4.5](../../../planos/PHYSICS2D-CONNECTIONS-2026-10-01.md). [Manifest](manifest.json).

Host final: conexões2D3/3; conexões3D4/4; física2D11/11; conexõesTimer3/3. Contrato de schemas, referência versionada, fachada gerada e atlas passaram. C#54/54, incluindo nove fixtures compiladas juntas. Logs neste diretório registram cada seleção; não houve execução da suíte completa.

O sensor e o corpo executam no Box2D3.1.1 real. A ação altera GameWorld e extração de luz antes do callback pela ponte de teste de scripts; essa ponte falsa verifica somente a ordem do callback, sem substituir física ou ação. A fixture C# enviada deve provar a mesma ordem na CLR do aparelho. Destruir o visitante produz saída para a conexão nativa do sensor sobrevivente; a ponte C# evita receber outro objeto morto. Stop restaura a autoria. Também passaram IME/picker/enum, Undo/Redo, persistência, remapeamento de receptor interno e recusa de configuração incompatível inicial e durante Play.

[Landscape](host-phone.png) e [portrait](host-portrait.png) são capturas da UI executável pelo rasterizador host, inspecionadas. Portrait deixa evento/ação/receptor juntos; landscape pagina propriedades. Checkbox no cabeçalho controla enabled, evitando repetir o campo. Linha do receptor é contextual. O ícone próprio integra o atlas228. Não são imagens conceituais nem prova visual no dispositivo.

A receita cria exatamente um sensor, Body2D e conexão. Criar o sensor antes do corpo impede a composição automática de acrescentar uma forma sólida extra. A ferramenta de preview passou a usar bigobj somente no MinGW após atingir o limite COFF; não altera o APK ou o código executado.

Build Android final passou; SDK/atlas comparados byte a byte, instalação e envio da fixture registrados no manifest. Aparelho permanece com keyguard seguro; PHYSICS2D CONNECTION PASS e edição física no dispositivo não foram observados. Implementação host não é qualificação Android. Contagem real:34 schemas/33 fachadas/56 receitas/228 ícones, ABI27, cena16, Timer3 e prefab3. Não conclui P06 nem o atlas P00–P20.
