# Três jogos para explorar a capacidade atual da Astra

Proposta para avaliação do usuário, 28/09/2026. Nenhum dos três jogos foi produzido nesta etapa.

NÃO IREI SER SIMPLISTA NO DESIGN.

## Contrato da experiência

Produzir três projetos independentes, editáveis e jogáveis no Play da Astra, com temas, objetivos e identidades diferentes. A entrega proposta é uma missão completa de aproximadamente 15–25 minutos por jogo, com reinício, vitória, falha e variações que incentivem repetir. A duração é meta de design.

Não modificar engine, renderer, shaders da engine, bindings, componentes nativos, editor ou APK para acomodar os jogos. Escrever scripts C# pertencentes aos projetos e preparar cenas, modelos, materiais, texturas e configurações usando os caminhos existentes. Preparação externa de assets é permitida; inventar uma API ou integrar outro backend não é.

Usar a instalação existente, identificando antes da produção sua versão e sua correspondência com o código. A inspeção atual encontrou HEAD `8d9e2a88`, branch `codex/gameplay-runtime`, e alterações locais preexistentes. O código observado não prova que o APK instalado contém a mesma capacidade. Não incorporar o worktree `atchengine-render-rebuild` para ampliar o resultado.

Não reconstruir o APK apenas para embutir novos exemplos. Entregar pastas de projeto e carregá-las pelo mecanismo existente de projetos; verificar a forma de transferência antes de produzir o pacote final. Exportação de APK independente não integra esta proposta.

## Base técnica observada

Inspeção de código e documentação, sem execução nesta etapa:

| Capacidade | Evidência local | Consequência para os jogos |
|---|---|---|
| Projetos, cenas e scripts C# | `ProjectStore.java`, `docs/runtime-gameplay.md`, exemplos em `android/app/src/main/assets/astra/example-projects/` | Projetos próprios; os exemplos existentes servem para consultar contratos, não para anunciar três jogos novos |
| GLB com hierarquia, UV e materiais | `native/resources/gltf_import.cpp` | Modelos importados com peças separadas, pivôs e materiais preparados fora da engine |
| PBR: base color, normal, metallic/roughness, emissão e oclusão | Importador GLB e caminho de materiais em `native/platform/android/instanced_renderer.cpp` | Arte realista usando mapas efetivamente consumidos; revisar perdas de extensões do glTF |
| HDRI, reflexão GGX, LUT BRDF e irradiância SH9 global | `docs/ENVIRONMENT-MAP.md`, contrato de ambiente existente | Boa base para exteriores e materiais; SH global não substitui GI espacial em interiores |
| Corpos, colisores, personagem, juntas e motores | `native/scene/schemas/physics3d.h`, `native/scene/joint.h`, `native/runtime/scene_physics.cpp` | Portas, plataformas, cargas e contrapesos conectados ao solver Jolt |
| Queries físicas, forças, impulsos e torque por scripts | `managed/Astra.Scripting/Physics.cs`, `Runtime/NativeBehaviorRuntime.cs`, scripts existentes | Interação por visada, manipulação física e possível suspensão autoral |
| Instanciação de prefabs | `managed/Astra.Scripting/World.cs` | Reuso e variações; verificar quais estruturas físicas podem nascer em Play antes de depender disso |
| Lightmap/GI assada | `native/core/engine_capability.h`: `render.gi.lightmap` está `Planned` | Sem promessa de lightmap nativo ou de bake espacial integrado |

A presença dessas cadeias no código não equivale à aprovação dos jogos no aparelho. Áudio, HUD autorável, gravação de progresso, skinning de personagens, navegação automática e veículo especializado não foram auditados de ponta a ponta nesta proposta. Não são dependências essenciais dos conceitos. Caso se usem, precisam de verificação anterior à promessa.

## 1. USINA 17 — sobrevivência e manutenção industrial

**Experiência:** você entra numa instalação de tratamento desativada para recuperar uma amostra e restaurar a saída. A partida acontece num pátio, sala elétrica, oficina e galeria de serviço conectados. O lugar apresenta desgaste plausível: tinta descascada, concreto com infiltração, metal oxidado, equipamentos pesados e iluminação de emergência.

