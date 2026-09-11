# Código de projeto — forças e sensores

API escrita em 10/09/2026 e ampliada na mesma data com consultas, camadas e contatos sólidos. Após autorização, os dois exemplos abaixo foram compilados nos testes managed contra a API efetiva. Uma cena separada comprovou força/impulso/torque, consulta de velocidade e callbacks de sensor no Android; isso não significa que estes exemplos exatos tenham sido instalados no aparelho. Ver [evidências](../validacao/2026-09-10-componentes-codigo.md) e [contrato e limites](../adr/ADR-REFUNDACAO-COMPOSICAO-FISICA.md).

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

Callbacks de sensor pertencem aos scripts do corpo sensor e agregam pares de corpos. A referência autoral pode apontar para objeto sem corpo; o comando retorna false. Alterar scripts requer Aplicar e nova sessão Play; hot reload permanece pendente.

## Contatos sólidos (10/09/2026)

`CollisionEnter/Stay/Exit` passaram a existir e chegam aos **dois** objetos do par, um evento por passo físico, agregados por par de corpos:

```csharp
using Astra;

[ComponentId("projeto.registro_de_contato")]
public sealed class RegistroDeContato : Behavior
{
    public override void CollisionEnter(Collision colisao)
        => Scene.Log(ObjectId, colisao.Normal is { } normal
            ? $"encostou em {colisao.Other.ObjectId}; normal {normal}"
            : $"encostou em {colisao.Other.ObjectId}");

    // O fim de um contato não traz geometria: aqui a normal é sempre nula.
    public override void CollisionExit(Collision colisao)
        => Scene.Log(ObjectId, $"separou de {colisao.Other.ObjectId}");
}
```

`Collision.Normal` é **nulo no Exit**: o Jolt não informa geometria quando o contato termina, e devolver um vetor zero pareceria um contato de frente. O par continua sem identificar subcollider — o backend filtra por corpo.

`CharacterMotor` continua **sem** esse encaminhamento: `CharacterVirtual` não participa da broadphase como corpo rígido, então sensores e consultas não acertam o personagem.

## Consultas (10/09/2026)

```csharp
using Astra;
using System.Numerics;

[ComponentId("projeto.mira")]
public sealed class Mira : Behavior
{
    [PropertyId("alcance")] public float Alcance = 8;
    // Vazio aceita qualquer camada; um nome desconhecido não acerta nada.
    [PropertyId("camada")] public string Camada = "";

    public override void Update(float deltaTime)
    {
        var pose = Object.WorldTransform;
        var filtro = QueryFilter.Default;
        filtro.Ignore = ObjectId;
        if (!string.IsNullOrEmpty(Camada)) filtro.LayerMask = Physics.LayerMask(Camada);
        var frente = Vector3.Transform(Vector3.UnitZ, pose.Rotation);
        if (Physics.RayCast(pose.Position, frente * Alcance, filtro) is not { } acerto) return;
        Scene.Log(ObjectId, $"acertou {acerto.Object.ObjectId}, colisor {acerto.ColliderInstance}");
    }
}
```

Devolvem identidade de **objeto** e a **instância do colisor** que respondeu, ponto, distância e fração. A normal do raycast vem de uma segunda consulta ao corpo acertado, não de um zero com cara de contato; `RayHit.Normal` é nulo quando não existe (sobreposição parada). `RayCastAll`/`Overlap` devolvem a contagem real e sinalizam truncamento. Sensores ficam fora por padrão. Raio de comprimento zero é recusado.

## Camadas de gameplay (10/09/2026)

32 camadas nomeadas por projeto, com matriz recíproca, persistidas na cena (arquivo v12). A matriz vale no **solver**: um par proibido não gera contato. A camada é do objeto e vale para o corpo inteiro; camada por colisor seria propriedade sem efeito, porque o filtro do Jolt é por corpo. Sem interface de edição ainda: hoje é API e arquivo.

Contrato completo em [runtime de gameplay](../runtime-gameplay.md); decisões em [ADR](../adr/ADR-RUNTIME-GAMEPLAY.md); resultados em [validação](../validacao/2026-09-10-runtime-gameplay.md).
