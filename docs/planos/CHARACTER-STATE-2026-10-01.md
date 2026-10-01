# Estado real do personagem — ABI 31

## Contrato implementado

`GameObject.ReadCharacterState()` consulta o CharacterVirtual vivo da sessão. `GroundState` distingue chão andável, contato íngreme, contato sem apoio e ar. `IsGrounded` é verdadeiro somente no primeiro estado. `Position` é a posição mundial dos pés da cápsula. `GroundNormal` e `GroundVelocity` são os dados do último contato do Jolt; só devem orientar gameplay quando o estado de contato correspondente for válido.

`Velocity` é o deslocamento mundial resolvido durante o último passo fixo dividido pela duração desse passo, incluindo correção de colisão, degrau e snap. `MotorVelocity` é a velocidade interna solicitada/corrigida pelo CharacterVirtual e não promete igual deslocamento após colisões. `HasMeasuredStep` é falso antes do primeiro passo aceito. Pausa ou consulta sem novo passo conserva a última amostra, não cria uma velocidade zero artificial. Rebuild compatível conserva a amostra; outra sessão, pose ou cápsula reinicia o histórico.

A ABI usa um snapshot sequencial de 80 bytes, com reservas zeradas e callback obrigatório na versão 31. Serviço existe antes de Start. Consulta após Stop recusa com NotRunning; objeto inativo ou sem binding vivo recusa explicitamente. Snapshot é runtime e não entra no formato de cena. Nenhuma propriedade de authoring ou tipo de componente novo foi adicionado.

## Cadeia real

Jolt CharacterVirtual → CharacterMotor (medição por passo) → ScenePhysics (identidade e atividade) → ScriptBridge → SDK C# e Inspector de Play. Sem alocação por medição ou consulta; reconstrução mantém somente escalares e refaz contatos. Inspector mostra estado de apoio e velocidade medida do objeto selecionado, sem transportar estado de UI para física. Ícone de chão existente é reutilizado.

## Referências e decisão

[Unity 6000.0 CharacterController.velocity](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/CharacterController-velocity.html) trata velocidade como deslocamento relativo à movimentação do controller. [Unity 6000.0 isGrounded](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/CharacterController-isGrounded.html) relaciona apoio à última movimentação. [Godot 4.5 CharacterBody3D](https://docs.godotengine.org/en/4.5/classes/class_characterbody3d.html) distingue velocidade solicitada, velocidade real e velocidade da plataforma. Extraímos a separação entre intenção e movimento resolvido, adaptada ao passo fixo nativo da Astra.

Fonte de backend: [Jolt CharacterVirtual no commit vendorizado 78d483dc](https://github.com/jrouwe/JoltPhysics/blob/78d483dc3d375581203cf070ea2790e8045e0879/Jolt/Physics/Character/CharacterVirtual.h). A velocidade interna não substitui a medição de deslocamento. Não há promessa de contato com ObjectId, lista de colisões do personagem, crouch, teleporte ou movimento 2D neste pacote.

## Validação em execução

Cenários: parede bloqueia deslocamento enquanto motor solicita movimento; consulta inicial sem amostra; queda/normal do piso; salto medido; consulta antes de Start; rejeição de layout/reserva; objeto inativo e callback retido após Stop. Fixture CharacterStateProbe compila com o SDK e exige apoio, salto e aterrissagem reais. Resultados finais, capturas host e manifesto do APK serão registrados em `docs/validacao/evidencias/character-state-abi31-20261001/`. Host: novos cenários 2/2, reconstrução 4/4, chão 4/4, schemas 13/13, atlas 5/5 e SDK 54/54 com 16 fixtures. Capturas finais inspecionadas. APK/SDK/atlas conferidos; instalação não ocorreu porque ADB não possui dispositivo conectado. Aceite físico permanece pendente.
