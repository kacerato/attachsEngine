namespace Aether.Physics;

/// <summary>Handle denso para um corpo físico — o mesmo <c>JPH::BodyID</c> empacotado, visto do
/// lado gerenciado. <see cref="Invalid"/> nunca é devolvido por uma criação bem-sucedida.</summary>
public readonly struct PhysicsBodyHandle : IEquatable<PhysicsBodyHandle>
{
    internal readonly uint Value;
    internal PhysicsBodyHandle(uint value) => Value = value;

    public static readonly PhysicsBodyHandle Invalid = new(0xFFFFFFFFu);
    public bool IsValid => Value != Invalid.Value;

    public bool Equals(PhysicsBodyHandle o) => Value == o.Value;
    public override bool Equals(object? o) => o is PhysicsBodyHandle h && Equals(h);
    public override int GetHashCode() => (int)Value;
    public static bool operator ==(PhysicsBodyHandle a, PhysicsBodyHandle b) => a.Equals(b);
    public static bool operator !=(PhysicsBodyHandle a, PhysicsBodyHandle b) => !a.Equals(b);
}

/// <summary>Handle para uma junta (item 4.1.3) — índice + geração numa tabela mantida pelo lado
/// nativo (ver comentário de <c>JointSlot</c> em jolt_bridge.cpp), não um <c>JPH::BodyID</c> como
/// <see cref="PhysicsBodyHandle"/>: uma <c>JPH::Constraint</c> não carrega um índice denso
/// reciclável embutido, diferente de corpo.</summary>
public readonly struct PhysicsJointHandle : IEquatable<PhysicsJointHandle>
{
    internal readonly uint Value;
    internal PhysicsJointHandle(uint value) => Value = value;

    public static readonly PhysicsJointHandle Invalid = new(0xFFFFFFFFu);
    public bool IsValid => Value != Invalid.Value;

    public bool Equals(PhysicsJointHandle o) => Value == o.Value;
    public override bool Equals(object? o) => o is PhysicsJointHandle h && Equals(h);
    public override int GetHashCode() => (int)Value;
    public static bool operator ==(PhysicsJointHandle a, PhysicsJointHandle b) => a.Equals(b);
    public static bool operator !=(PhysicsJointHandle a, PhysicsJointHandle b) => !a.Equals(b);
}

/// <summary>Estado e alvo de um motor de junta — só tem efeito em <see cref="JointKind.Hinge"/>
/// e <see cref="JointKind.Slider"/> (Point/Distance não têm motor no Jolt, ver jolt_bridge.h).
/// <see cref="TargetVelocity"/> é rad/s (Hinge) ou m/s (Slider); <see cref="TargetPosition"/> é
/// rad (Hinge) ou m (Slider), usado só quando <see cref="State"/> inclui Position.
/// <see cref="SpringFrequency"/>/<see cref="SpringDamping"/> alimentam o modo
/// FrequencyAndDamping da mola do Jolt que dirige o motor de posição — ignorados em modo
/// Velocity puro.</summary>
public struct JointMotor
{
    public NativeMotorState State;
    public float TargetVelocity;
    public float TargetPosition;
    public float MaxForceOrTorque;
    public float SpringFrequency;
    public float SpringDamping;

    public static readonly JointMotor Off = default;

    public static JointMotor Velocity(float targetVelocity, float maxForceOrTorque) => new()
    {
        State = NativeMotorState.Velocity,
        TargetVelocity = targetVelocity,
        MaxForceOrTorque = maxForceOrTorque,
    };

    public static JointMotor Position(float targetPosition, float maxForceOrTorque, float springFrequency = 4f, float springDamping = 1f) => new()
    {
        State = NativeMotorState.Position,
        TargetPosition = targetPosition,
        MaxForceOrTorque = maxForceOrTorque,
        SpringFrequency = springFrequency,
        SpringDamping = springDamping,
    };

    internal NativeJointMotorDesc ToNative() => new()
    {
        State = State,
        TargetVelocity = TargetVelocity,
        TargetPosition = TargetPosition,
        MaxForceOrTorque = MaxForceOrTorque,
        SpringFrequency = SpringFrequency,
        SpringDamping = SpringDamping,
    };
}

/// <summary>Tipo de junta — as 4 mais comuns em jogos (ver jolt_bridge.h para por que SixDOF e
/// as demais ficam fora desta fatia).</summary>
public enum JointKind : uint
{
    Point = NativeJointKind.Point,
    Hinge = NativeJointKind.Hinge,
    Slider = NativeJointKind.Slider,
    Distance = NativeJointKind.Distance,
}