**Ciclo:** explorar → identificar a falha → transportar e instalar peças → reconfigurar energia → abrir caminhos → recuperar a amostra → sair antes de consumir os recursos disponíveis.

Mecânicas propostas:

- Inspeção por raycast e alcance; objetos atrás de paredes não podem ser usados.
- Pegar, carregar, soltar e arremessar objetos com força limitada pelo peso. Buscar uma resposta por mola e amortecimento usando as APIs existentes; evitar mover corpos através da geometria.
- Portas articuladas com limites, fechaduras e obstrução física. Uma caixa pode impedir uma porta de fechar.
- Fusíveis e baterias como itens físicos; posições de encaixe verificam proximidade e estado do item.
- Rede elétrica de gameplay nos scripts: orçamento de potência e prioridades. Alimentar a ventilação pode exigir desligar o elevador.
- Elevador de manutenção com junta deslizante, curso limitado e carga máxima como regra de jogo.
- Áreas perigosas com exposição acumulada, consumo de filtro e recuperação em zona segura. Trata-se de modelo de gameplay, não de simulação de gases.
- Lanterna com bateria e iluminação local, sujeita à confirmação do caminho atual de luz e sombra no APK.
- Recursos escassos, rotas alternativas e condição de extração. Variações de posição usam conjuntos previamente preparados quando a instanciação física não for suportada.

**Cena decisiva:** o jogador precisa mover um carrinho para liberar a porta, transportar uma bateria que exige as duas mãos, desligar um circuito e subir com uma carga no elevador. Uma escolha de energia altera o caminho disponível.

**Arte e importação:** gerador, extintor, tubulações, quadros elétricos, carrinho, ferramentas e arquitetura industrial modular. Separar portas, alavancas e gavetas antes da exportação. Usar colisores compostos simples nos objetos móveis e reservar malha detalhada para o visual.

**O que este jogo pressiona:** luzes locais, interiores, normal maps próximos à câmera, colisão em espaços apertados, scripts que coordenam estados e persistência da autoria.

**Risco real:** iluminação indireta global pode deixar interiores incoerentes. Aprovar uma sala de referência dentro da Astra antes de produzir toda a instalação. Não resolver isso adicionando GI à engine.

## 2. CARGA BRUTA — transporte e máquinas numa pedreira

**Experiência:** operar um pequeno veículo industrial e equipamentos de carga numa pedreira abandonada. O desafio é entregar três cargas por uma rota com rampas, curvas, piso irregular e uma plataforma de serviço. A massa transportada muda a condução.

**Ciclo:** escolher carga e rota → carregar → distribuir o peso → transportar → operar a plataforma → descarregar sem destruir a carga.

Mecânicas propostas:

- Direção, aceleração, frenagem e marcha à ré como scripts de projeto.
- Suspensão por pontos de contato consultados por raycast, com mola/amortecimento e torque resultante. Provar primeiro a estabilidade e o custo dessa composição com as APIs públicas existentes.
- Tração limitada pelo contato; perda de aderência como modelo simplificado de jogo. Sem anunciar modelo completo de pneus, WheelCollider ou simulação automotiva científica.
- Cargas com corpos independentes: caixas deslizam, tombam e caem. Evitar recalcular ficticiamente o centro de massa quando o solver já recebe o peso pelos contatos.
- Escolha da distribuição da carga e comprometimento entre velocidade e segurança.
- Guincho de curso fixo/plataforma com junta deslizante e motor; carga suspensa por vínculo de distância. Não prometer cabo contínuo deformável nem corda com colisão.
- Rampas móveis, barreiras articuladas e contrapesos.
- Fragilidade de carga estimada por eventos de colisão e velocidade. Só usar impulso de contato como medida se a API realmente o fornecer.
- Limite de combustível ou energia como estado de gameplay; penalidades por perda de carga e tempo.
- Três contratos e dois trajetos, com modo de stress que aumenta cargas físicas mantendo a mesma implementação.

