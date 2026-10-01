# Character: movimento preservado em rebuild físico

Problema reproduzido: ScenePhysics::rebuild guardava velocidades de Rigidbody, mas destruía/recriava CharacterMotor sem preservar velocidade, intenção do frame e salto aceito. Uma alteração de atrito em outro corpo podia reiniciar a queda ou apagar comandos de script. Isso fazia parte da cadeia de lifecycle do personagem, não de uma função nova de UI.

Referências concretas: [Unity6000.0 CharacterController.Move](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/CharacterController.Move.html) separa deslocamento solicitado e gravidade; [Godot4.5 CharacterBody3D](https://docs.godotengine.org/en/4.5/classes/class_characterbody3d.html) mantém velocidade como estado de movimento; [Jolt CharacterVirtual no commit vendorizado](https://github.com/jrouwe/JoltPhysics/blob/78d483dc3d375581203cf070ea2790e8045e0879/Jolt/Physics/Character/CharacterVirtual.h) exige RefreshContacts após mudanças de posição/mundo. A adaptação Astra preserva escalares e refaz contatos no mundo novo; nenhum contato/handle Jolt antigo é reutilizado.

## Política

Snapshot somente durante reconstrução: ID do objeto, identidade do componente, forma (radius/halfHeight/eyeHeight/slope), pose mundial, velocidade, salto pendente, acumulador do motor e intenção de movimento normal/script do frame. O mundo novo restaura primeiro movimento de corpos rígidos; depois restaura personagens compatíveis e consulta apoio contra os corpos novos.

Mesma instância, mesma forma e pose igual dentro de1e-4: movimento preservado. Troca/remoção de instância, alteração de forma/inclinação ou reposicionamento explícito: conserva a política de reset já existente. Não foi criado um estado universal serializável. Arrays/vetor são alocados no caminho frio de rebuild; o update normal não captura snapshots a cada frame.

O adaptador registra WorldId proprietário. Rebuild para outra sessão usa start limpo, mesmo que IDs, formas e poses coincidam; a proteção também impede transportar velocidades de corpos rígidos entre sessões.

Se o apoio desapareceu, RefreshContacts consulta a realidade nova; um salto pendente não ganha chão fictício. O próprio motor continua exigindo OnGround para consumi-lo. Falha de restauração é erro explícito, sem fallback que faça o movimento parecer conservado. Teardown mantém a ordem: motores emprestados são destruídos antes do mundo proprietário.

## Aceite

Queda após0.25s e rebuild por atrito de outro corpo: a segunda janela0.25s conserva velocidade adquirida. Movimento e salto aceitos antes de FixedUpdate sobrevivem a edição de corpo pela ABI e reconciliação no próprio callback. Reposicionamento por Inspector continua reinicializando movimento. [Evidências antes/depois](../validacao/evidencias/character-rebuild-20261001/README.md).

Sem novo schema, fachada, receita, propriedade autoral ou ícone. ABI30/Character3/cena16/prefab3;34/33/57/230. Não conclui P07: outras políticas e comandos de personagem continuam no atlas.