/// <summary>Descreve uma junta a criar entre dois corpos do mesmo <see cref="PhysicsWorld"/>.
/// Espelha <see cref="NativeJointDesc"/> — ver o comentário completo em jolt_bridge.h para a
/// convenção de cada campo por tipo de junta (o que é ignorado, unidades, espaço mundial).
/// <para>
/// AVISO específico de <see cref="JointKind.Hinge"/>: o Jolt exige <see cref="LimitsMin"/> em
/// [-pi,0] e <see cref="LimitsMax"/> em [0,pi] (não é validação desta fachada — é a API real,
/// ver <c>HingeConstraint::SetLimits</c>). Para uma dobradiça sem limite físico (gira livre
/// como uma roda), use exatamente <c>-MathF.PI</c>/<c>MathF.PI</c> — é o valor exato em que o
/// Jolt desliga a checagem de limite internamente, não um "limite muito largo" qualquer.
/// </para>
/// </summary>
public struct JointDesc
{
    public JointKind Kind;
    public float3 Point1;
    public float3 Point2;
    public float3 Axis1;
    public float3 Axis2;
    public float LimitsMin;
    public float LimitsMax;
    public JointMotor Motor;

    internal NativeJointDesc ToNative() => new()
    {
        Kind = (NativeJointKind)Kind,
        Point1 = Point1,
        Point2 = Point2,
        Axis1 = Axis1,
        Axis2 = Axis2,
        LimitsMin = LimitsMin,
        LimitsMax = LimitsMax,
        Motor = Motor.ToNative(),
    };
}

/// <summary>Descreve a forma de colisão de um corpo. Espelha <see cref="NativeShapeDesc"/> num
/// formato mais confortável para código gerenciado (union manual via dois construtores nomeados
/// em vez de dois campos que o chamador precisa preencher certo na mão).</summary>
public readonly struct PhysicsShape
{
    public readonly NativeShapeKind Kind;
    public readonly float3 BoxHalfExtent;
    public readonly float SphereRadius;

    private PhysicsShape(NativeShapeKind kind, float3 boxHalfExtent, float sphereRadius)
    { Kind = kind; BoxHalfExtent = boxHalfExtent; SphereRadius = sphereRadius; }

    public static PhysicsShape Box(float3 halfExtent) => new(NativeShapeKind.Box, halfExtent, 0f);
    public static PhysicsShape Sphere(float radius) => new(NativeShapeKind.Sphere, default, radius);

    internal NativeShapeDesc ToNative() => new()
    {
        Kind = Kind,
        BoxHalfExtent = BoxHalfExtent,
        SphereRadius = SphereRadius,
    };
}

/// <summary>
/// Wrapper gerenciado de um <c>AetherPhysicsWorld*</c> nativo. Dono do ponteiro nativo — chamar
/// <see cref="Dispose"/> (ou deixar o finalizador rodar, como rede de segurança) libera o mundo e
/// invalida todo <see cref="PhysicsBodyHandle"/> emitido por ele, na mesma disciplina do lado C++
/// (ver comentário de <c>AetherPhysics_DestroyWorld</c> em <c>jolt_bridge.h</c>).
/// <para>
/// Cada chamada nativa aqui é uma chamada P/Invoke individual — aceitável para criação/destruição
/// de corpo (evento raro, não roda every frame), mas <see cref="PhysicsSyncSystem"/> agrupa os
/// <c>Step</c>+<c>GetTransform</c> do caminho quente para respeitar o orçamento de 200 chamadas
/// nativas por frame (<c>docs/CONVENCOES.md</c> §2).
/// </para>
/// </summary>
public sealed class PhysicsWorld : IDisposable
{
    private nint _handle;
    private bool _disposed;

    public PhysicsWorld(float3 gravity, uint maxBodies = 1024)
    {
        _handle = NativePhysics.AetherPhysics_CreateWorld(gravity, maxBodies);
        if (_handle == nint.Zero)
            throw new InvalidOperationException("Falha ao criar o mundo de física nativo (Jolt) — ver stderr para detalhes do Jolt.");
    }

    internal nint Handle => _disposed ? throw new ObjectDisposedException(nameof(PhysicsWorld)) : _handle;

    public PhysicsBodyHandle CreateBody(in PhysicsShape shape, float3 position, quaternion rotation,
        NativeMotionType motionType, float friction = 0.5f, float restitution = 0.0f)
    {
        var desc = new NativeBodyDesc
        {
            Shape = shape.ToNative(),
            Position = position,
            Rotation = rotation,
            MotionType = motionType,
            Friction = friction,
            Restitution = restitution,
        };
        uint raw = NativePhysics.AetherPhysics_CreateBody(Handle, in desc);
        return new PhysicsBodyHandle(raw);
    }

