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

/// <summary>Estado de alto nível derivado pelo <see cref="CharacterMotorSystem"/>.
/// Não replica <see cref="CharacterGroundState"/>: Rising/Falling representam a
/// intenção balística, enquanto Sliding representa contato não caminhável.</summary>
public enum CharacterMotionState : uint
{
    Falling,
    Grounded,
    Rising,
    Sliding,
}

public enum CharacterStance : uint
{
    Standing,
    Crouching,
}

/// <summary>Parâmetros reutilizáveis do motor. A velocidade desejada vem do
/// gameplay/input, mas gravidade, plataforma e aderência ao chão são compostas
/// aqui de forma determinística.</summary>
public struct CharacterMotorSettings
{
    public float3 Gravity;
    public float MaxFallSpeed;
    public float GroundStickSpeed;
    public bool InheritGroundVelocity;

    public static CharacterMotorSettings Default => new()
    {
        Gravity = new float3(0f, -9.81f, 0f),
        MaxFallSpeed = 55f,
        GroundStickSpeed = 0.25f,
        InheritGroundVelocity = true,
    };
}

/// <summary>Estado por personagem que o chamador guarda entre fixed steps.
/// É um struct sem referência gerenciada para poder morar futuramente num chunk
/// ECS sem mudar o contrato do sistema.</summary>
public struct CharacterMotorState
{
    public float3 GravityVelocity;
    public CharacterMotionState MotionState;
    public CharacterStance Stance;

    public static CharacterMotorState Initial => new()
    {
        MotionState = CharacterMotionState.Falling,
        Stance = CharacterStance.Standing,
    };
}

/// <summary>Entrada de um fixed step. <see cref="DesiredVelocity"/> é a
/// velocidade de locomoção em espaço de mundo, sem gravidade e sem a velocidade
/// da plataforma; o sistema compõe ambas exatamente uma vez.</summary>
public readonly struct CharacterMotorInput
{
    public readonly float3 DesiredVelocity;
    public readonly QueryLayerMask LayerMask;
    public readonly PhysicsBodyHandle IgnoreBody;

    public CharacterMotorInput(float3 desiredVelocity,
        QueryLayerMask layerMask = QueryLayerMask.All,
        PhysicsBodyHandle? ignoreBody = null)
    {
        DesiredVelocity = desiredVelocity;
        LayerMask = layerMask;
        IgnoreBody = ignoreBody is { IsValid: true } value ? value : PhysicsBodyHandle.Invalid;
    }
}

/// <summary>
/// Contrato único de composição do character já existente. Deve ser chamado
/// uma vez por personagem antes do único <see cref="PhysicsWorld.Step"/> do
/// fixed tick. Ele integra gravidade, limita queda, herda a velocidade do chão,
/// alimenta o ExtendedUpdate (degraus/stick-to-floor) e publica um estado claro.
/// Não contém input, câmera, jump rules, escalada ou natação.
/// </summary>
public static class CharacterMotorSystem
{
    public static CharacterMotionState UpdateBeforePhysics(
        PhysicsWorld physicsWorld,
        PhysicsCharacterHandle character,
        ref CharacterMotorState state,
        in CharacterMotorSettings settings,
        in CharacterMotorInput input,
        float deltaTime)
    {
        ArgumentNullException.ThrowIfNull(physicsWorld);
        if (!character.IsValid)
            throw new ArgumentException("O CharacterMotor recebeu um handle de personagem inválido.", nameof(character));
        Validate(in settings, in input, deltaTime);

        CharacterGroundState groundBefore = physicsWorld.GetCharacterGroundState(character);
        float gravityLength = settings.Gravity.Length;
        float3 gravityDirection = gravityLength > 1e-6f
            ? settings.Gravity / gravityLength
            : float3.Zero;

        bool supported = groundBefore == CharacterGroundState.OnGround;
        float fallingSpeed = math.Dot(state.GravityVelocity, gravityDirection);
        if (supported && fallingSpeed >= 0f)
        {
            state.GravityVelocity = gravityDirection * settings.GroundStickSpeed;
        }
        else
        {
            state.GravityVelocity += settings.Gravity * deltaTime;
            float updatedFallingSpeed = math.Dot(state.GravityVelocity, gravityDirection);
            if (updatedFallingSpeed > settings.MaxFallSpeed)
                state.GravityVelocity -= gravityDirection *
                    (updatedFallingSpeed - settings.MaxFallSpeed);
        }

        float3 composed = input.DesiredVelocity + state.GravityVelocity;
        if (settings.InheritGroundVelocity && groundBefore != CharacterGroundState.InAir)
            composed += physicsWorld.GetCharacterGroundVelocity(character);

        physicsWorld.SetCharacterVelocity(character, composed);
        physicsWorld.UpdateCharacter(character, deltaTime, settings.Gravity, input.LayerMask,
            input.IgnoreBody.IsValid ? input.IgnoreBody : null);

        CharacterGroundState groundAfter = physicsWorld.GetCharacterGroundState(character);
        state.MotionState = Classify(groundAfter, state.GravityVelocity, gravityDirection);
        return state.MotionState;
    }