**Cena decisiva:** conduzir vazio pela rampa, repetir com carga alta e observar a diferença; frear numa curva, corrigir a distribuição, entregar numa plataforma sem perder caixas. A dificuldade deve nascer do contato e do peso.

**Arte e importação:** veículo com rodas separadas e pivôs corretos, equipamento de içamento, paletes, blocos de pedra, chapas, barreiras e rochas de fotogrametria. Usar texturas cuja escala física combine com os modelos. O asset `quarry_wall` da Poly Haven é um candidato concreto para superfícies rochosas.

**O que este jogo pressiona:** forças, torque, juntas, estabilidade de pilhas, terreno irregular, sombras exteriores e visibilidade de muitas peças.

**Condição de viabilidade:** este é o conceito de maior risco. Antes da arte final, produzir apenas o protótipo de veículo como conteúdo de projeto. Se as APIs atuais não sustentarem a condução, registrar o bloqueio e apresentar uma versão centrada em operação de guindaste/plataformas para decisão do usuário. Não trocar o conceito silenciosamente nem mover um veículo por trilho fingindo física.

## 3. SEPULCRO DE CALCÁRIO — exploração arqueológica e mecanismos

**Experiência:** explorar uma necrópole construída numa encosta árida, com pátio iluminado, corredor estreito e câmara subterrânea. Abrir o sepulcro exige entender o peso das peças e a relação entre mecanismos; retirar o artefato altera o equilíbrio que mantinha a saída aberta.

**Ciclo:** observar marcas e estrutura → experimentar massas e posições → atravessar mecanismos → retirar o artefato → reconfigurar a saída.

Mecânicas propostas:

- Caminhar, saltar e transportar objetos; interação condicionada por distância e visibilidade.
- Balanças e plataformas de pressão: usar corpos detectados e suas massas, sem ativar apenas porque qualquer objeto tocou um trigger.
- Portões com contrapesos, plataformas deslizantes e vigas articuladas.
- Pêndulos com massa, colisão e limites, permitindo sincronizar a travessia.
- Peças arqueológicas com orientação e encaixe. Pontos de encaixe são regra de jogo explícita; travar a peça exige um caminho físico disponível.
- Ferramenta pesada capaz de deslocar blocos e liberar mecanismos.
- Desabamento localizado com peças pré-fraturadas produzidas fora da engine e corpos preparados na cena. Não é fratura procedural nem soft body.
- Rotas que combinam manipulação física, salto e observação espacial.
- Artefatos opcionais que acrescentam carga ou exigem consumir um contrapeso útil.
- Solução alternativa por distribuição de massa, em vez de uma única combinação de botões.

**Cena decisiva:** retirar uma estátua de uma balança, ver o portão perder sustentação e improvisar um peso equivalente com objetos coletados; atravessar enquanto um pêndulo continua sua simulação.

**Arte e importação:** rochas, colunas, relevos, vasos, ferramentas e portas pesadas; pedra com erosão em escala correta, arestas com geometria e detalhes pequenos em normal maps. Evitar ambientes inteiros importados como uma malha sem peças editáveis.

**O que este jogo pressiona:** contraste entre exterior/interior, materiais minerais, contato de peças irregulares, mecanismos encadeados e muitas partes móveis próximas à câmera.

**Risco real:** grandes razões de massa e correntes longas de juntas podem ficar instáveis. Projetar mecanismos curtos e mensuráveis, aumentar a carga depois e registrar o limite sem alterar o solver.

## Direção visual e interação

NÃO IREI SER SIMPLISTA NO DESIGN.

Usina: geometria apertada, luzes funcionais, aço pintado e concreto. Pedreira: visão mais aberta, céu e luz solar coerentes, rocha e poeira incorporada às texturas. Sepulcro: grandes massas de pedra, relevos próximos e recortes de luz arquitetônicos.

A proposta estrutural é explicar estados pelo próprio mundo: circuitos por lâmpadas existentes, peso pelo deslocamento da plataforma, risco da carga por sua posição e acesso por movimento dos portões. Isso depende de scripts, luzes e transforms já expostos, evitando exigir um novo sistema de HUD.

