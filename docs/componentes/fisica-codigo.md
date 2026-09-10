# Código de projeto — forças e sensores

API escrita em 10/09/2026. Após autorização, os dois exemplos abaixo foram compilados nos testes managed contra a API efetiva. Uma cena separada comprovou força/impulso/torque, consulta de velocidade e callbacks de sensor no Android; isso não significa que estes exemplos exatos tenham sido instalados no aparelho. Ver [evidências](../validacao/2026-09-10-componentes-codigo.md) e [contrato e limites](../adr/ADR-REFUNDACAO-COMPOSICAO-FISICA.md).

## Força contínua

Anexar Corpo físico Dinâmico e Colisor 3D, criar/aplicar o script e anexá-lo. Vetores são globais, independentes da malha. Não multiplicar força por deltaTime: o integrador aplica o tempo.

```csharp
using Astra;
using System;
using System.Numerics;

[ComponentId("projeto.forca_continua")]
public sealed class ForcaContinua : Behavior
{
    [PropertyId("forca")]
    public Vector3 Forca = new(0, 12, 0);

    [PropertyId("torque")]
    public Vector3 Torque = Vector3.Zero;

    public override void FixedUpdate(float deltaTime)
    {
        if (!Scene.AddForce(ObjectId, Forca) || !Scene.AddTorque(ObjectId, Torque))
            throw new InvalidOperationException("Requer corpo dinâmico.");
    }
}
```

AddImpulse/AddAngularImpulse são instantâneos; GetBodyVelocity lê velocidade mundial e lança erro sem corpo ativo. SetBodyVelocity aceita cinemáticos; forças exigem dinâmica livre.

## Sensor e referência do inspetor

Objeto Sensor: Corpo físico com Sensor habilitado e Colisor 3D; Malha é opcional. Para observar estáticos, usar sensor cinemático. Escolher Alvo pelo seletor de objetos; o impulso requer que esse alvo tenha corpo dinâmico.

```csharp
using Astra;
using System.Numerics;

[ComponentId("projeto.sensor_impulso")]
public sealed class SensorImpulso : Behavior
{
    [PropertyId("alvo")]
    public ObjectReference Alvo;

    [PropertyId("impulso")]
    public Vector3 Impulso = new(0, 3, 0);

    public override void TriggerEnter(ObjectReference other)
    {
        if (Alvo.ObjectId == 0 || !Scene.Exists(Alvo.ObjectId))
        {
            Scene.Log(ObjectId, "Escolha um objeto existente em Alvo.");
            return;
        }
        if (!Scene.AddImpulse(Alvo.ObjectId, Impulso))
            Scene.Log(ObjectId, "Alvo não possui corpo dinâmico ativo.");
    }

    public override void TriggerExit(ObjectReference other)
        => Scene.Log(ObjectId, $"Saiu do sensor: {other.ObjectId}");
}
```

Callbacks pertencem aos scripts do corpo sensor e agregam pares de corpos. Não identificam subcollider. CharacterMotor e contatos sólidos não têm esse encaminhamento. A referência autoral pode apontar para objeto sem corpo; o comando retorna false. Alterar scripts requer Aplicar e nova sessão Play; hot reload permanece pendente.