    public void DestroyBody(PhysicsBodyHandle handle)
    {
        if (!handle.IsValid) return;
        NativePhysics.AetherPhysics_DestroyBody(Handle, handle.Value);
    }

    public void Step(float deltaTime, int collisionSteps = 1) =>
        NativePhysics.AetherPhysics_Step(Handle, deltaTime, collisionSteps);

    public unsafe void GetTransform(PhysicsBodyHandle handle, out float3 position, out quaternion rotation)
    {
        position = default;
        rotation = quaternion.Identity;
        if (!handle.IsValid) return;
        fixed (float3* p = &position)
        fixed (quaternion* r = &rotation)
        {
            NativePhysics.AetherPhysics_GetTransform(Handle, handle.Value, p, r);
        }
    }

    public void SetLinearVelocity(PhysicsBodyHandle handle, float3 velocity)
    {
        if (!handle.IsValid) return;
        NativePhysics.AetherPhysics_SetLinearVelocity(Handle, handle.Value, velocity);
    }

    public float3 GetLinearVelocity(PhysicsBodyHandle handle) =>
        handle.IsValid ? NativePhysics.AetherPhysics_GetLinearVelocity(Handle, handle.Value) : float3.Zero;

    public bool IsActive(PhysicsBodyHandle handle) =>
        handle.IsValid && NativePhysics.AetherPhysics_IsActive(Handle, handle.Value) != 0;

    public unsafe bool RayCastClosest(float3 origin, float3 direction, out PhysicsBodyHandle hitBody, out float hitFraction)
    {
        uint rawBody = PhysicsBodyHandle.Invalid.Value;
        float fraction = 0f;
        int hit = NativePhysics.AetherPhysics_RayCastClosest(Handle, origin, direction, &rawBody, &fraction);
        hitBody = new PhysicsBodyHandle(rawBody);
        hitFraction = fraction;
        return hit != 0;
    }

    // ---------------------------------------------------------------- juntas e motores (4.1.3)

    /// <summary>Cria uma junta entre dois corpos deste mundo. Devolve <see cref="PhysicsJointHandle.Invalid"/>
    /// se algum dos dois handles de corpo for inválido — mesma disciplina defensiva do resto
    /// desta fachada (nunca lança por handle ruim, só devolve inválido).</summary>
    public PhysicsJointHandle CreateJoint(PhysicsBodyHandle body1, PhysicsBodyHandle body2, in JointDesc desc)
    {
        if (!body1.IsValid || !body2.IsValid) return PhysicsJointHandle.Invalid;
        NativeJointDesc native = desc.ToNative();
        uint raw = NativePhysics.AetherPhysics_CreateJoint(Handle, body1.Value, body2.Value, in native);
        return new PhysicsJointHandle(raw);
    }

    public void DestroyJoint(PhysicsJointHandle handle)
    {
        if (!handle.IsValid) return;
        NativePhysics.AetherPhysics_DestroyJoint(Handle, handle.Value);
    }

    /// <summary>Atualiza o motor de uma junta Hinge/Slider já criada. Sem efeito em Point/Distance
    /// (não têm motor) ou handle inválido — não é erro do chamador, ver jolt_bridge.h.</summary>
    public void SetJointMotor(PhysicsJointHandle handle, in JointMotor motor)
    {
        if (!handle.IsValid) return;
        NativeJointMotorDesc native = motor.ToNative();
        NativePhysics.AetherPhysics_SetJointMotor(Handle, handle.Value, in native);
    }

    /// <summary>Ângulo atual em radianos (Hinge) ou posição atual em metros (Slider) ao longo do
    /// eixo da junta. 0 para Point/Distance/handle inválido — ver jolt_bridge.h.</summary>
    public float GetJointPosition(PhysicsJointHandle handle) =>
        handle.IsValid ? NativePhysics.AetherPhysics_GetJointPosition(Handle, handle.Value) : 0f;

    public void Dispose()
    {
        if (_disposed) return;
        NativePhysics.AetherPhysics_DestroyWorld(_handle);
        _handle = nint.Zero;
        _disposed = true;
        GC.SuppressFinalize(this);
    }

    ~PhysicsWorld()
    {
        // Rede de segurança: um mundo de física esquecido sem Dispose ainda libera o recurso
        // nativo, embora mais tarde (não confiar nisso como caminho normal — ver Dispose acima).
        if (!_disposed && _handle != nint.Zero) NativePhysics.AetherPhysics_DestroyWorld(_handle);
    }
}