Controles previstos: movimento, olhar e poucas ações de contexto pelos bindings existentes. Confirmar alternância entre jogador e veículo e o comportamento do toque antes de fixar a UX. Não prometer inventário gráfico, arrastar entre painéis ou ícones de ações variáveis sem um consumidor atual.

Cada conceito começa com um cenário compacto. O objetivo é densidade de interação e detalhe visível; aumentar área sem capacidade de preenchê-la prejudicaria o realismo. A produção só aprova arte por captura do Play da Astra, não por render externo.

## Texturas, assets e iluminação

### Assets reais

Buscar modelos de qualidade por adequação visual e técnica, incluindo acervos comerciais aos quais houver acesso. Não limitar a pesquisa a CC0. Não efetuar compras nesta fase; registrar origem, disponibilidade e formato de cada candidato.

Poly Haven oferece candidatos imediatos para objetos, materiais e HDRIs. Para peças centrais específicas, avaliar bibliotecas comerciais ou assets fornecidos pelo usuário. Conferir o pacote baixável: um material preparado apenas para outra engine pode depender de shaders que a Astra não implementa.

Preparar fora da engine: escala em metros, pivôs, transforms, topologia, normais/tangentes, UV, colisor simples e partes articuladas. Converter para o perfil GLB aceito e revisar o relatório de importação. Não aceitar silenciosamente perdas de materiais, transparência ou extensões.

Texturas de trabalho em 4K; 8K para fontes ou peças centrais que mostrem ganho real no enquadramento. Variantes de 1K/2K/4K para medir memória e banda. Isso é um ponto inicial de produção, não limite certificado do dispositivo.

Base color sem iluminação fotografada forte; normal a partir de detalhe geométrico; roughness com variação por material e uso; metallic coerente com metal exposto versus tinta; AO localizado. Separar espaços de cor e verificar normal Y e empacotamento dos canais no resultado importado.

Usar displacement na preparação offline de geometria/normal. Não assumir tessellation ou displacement em runtime. Mipmaps, filtragem e compressão apenas nos caminhos efetivamente aceitos pelo importador/backend.

### O limite dos lightmaps

A Astra marca `render.gi.lightmap` como planejado. Ler UV secundária no GLB não cria lightmap: faltam o contrato de iluminação assada, associação por instância e consumidor correspondente. O plano não entrega GI nativa por simplesmente importar uma textura com esse nome.

Estratégia garantida pela proposta de conteúdo: PBR, HDRI/IBL existente, luzes reais disponíveis, geometria preparada e bake de detalhes/oclusão compatíveis. O bake de AO não deve ser apresentado como bounce lighting.

Estratégia experimental: avaliar bake externo de contribuição difusa estática no Blender, reempacotado em material compatível. Só adotar após uma sala provar exposição, interação com luz direta e ausência de iluminação duplicada. Emissão não ilumina automaticamente objetos vizinhos e não responde como lightmap ao movimento de luzes/oclusores. Se a aproximação causar superfícies luminosas ou resposta incorreta, descartá-la e registrar a limitação.

Técnicas offline propostas: UV sem sobreposição para os bakes únicos, densidade controlada por superfície, margens/dilatação que sobrevivam aos mips, separação de direto/indireto/AO, comparação antes/depois do denoise e exclusão de portas, cargas e demais móveis da iluminação estática do ambiente. Parâmetros de amostragem dependem do ruído observado, não de escolher um número alto por aparência.

Não assar luz direta no albedo e depois aplicar a mesma luz dinâmica. Preservar a fonte PBR original para comparação e reversão. Se o critério obrigatório for lightmap integrado equivalente ao da Godot/Unity, ele é incompatível com a restrição atual de não alterar a engine.

## Física e stress mensuráveis

Avançar por cenários: objeto isolado → mecanismo completo → missão → carga crescente. Proposta inicial de degraus: 16, 32, 64 e 128 corpos ativos na área de teste; são cargas experimentais, não capacidade afirmada. Parar antes de comprometer a sessão ou perder dados.

