# Física 3D: quebra de junta e colisão do personagem (bloco F)

## Quebra de junta (F041)

Referência: Unity 6000.0 [Joint.breakForce / breakTorque](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Joint-breakForce.html) e [OnJointBreak](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.OnJointBreak.html).

- Junta v3: `break_force` (N) e `break_torque` (N·m), aba Quebra; zero nunca quebra. v2 lê como inquebrável.
- A cada passo físico, `AetherPhysics_GetJointImpulseV1` lê o impulso total da restrição (posição, limites e motor, por tipo de junta do Jolt) e o Play compara impulso ÷ passo com o limite. Passou: a restrição sai do solver no mesmo passo e o evento `broken` (payload: força ou torque) vai para scripts (`Joint.OnBroken`) e para a Conexão de evento (evento 14, "Junta quebrou").
- A junta quebrada fica registrada até o fim do Play: reconstruções da física no mesmo Play não a recriam. O componente autoral não muda; Parar devolve a cena original.

| Aspecto | Astra | Classificação |
|---|---|---|
| Componente depois da quebra | Continua no objeto, sem efeito até o fim do Play | Adaptação explícita: a Unity destrói o `Joint`; aqui o evento chega mesmo a quem assina o componente |
| Callback | Assinatura do evento do componente (`OnBroken`) ou Conexão de evento | Adaptação explícita (sem método mágico no Behavior) |
| Força medida | Impulso médio do passo ÷ passo (1/60 s) | Equivalente ao critério da Unity (força de restrição no passo) |

## Colisão do personagem (F042)

Referência: Unity 6000.0 [OnControllerColliderHit](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/MonoBehaviour.OnControllerColliderHit.html).

- Evento `collider_hit` no Personagem: objeto tocado, ponto e normal. Sai depois do movimento de cada passo físico, uma vez por objeto, para contatos do `CharacterVirtual` com colisão real (`mHadCollision`), sem sensores (`AetherPhysics_GetCharacterContactsV1`).
- Scripts: `Character.OnColliderHit`; Conexão de evento: evento 15, "Personagem bateu num colisor", com filtro pelo outro objeto.

| Aspecto | Astra | Classificação |
|---|---|---|
| Frequência | Uma vez por objeto por passo físico com colisão | Equivalente (a Unity chama a cada `Move` com colisão, inclusive o chão) |
| Direção "para cima" configurável | Sempre +Y do mundo | Não aplicável com motivo: o `CharacterController` da Unity também é sempre Y; o `up_direction` do Godot pede cápsula e motor reorientados, fora deste bloco |

## Sensor/Área (F040)

Referência: Godot 4.5 [Area3D](https://docs.godotengine.org/en/4.5/classes/class_area3d.html) (`monitoring`, `monitorable`); Unity 6000.0 [OnTriggerStay](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Collider.OnTriggerStay.html).

- Corpo v7: "Monitorar" (só em sensores; desligado, o sensor não publica nada) e "Detectável por sensores" (desligado, sensores ignoram o corpo). Filtro aplicado no Play antes de entregar o evento.
- Permanência como evento de componente: `trigger_stay` no Colisor, no máximo uma vez por quadro e par, mesmo com vários passos físicos no quadro. Scripts: `Collider.OnTriggerStay`; Conexão de evento: 16, "Sensor 3D: dentro". O contato sólido continua com Stay só no callback do script.

## Corpo 3D: massa e interpolação (F035)

Referência: Unity 6000.0 [Rigidbody](https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Rigidbody.html) (`automaticCenterOfMass`, `centerOfMass`, `automaticInertiaTensor`, `inertiaTensor`, `interpolation`).

- Corpo v8, aba Massa: centro de massa automático ou manual (ponto local, escalado com o objeto) e inércia automática ou manual (diagonal nos eixos locais, kg·m²). O bridge desloca o centro com `OffsetCenterOfMassShape` e grava a inércia nas propriedades de massa do Jolt (`AetherPhysics_SetBodyMassPropertiesV1`).
- Interpolação (aba Simulação): Nenhuma, Interpolar (pose desenhada entre o passo anterior e o último, atraso de até um passo, como na Unity) e Extrapolar (projeção pela velocidade linear e angular). A física continua no último passo; só a pose publicada muda.

| Aspecto | Astra | Classificação |
|---|---|---|
| Tensor de inércia com rotação | Só diagonal nos eixos locais | Adaptação explícita (`inertiaTensorRotation` não exposto) |
| Densidade | Massa direta | Equivalente ao `Rigidbody.mass` da Unity |

Validação host: `sensor_area_monitoring_monitorable_and_stay_event_once_per_frame` (uma entrada, permanência por quadro e não por passo, sensor surdo e corpo fantasma em silêncio), `body_center_of_mass_and_inertia_change_the_real_jolt_response` (o centro deslocado faz o impulso girar; inércia 10× gira 10× menos), `body_interpolation_publishes_a_pose_between_physics_steps`. Aparelho (07/10, `FisicaF-20261007`): `sensor entradas=1 fantasma=0 quadros-dentro=210`, `centro-de-massa inclinacao=90 equilibrada=0` e `interpolacao` — PASS.

## Validação executada (07/10/2026)

- Host: `joint_breaks_when_constraint_force_passes_*` (v3/v2; peso de 10 kg em junta fixa com limite 50 N quebra uma vez, evento leva dono/instância/força > 50, o peso cai, reconstrução não recria; com 1000 N a junta segura) e `character_movement_reports_controller_collider_hit_*` (personagem andando contra parede: evento com a parede, normal −X e ponto na face). Suíte 1427/1431 (4 falhas anteriores à branch); C# 521/521.
- UI executável: aba Quebra em `docs/validacao/evidencias/joint-break-20261007/`.
- Aparelho (07/10, projeto `FisicaF-20261007`): `junta quebrou=True forca=98` e `personagem bateu na parede=True` — PASS, 0 erros Vulkan (`docs/validacao/evidencias/physics-f-20261007/aparelho-logcat.txt`).
