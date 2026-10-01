# Plataformas móveis e impulso horizontal do personagem

## Caminho qualificado no runtime existente

Corpo cinemático e Collider autorados → comando MoveKinematic em FixedUpdate → CharacterVirtual consulta o piso antes do passo do mundo → motor usa velocidade no ponto de apoio → ScenePhysics publica pose → ReadCharacterState consulta apoio e deslocamento. Não há componente de plataforma fictício: o suporte é um corpo existente com geometria e movimento efetivos.

Três cenários passaram usando Jolt: transporte horizontal 1,983 m e vertical 2,000 m em uma trajetória de 2 m; rotação de 1 radiano carregou cápsula fora do centro até (1,095; -1,674) no plano X/Z, com velocidade tangencial (-0,839; -0,544); o Play com FixedUpdate, ABI, sincronização de pose e snapshot também passou. SDK 54/54 com 17 fixtures. Projeto independente CharacterPlatform-20261001 preparado. Esses resultados não qualificam todas as geometrias, acelerações, escalas ou interações de plataformas.

## Character v4 — opção autoral conectada

`inherit_platform_horizontal`, exibida em Chão como **Impulso ao sair**, conserva X/Z da velocidade do apoio enquanto a cápsula está no ar. A intenção de movimento do jogador continua somada a essa parcela. Desligar em Play limpa somente essa parcela no próximo passo; não reconstrói a cápsula. Retornar ao apoio substitui o impulso pelo suporte atual. A opção não acrescenta política nova para a velocidade vertical, que continua no integrador existente.

Projetos v1–v3 carregam desligados. Valor autoral salva em Character v4; velocidade herdada é estado runtime, não é serializada. Rebuild compatível transporta esse escalar junto de velocidade/salto; mudança de pose/forma ou outra sessão reinicia o motor. Nenhum campo sem consumidor: ScenePhysics lê a opção a cada passo. Inspector usa booleano/histórico comuns; SDK foi regenerado do descriptor. Ícone vetorial character-platform-carry integra o atlas e o marcador/diagnóstico do personagem com opção ativa.

## Referências

[Godot 4.5 CharacterBody3D](https://docs.godotengine.org/en/4.5/classes/class_characterbody3d.html) distingue velocidade da plataforma, velocidade real e política de saída. Extraímos a necessidade de uma escolha explícita ao perder apoio; esta entrega conserva apenas impulso horizontal, com padrão compatível com arquivos Astra anteriores. Não declara paridade com os três modos completos da Godot.

[Jolt CharacterVirtual, commit 78d483dc](https://github.com/jrouwe/JoltPhysics/blob/78d483dc3d375581203cf070ea2790e8045e0879/Jolt/Physics/Character/CharacterVirtual.h) fornece velocidade do ponto de apoio e ground state. [Unity 6000.0 Rigidbody.MovePosition](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Rigidbody.MovePosition.html) é referência de movimentação cinemática pelo passo fixo; aqui o comando chega ao Jolt e a pose publicada tem autoridade física.

## Aceite em curso

Saltar de plataforma móvel com opção ligada/desligada → medir diferença horizontal no ar → reconstruir sem perder impulso → desligar em Play sem deslocamento horizontal residual → salvar/reabrir/Undo/Redo → rejeitar valor de arquivo inválido → verificar Inspector e SDK. Resultados do novo campo serão registrados após execução; os três testes acima qualificam o transporte existente, não o campo v4 ainda em validação.

## Resultado do impulso horizontal

Host 5/5 cenários de plataformas: salto sem opção deslocou 0,017 m e com opção 0,500 m no intervalo medido; reconstrução compatível conservou a parcela, desligamento ao vivo removeu seu efeito. Migração v1–v3, arquivo v4, valor inválido, Undo/Redo e propriedade runtime sem invalidação de corpo passaram. Regressões de estado 2/2, reconstrução 4/4, chão 4/4, schemas 13/13 e atlas 5/5. SDK 54/54 com 18 fixtures compiladas, incluindo a propriedade tipada InheritPlatformHorizontal.

Capturas do Inspector executável em host foram inspecionadas: opção legível em retrato, estado do apoio e ícone do impulso sem diagnóstico duplicado. Não são captura Vulkan nem aceite de aparelho. Projeto independente CharacterPlatformCarry-20261001 criado. Contagens: 34 schemas, 33 fachadas, 57 receitas, 231 ícones; Character4/ABI31. Build Android e sua conferência de artefatos estão em andamento; ADB continua sem dispositivo.
