namespace Aether.Physics;

/// <summary>Handle para um character controller (item 4.1.5) — índice + geração numa tabela
/// mantida pelo lado nativo, mesmo esquema de <see cref="PhysicsJointHandle"/>:
/// <c>JPH::CharacterVirtual</c> não é adicionado à broadphase e não carrega índice denso
/// reciclável como <see cref="PhysicsBodyHandle"/>.</summary>
public readonly struct PhysicsCharacterHandle : IEquatable<PhysicsCharacterHandle>
{
    internal readonly uint Value;
    internal PhysicsCharacterHandle(uint value) => Value = value;

    public static readonly PhysicsCharacterHandle Invalid = new(0xFFFFFFFFu);
    public bool IsValid => Value != Invalid.Value;

    public bool Equals(PhysicsCharacterHandle o) => Value == o.Value;
    public override bool Equals(object? o) => o is PhysicsCharacterHandle h && Equals(h);
    public override int GetHashCode() => (int)Value;
    public static bool operator ==(PhysicsCharacterHandle a, PhysicsCharacterHandle b) => a.Equals(b);
    public static bool operator !=(PhysicsCharacterHandle a, PhysicsCharacterHandle b) => !a.Equals(b);
}

/// <summary>Estado de sustentação do personagem — mesmo <c>JPH::CharacterBase::EGroundState</c>
/// exposto pela fronteira. <see cref="OnSteepGround"/> é o sinal para aplicar deslizamento (a
/// gravidade acumulada na velocidade, sem cancelar, faz o personagem escorregar ao longo da
/// rampa — o Jolt resolve isso via o solver de contato, não é um cálculo manual nosso).</summary>
public enum CharacterGroundState : uint
{
    OnGround = NativeCharacterGroundState.OnGround,
    OnSteepGround = NativeCharacterGroundState.OnSteepGround,
    NotSupported = NativeCharacterGroundState.NotSupported,
    InAir = NativeCharacterGroundState.InAir,
}

/// <summary>Descreve a cápsula e os parâmetros de movimento de um character controller.
/// Espelha <see cref="NativeCharacterDesc"/> — ver o comentário completo em jolt_bridge.h.
/// <see cref="MaxSlopeAngle"/> em radianos: rampas mais íngremes que isso viram
/// <see cref="CharacterGroundState.OnSteepGround"/> em vez de andáveis.</summary>
public struct CharacterDesc
{
    public float Radius;
    public float StandingHalfHeight;
    public float CrouchingHalfHeight;
    public float MaxSlopeAngle;
    public float Mass;
    public float MaxStrength;

    public static CharacterDesc Default => new()
    {
        Radius = 0.3f,
        StandingHalfHeight = 0.9f,
        CrouchingHalfHeight = 0.4f,
        MaxSlopeAngle = 45f * MathF.PI / 180f,
        Mass = 70f,
        MaxStrength = 100f,
    };

    internal NativeCharacterDesc ToNative() => new()
    {
        Radius = Radius,
        StandingHalfHeight = StandingHalfHeight,
        CrouchingHalfHeight = CrouchingHalfHeight,
        MaxSlopeAngle = MaxSlopeAngle,
        Mass = Mass,
        MaxStrength = MaxStrength,
    };
}

// A API de fato (CreateCharacter/UpdateCharacter/GetCharacterGroundState/...) mora em
// PhysicsWorld.cs como métodos de instância — mesmo padrão de CreateJoint/RayCastAll, para
// PhysicsWorld continuar sendo o único ponto de entrada da fachada de física. Este arquivo
// carrega só os tipos de dado (handle, enum, desc) do character controller, análogo a como
// PhysicsJointHandle/JointDesc moram fisicamente em PhysicsWorld.cs mas os tipos deste
// subsistema (maior) foram separados para não inchar ainda mais aquele arquivo.