    /// <summary>Define uma velocidade ao longo do eixo oposto à gravidade. Útil
    /// para jump/launch sem embutir no Core a regra que decide quando ele pode ocorrer.</summary>
    public static void SetVerticalSpeed(
        ref CharacterMotorState state, in CharacterMotorSettings settings, float speed)
    {
        if (!float.IsFinite(speed))
            throw new ArgumentOutOfRangeException(nameof(speed), "A velocidade vertical precisa ser finita.");
        float length = settings.Gravity.Length;
        float3 up = length > 1e-6f ? -settings.Gravity / length : float3.Up;
        state.GravityVelocity = up * speed;
    }

    public static bool TrySetStance(
        PhysicsWorld physicsWorld,
        PhysicsCharacterHandle character,
        ref CharacterMotorState state,
        CharacterStance stance)
    {
        ArgumentNullException.ThrowIfNull(physicsWorld);
        if (!character.IsValid) return false;
        if (state.Stance == stance) return true;

        bool changed = physicsWorld.SetCharacterCrouching(
            character, stance == CharacterStance.Crouching);
        if (changed) state.Stance = stance;
        return changed;
    }

    private static CharacterMotionState Classify(
        CharacterGroundState ground, float3 gravityVelocity, float3 gravityDirection)
    {
        if (ground is CharacterGroundState.OnSteepGround or CharacterGroundState.NotSupported)
            return CharacterMotionState.Sliding;
        if (ground == CharacterGroundState.OnGround)
            return CharacterMotionState.Grounded;
        if (gravityDirection.LengthSquared > 0f && math.Dot(gravityVelocity, gravityDirection) < -1e-4f)
            return CharacterMotionState.Rising;
        return CharacterMotionState.Falling;
    }

    private static void Validate(
        in CharacterMotorSettings settings, in CharacterMotorInput input, float deltaTime)
    {
        if (!float.IsFinite(deltaTime) || deltaTime <= 0f)
            throw new ArgumentOutOfRangeException(nameof(deltaTime), "O fixed delta do CharacterMotor precisa ser finito e positivo.");
        if (!IsFinite(settings.Gravity) || !float.IsFinite(settings.MaxFallSpeed) || settings.MaxFallSpeed <= 0f ||
            !float.IsFinite(settings.GroundStickSpeed) || settings.GroundStickSpeed < 0f)
            throw new ArgumentException("As configurações do CharacterMotor precisam ser finitas e seus limites positivos.", nameof(settings));
        if (!IsFinite(input.DesiredVelocity))
            throw new ArgumentException("A velocidade desejada do CharacterMotor precisa ser finita.", nameof(input));
        if (input.LayerMask == QueryLayerMask.None)
            throw new ArgumentException("O CharacterMotor precisa consultar ao menos uma camada física.", nameof(input));
    }

    private static bool IsFinite(float3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);
}

// A API nativa de baixo nível (CreateCharacter/UpdateCharacter/queries) mora em PhysicsWorld.cs,
// que continua sendo a única fronteira com o handle do mundo. Este arquivo concentra os tipos e
// a composição gerenciada reutilizável; CharacterMotorSystem não possui o PhysicsWorld nem o
// handle e, portanto, não duplica ownership nem cria outro lifecycle.