Medir quando houver ferramentas existentes: tempo de frame e física, memória, corpos ativos, estabilidade após repouso, juntas sob carga e aquecimento numa sessão prolongada. Usar a mesma rota/câmera e configurações registradas. Separar custo visual do custo físico alterando uma variável por rodada.

Escolher o perfil de maior qualidade que sustente o objetivo acordado no aparelho; propor 30 FPS estáveis para qualidade e comparar 60 FPS onde viável. Não prometer esses resultados antes das medições. Guardar um perfil extremo separado do perfil jogável.

Não anunciar simulação de fluidos, lama deformável, pneus avançados, cordas contínuas, destruição procedural ou anatomia física só porque seria possível escrever uma aproximação visual.

## Ordem de produção depois da aprovação

1. Registrar baseline da instalação e código, conferir transferência de projetos e montar uma amostra GLB/PBR com um mecanismo físico. Nenhuma mudança na engine.
2. Provar os riscos: sala industrial iluminada, suspensão do veículo por script e balança/contrapeso. Retornar decisões apenas se algum conceito ficar inviável.
3. Construir a missão completa da Usina com arte final numa sala representativa antes de expandir.
4. Construir Carga Bruta após aceitar o protótipo físico; em seguida Sepulcro, reaproveitando somente scripts e assets comuns dentro dos projetos.
5. Refinar materiais, importar peças centrais e registrar comparações na Astra.
6. Fazer validação focada e rodadas de stress, quando autorizada a execução. Não rodar suíte geral da engine para validar conteúdo.

## Aceite e entrega

Cada jogo deve abrir como projeto independente, permitir editar uma peça/luz/script, salvar, reabrir e preservar a alteração. Deve iniciar no Play, permitir completar a missão, falhar e reiniciar. Parar o Play deve preservar os dados de autoria conforme o contrato da Astra.

Persistência de projeto é requisito. Savegame de progresso entre sessões é uma capacidade separada: não afirmar que existe por salvar a cena. Definir checkpoints persistentes apenas se houver caminho disponível sem ampliar a engine.

Entregar pasta do projeto, fontes C#, assets preparados, registro de origem, instruções de controle, configuração visual e relatório curto de limites. Capturas e vídeo devem mostrar gameplay real, incluindo manipulação, efeito da física e edição seguida de reabertura.

Manter três estados explícitos na entrega: conteúdo implementado, funcionamento observado no aparelho e recurso bloqueado pela engine. Se algo só compilar ou aparecer na cena, não contar como mecânica concluída.

Nesta etapa: apenas plano e inspeção de fontes. Sem download de pacotes, compras, geração de assets, execução de testes/builds, instalação ou alteração da engine.

## Referências consultadas e aplicação

- [Unity 6.0 — Hinge Joint](https://docs.unity3d.com/6000.0/Documentation/Manual/class-HingeJoint.html): referência de eixos, limites e motores para portas/mecanismos; adaptar aos quatro tipos de junta que a Astra expõe.
- [Unity 6.0 — Wheel collider suspension](https://docs.unity3d.com/6000.0/Documentation/Manual/wheel-colliders-suspension.html): referência de mola/amortecimento; não comprova WheelCollider na Astra. O veículo proposto depende de um protótipo autoral.
- [Godot 4.5 — LightmapGI](https://docs.godotengine.org/en/4.5/tutorials/3d/global_illumination/using_lightmap_gi.html): distinguir luz assada, UV exclusiva, objetos móveis e reflexos; usar essa separação para não confundir texturas assadas com GI integrada.
- [Blender 4.5 LTS — Render Baking](https://docs.blender.org/UATEST/manual/en/4.5/render/cycles/baking.html): bake de normal/AO, passes e margens; usar no preparo offline, sem transferir capacidades do Blender para a Astra.
- [Poly Haven — modelos](https://polyhaven.com/models) e [Quarry Wall](https://polyhaven.com/a/quarry_wall): candidatos de conteúdo real; a textura de pedreira documenta mapas e escala física. A escolha final depende da importação e da inspeção no Play.

As referências são de contratos e preparação de conteúdo. Nesta fase não houve análise de vídeo de workflow nem validação visual da UI; não se propõe alterar o editor.
