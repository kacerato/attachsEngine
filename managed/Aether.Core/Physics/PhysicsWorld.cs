namespace Aether.Physics;

/// <summary>Handle denso para um corpo físico — o mesmo <c>JPH::BodyID</c> empacotado, visto do
/// lado gerenciado. <see cref="Invalid"/> nunca é devolvido por uma criação bem-sucedida.</summary>
[System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
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

/// <summary>Filtro de camada para queries (item 4.1.4). Combinável com OR bit a bit —
/// <c>QueryLayerMask.Static | QueryLayerMask.Dynamic</c> equivale a <see cref="All"/>. Mapeia
/// 1:1 para as duas únicas camadas que o mundo tem hoje (estático vs dinâmico/cinemático, ver
/// jolt_bridge.cpp); um esquema de N camadas nomeadas continua sendo trabalho futuro (mesma
/// limitação já documentada para 4.1.1).</summary>
[Flags]
public enum QueryLayerMask : uint
{
    None = NativeQueryLayerMask.None,
    Static = NativeQueryLayerMask.Static,
    Dynamic = NativeQueryLayerMask.Dynamic,
    All = NativeQueryLayerMask.All,
}

/// <summary>Fase do overlap persistente de um <see cref="Trigger"/>. Enter aparece
/// no primeiro passo em contato, Stay nos passos seguintes e Exit no primeiro
/// passo em que o último subcontato entre os dois corpos desaparece.</summary>
public enum TriggerEventType : uint
{
    Enter = 0,
    Stay = 1,
    Exit = 2,
}

/// <summary>Evento determinístico publicado por <see cref="PhysicsWorld.GetTriggerEvents"/>.
/// Os handles são os mesmos gravados em <see cref="RigidBody.Handle"/>; um sensor-sensor
/// gera um evento dirigido para cada sensor cujo filtro aceite o outro.</summary>
[System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
public struct PhysicsTriggerEvent
{
    public PhysicsBodyHandle Sensor;
    public PhysicsBodyHandle Other;
    public TriggerEventType Type;
    private uint _reserved;
}

/// <summary>Um hit de <see cref="PhysicsWorld.ShapeCastClosest"/> ou <see cref="PhysicsWorld.OverlapShape"/>
/// — inclui ponto e direção de contato, ao contrário de <see cref="PhysicsWorld.RayCastClosest"/>/
/// <see cref="PhysicsWorld.RayCastAll"/> (que só têm corpo+fração; um raio não gera contato de
/// forma-contra-forma no Jolt sem uma segunda consulta). <see cref="Fraction"/> só é
/// significativo em <see cref="PhysicsWorld.ShapeCastClosest"/> — em overlap parado não há "ao
/// longo de quê" medir, fica sempre 0.</summary>
public readonly struct ShapeQueryHit
{
    public readonly PhysicsBodyHandle Body;
    public readonly float Fraction;
    public readonly float3 ContactPointOnQuery;
    public readonly float3 ContactPointOnHit;
    public readonly float3 PenetrationAxis;

    internal ShapeQueryHit(in NativeShapeQueryHit native)
    {
        Body = new PhysicsBodyHandle(native.Body);
        Fraction = native.Fraction;
        ContactPointOnQuery = native.ContactPointOnQuery;
        ContactPointOnHit = native.ContactPointOnHit;
        PenetrationAxis = native.PenetrationAxis;
    }
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

/// <summary>Referencial de autoria dos pontos/eixos de uma junta. Em modos locais, todos os
/// pontos são transformados por posição+rotação do corpo indicado e todos os eixos somente por
/// sua rotação no instante da criação.</summary>
public enum JointSpace : uint
{
    World = NativeJointSpace.World,
    LocalToBody1 = NativeJointSpace.LocalToBody1,
    LocalToBody2 = NativeJointSpace.LocalToBody2,
}

/// <summary>Descreve uma junta a criar entre dois corpos do mesmo <see cref="PhysicsWorld"/>.
/// Espelha <see cref="NativeJointDescV2"/> — ver o comentário completo em jolt_bridge.h para a
/// convenção de cada campo por tipo de junta (o que é ignorado, unidades e referencial).
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
    public JointSpace Space;
    public float3 Point1;
    public float3 Point2;
    public float3 Axis1;
    public float3 Axis2;
    public float LimitsMin;
    public float LimitsMax;
    public JointMotor Motor;

    internal NativeJointDescV2 ToNativeV2() => new()
    {
        StructSize = (uint)System.Runtime.InteropServices.Marshal.SizeOf<NativeJointDescV2>(),
        ApiVersion = 2,
        Kind = (NativeJointKind)Kind,
        Space = (NativeJointSpace)Space,
        Point1 = Point1,
        Point2 = Point2,
        Axis1 = Axis1,
        Axis2 = Axis2,
        LimitsMin = LimitsMin,
        LimitsMax = LimitsMax,
        Motor = Motor.ToNative(),
    };

    internal readonly bool IsValid()
    {
        if (Kind is < JointKind.Point or > JointKind.Distance ||
            Space is < JointSpace.World or > JointSpace.LocalToBody2 ||
            !Finite(Point1) || !Finite(Point2) || !Finite(Axis1) || !Finite(Axis2) ||
            !float.IsFinite(LimitsMin) || !float.IsFinite(LimitsMax) ||
            Motor.State is < NativeMotorState.Off or > NativeMotorState.PositionAndVelocity ||
            !float.IsFinite(Motor.TargetVelocity) || !float.IsFinite(Motor.TargetPosition) ||
            !float.IsFinite(Motor.MaxForceOrTorque) || Motor.MaxForceOrTorque < 0f ||
            !float.IsFinite(Motor.SpringFrequency) || Motor.SpringFrequency < 0f ||
            !float.IsFinite(Motor.SpringDamping) || Motor.SpringDamping < 0f)
            return false;

        if (Kind is JointKind.Hinge or JointKind.Slider)
        {
            if (LengthSquared(Axis1) <= 1e-12f || LengthSquared(Axis2) <= 1e-12f ||
                LimitsMin > LimitsMax)
                return false;
        }
        return Kind != JointKind.Hinge ||
               (LimitsMin >= -MathF.PI && LimitsMin <= 0f &&
                LimitsMax >= 0f && LimitsMax <= MathF.PI);
    }

    private static bool Finite(float3 value) =>
        float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);

    private static float LengthSquared(float3 value) =>
        value.X * value.X + value.Y * value.Y + value.Z * value.Z;
}

/// <summary>Descreve a forma de colisão de um corpo. Espelha <see cref="NativeShapeDesc"/> num
/// formato mais confortável para código gerenciado (union manual via dois construtores nomeados
/// em vez de dois campos que o chamador precisa preencher certo na mão).</summary>
public readonly struct PhysicsShape
{
    public readonly NativeShapeKind Kind;
    public readonly float3 BoxHalfExtent;
    public readonly float SphereRadius;
    public readonly float CapsuleHalfHeight;

    private PhysicsShape(NativeShapeKind kind, float3 boxHalfExtent, float radius, float capsuleHalfHeight)
    { Kind = kind; BoxHalfExtent = boxHalfExtent; SphereRadius = radius; CapsuleHalfHeight = capsuleHalfHeight; }

    public static PhysicsShape Box(float3 halfExtent) => new(NativeShapeKind.Box, halfExtent, 0f, 0f);
    public static PhysicsShape Sphere(float radius) => new(NativeShapeKind.Sphere, default, radius, 0f);
    /// <summary>Item 4.1.5 — cápsula (cilindro com tampas esféricas), a forma padrão de
    /// character controller. <paramref name="halfHeight"/> é a meia-altura do CILINDRO (sem as
    /// tampas); altura total = 2*(halfHeight+radius).</summary>
    public static PhysicsShape Capsule(float radius, float halfHeight) => new(NativeShapeKind.Capsule, default, radius, halfHeight);

    internal NativeShapeDesc ToNative() => new()
    {
        Kind = Kind,
        BoxHalfExtent = BoxHalfExtent,
        SphereRadius = SphereRadius,
        CapsuleHalfHeight = CapsuleHalfHeight,
    };
}

/// <summary>Descritor blittable para criação unitária ou em lote. O layout espelha
/// <c>AetherBodyDescV2</c>, mas os campos de versão ficam encapsulados para que todo
/// descritor construído por esta API atravesse a ABI com um contrato válido.</summary>
[System.Runtime.InteropServices.StructLayout(System.Runtime.InteropServices.LayoutKind.Sequential)]
public struct PhysicsBodyDescription
{
    private uint _structSize;
    private uint _apiVersion;
    private NativeShapeDesc _shape;
    public float3 Position;
    public quaternion Rotation;
    public NativeMotionType MotionType;
    public float Friction;
    public float Restitution;
    public AllowedDOFs AllowedDOFs;
    private uint _isSensor;
    private QueryLayerMask _eventLayerMask;

    public readonly PhysicsShape Shape => _shape.Kind switch
    {
        NativeShapeKind.Box => PhysicsShape.Box(_shape.BoxHalfExtent),
        NativeShapeKind.Capsule => PhysicsShape.Capsule(_shape.SphereRadius, _shape.CapsuleHalfHeight),
        _ => PhysicsShape.Sphere(_shape.SphereRadius),
    };
    public bool IsSensor { readonly get => _isSensor != 0; set => _isSensor = value ? 1u : 0u; }
    public QueryLayerMask EventLayerMask
    {
        readonly get => _eventLayerMask;
        set
        {
            if ((value & ~QueryLayerMask.All) != 0) throw new ArgumentOutOfRangeException(nameof(value));
            _eventLayerMask = value;
        }
    }

    public PhysicsBodyDescription(in PhysicsShape shape, float3 position, quaternion rotation,
        NativeMotionType motionType, float friction = 0.5f, float restitution = 0f,
        AllowedDOFs allowedDOFs = AllowedDOFs.All, bool isSensor = false,
        QueryLayerMask eventLayerMask = QueryLayerMask.All)
    {
        if ((eventLayerMask & ~QueryLayerMask.All) != 0)
            throw new ArgumentOutOfRangeException(nameof(eventLayerMask));
        _structSize = (uint)System.Runtime.InteropServices.Marshal.SizeOf<PhysicsBodyDescription>();
        _apiVersion = 2;
        _shape = shape.ToNative();
        Position = position;
        Rotation = rotation;
        MotionType = motionType;
        Friction = friction;
        Restitution = restitution;
        AllowedDOFs = allowedDOFs;
        _isSensor = isSensor ? 1u : 0u;
        _eventLayerMask = eventLayerMask;
    }
}

/// <summary>Graus de liberdade permitidos a um corpo dinâmico/cinemático (item 4.1.6, física
/// 2D). Espelha <see cref="NativeAllowedDOFs"/> — ver o aviso de convenção lá (<see cref="All"/>
/// == 0, não os 6 bits do Jolt) e o comentário completo em jolt_bridge.h. Um corpo
/// <see cref="NativeMotionType.Dynamic"/> com nenhum eixo de translação livre (ex.: só
/// <see cref="RotationZ"/> sozinho) é recusado por <see cref="PhysicsWorld.CreateBody"/>
/// (devolve <see cref="PhysicsBodyHandle.Invalid"/>) — o Jolt trata essa combinação como
/// inválida (crasha por divisão por zero em corpo Dynamic; um corpo totalmente travado deveria
/// ser <see cref="NativeMotionType.Static"/>).</summary>
[Flags]
public enum AllowedDOFs : uint
{
    All = NativeAllowedDOFs.All,
    TranslationX = NativeAllowedDOFs.TranslationX,
    TranslationY = NativeAllowedDOFs.TranslationY,
    TranslationZ = NativeAllowedDOFs.TranslationZ,
    RotationX = NativeAllowedDOFs.RotationX,
    RotationY = NativeAllowedDOFs.RotationY,
    RotationZ = NativeAllowedDOFs.RotationZ,
    /// <summary>Plano XY: corpo 2D andando/caindo "na tela" (plataforma vista de lado/de
    /// frente) — trava profundidade e as rotações que tirariam o corpo do plano.</summary>
    Plane2D = NativeAllowedDOFs.Plane2D,
}

/// <summary>Comportamento ao esgotar uma capacidade da simulação. O padrão interrompe
/// imediatamente builds Debug/teste e registra warnings limitados por frequência em Release.</summary>
public enum PhysicsOverflowPolicy : uint
{
    BuildDefault = 0,
    /// <summary>Em Release, registra warning e contador. Em Debug, o assert interno do próprio
    /// Jolt continua fail-fast antes que o retorno possa ser consumido.</summary>
    Warning = 1,
    FailFast = 2,
}

/// <summary>Flags devolvidas pelo passo do Jolt. Qualquer valor diferente de
/// <see cref="None"/> significa que contatos foram descartados e a simulação daquele frame
/// não é integral.</summary>
[Flags]
public enum PhysicsUpdateError : uint
{
    None = 0,
    ManifoldCacheFull = 1u << 0,
    BodyPairCacheFull = 1u << 1,
    ContactConstraintsFull = 1u << 2,
}

/// <summary>Capacidades independentes do mundo de física. <see cref="MaxBroadPhasePairs"/>
/// controla o buffer de pares em voo produzido pela broad phase; não é sinônimo de
/// <see cref="MaxBodyPairs"/>, que limita o cache persistente de pares de corpos.</summary>
public readonly struct PhysicsWorldConfiguration
{
    public float3 Gravity { get; }
    public uint MaxBodies { get; }
    public uint MaxBodyPairs { get; }
    public uint MaxContactConstraints { get; }
    public uint MaxBroadPhasePairs { get; }
    public PhysicsOverflowPolicy OverflowPolicy { get; }

    public PhysicsWorldConfiguration(float3 gravity, uint maxBodies, uint maxBodyPairs,
        uint maxContactConstraints, uint maxBroadPhasePairs,
        PhysicsOverflowPolicy overflowPolicy = PhysicsOverflowPolicy.BuildDefault)
    {
        if (maxBodies == 0) throw new ArgumentOutOfRangeException(nameof(maxBodies));
        if (maxBodyPairs < 4) throw new ArgumentOutOfRangeException(nameof(maxBodyPairs), "Jolt requer ao menos quatro slots de pares.");
        if (maxContactConstraints == 0) throw new ArgumentOutOfRangeException(nameof(maxContactConstraints));
        if (maxBroadPhasePairs < 1024 || maxBroadPhasePairs > int.MaxValue)
            throw new ArgumentOutOfRangeException(nameof(maxBroadPhasePairs), "O buffer da broad phase deve estar entre 1.024 e Int32.MaxValue.");
        if (overflowPolicy is < PhysicsOverflowPolicy.BuildDefault or > PhysicsOverflowPolicy.FailFast)
            throw new ArgumentOutOfRangeException(nameof(overflowPolicy));

        Gravity = gravity;
        MaxBodies = maxBodies;
        MaxBodyPairs = maxBodyPairs;
        MaxContactConstraints = maxContactConstraints;
        MaxBroadPhasePairs = maxBroadPhasePairs;
        OverflowPolicy = overflowPolicy;
    }

    /// <summary>Defaults conservadores usados também pelo símbolo nativo V1. Eles preservam
    /// compatibilidade sem confundir a capacidade de corpos com a de pares/contatos.</summary>
    public static PhysicsWorldConfiguration ForBodyCapacity(float3 gravity, uint maxBodies,
        PhysicsOverflowPolicy overflowPolicy = PhysicsOverflowPolicy.BuildDefault)
    {
        if (maxBodies == 0) throw new ArgumentOutOfRangeException(nameof(maxBodies));
        uint bodyPairs = Math.Max(1024u, SaturatingMultiply(maxBodies, 4));
        uint contacts = Math.Max(1024u, SaturatingMultiply(maxBodies, 2));
        uint broadPhasePairs = Math.Max(16384u, bodyPairs);
        if (broadPhasePairs > int.MaxValue) broadPhasePairs = int.MaxValue;
        return new PhysicsWorldConfiguration(gravity, maxBodies, bodyPairs, contacts,
            broadPhasePairs, overflowPolicy);
    }

    private static uint SaturatingMultiply(uint value, uint multiplier)
    {
        ulong result = (ulong)value * multiplier;
        return result > uint.MaxValue ? uint.MaxValue : (uint)result;
    }
}

/// <summary>Diagnóstico cumulativo de capacidade de um mundo.</summary>
public readonly record struct PhysicsStepStatistics(
    ulong TotalSteps,
    ulong OverflowSteps,
    ulong ManifoldCacheFullCount,
    ulong BodyPairCacheFullCount,
    ulong ContactConstraintsFullCount,
    PhysicsUpdateError LastErrorFlags);

/// <summary>Telemetria cumulativa apenas da fronteira de criação/destruição de corpos.
/// Um lote de 10.000 descritores conta como um crossing, com os bytes do lote inteiro.</summary>
public readonly record struct PhysicsBodyInteropStatistics(
    ulong NativeCrossings,
    ulong BytesSent,
    ulong BytesReceived,
    ulong BodiesCreated,
    ulong BodiesDestroyed);

/// <summary>
/// Wrapper gerenciado de um <c>AetherPhysicsWorld*</c> nativo. Dono do ponteiro nativo — chamar
/// <see cref="Dispose"/> (ou deixar o finalizador rodar, como rede de segurança) libera o mundo e
/// invalida todo <see cref="PhysicsBodyHandle"/> emitido por ele, na mesma disciplina do lado C++
/// (ver comentário de <c>AetherPhysics_DestroyWorld</c> em <c>jolt_bridge.h</c>).
/// <para>
/// Criação/destruição massiva usa <see cref="CreateBodies"/>/<see cref="DestroyBodies"/> e
/// atravessa a ABI uma vez por lote; as APIs unitárias são wrappers de conveniência sobre esse
/// caminho. As demais operações mantêm granularidade própria e serão agregadas pelo profiler
/// global da PoC-A.
/// </para>
/// </summary>
public sealed class PhysicsWorld : IDisposable
{
    private struct KinematicSyncState
    {
        public float3 TargetPosition;
        public quaternion TargetRotation;
        public bool NeedsStopCommand;
    }

    private nint _handle;
    private bool _disposed;
    private readonly Dictionary<uint, KinematicSyncState> _kinematicSync = new();
    private ulong _bodyInteropCrossings;
    private ulong _bodyInteropBytesSent;
    private ulong _bodyInteropBytesReceived;
    private ulong _bodiesCreated;
    private ulong _bodiesDestroyed;

    /// <summary>Quantidade de crossings nativos realmente emitidos por
    /// <see cref="MoveKinematic"/>. Útil para profiling e regressões de dirty sync.</summary>
    public ulong KinematicMoveCallCount { get; private set; }

    public PhysicsWorld(float3 gravity, uint maxBodies = 1024)
        : this(PhysicsWorldConfiguration.ForBodyCapacity(gravity, maxBodies))
    {
    }

    public PhysicsWorld(in PhysicsWorldConfiguration configuration)
    {
        var desc = new NativePhysicsWorldDescV2
        {
            StructSize = (uint)System.Runtime.InteropServices.Marshal.SizeOf<NativePhysicsWorldDescV2>(),
            ApiVersion = 2,
            Gravity = configuration.Gravity,
            MaxBodies = configuration.MaxBodies,
            MaxBodyPairs = configuration.MaxBodyPairs,
            MaxContactConstraints = configuration.MaxContactConstraints,
            MaxBroadPhasePairs = configuration.MaxBroadPhasePairs,
            OverflowPolicy = (NativePhysicsOverflowPolicy)configuration.OverflowPolicy,
        };
        _handle = NativePhysics.AetherPhysics_CreateWorldV2(in desc);
        if (_handle == nint.Zero)
            throw new InvalidOperationException("Falha ao criar o mundo de física nativo V2 (Jolt) — ver stderr [Physics] para o limite ou versão recusada.");
    }

    internal nint Handle => _disposed ? throw new ObjectDisposedException(nameof(PhysicsWorld)) : _handle;

    /// <summary>Cria um corpo. <paramref name="allowedDOFs"/> default (<see cref="AllowedDOFs.All"/>)
    /// é um corpo 3D normal, sem restrição — passe <see cref="AllowedDOFs.Plane2D"/> (ou uma
    /// combinação customizada) para um corpo de física 2D (item 4.1.6). Devolve
    /// <see cref="PhysicsBodyHandle.Invalid"/> também se <paramref name="motionType"/> for
    /// <see cref="NativeMotionType.Dynamic"/> e <paramref name="allowedDOFs"/> não deixar
    /// nenhum eixo de translação livre — ver <see cref="AllowedDOFs"/>.</summary>
    public PhysicsBodyHandle CreateBody(in PhysicsShape shape, float3 position, quaternion rotation,
        NativeMotionType motionType, float friction = 0.5f, float restitution = 0.0f,
        AllowedDOFs allowedDOFs = AllowedDOFs.All, bool isSensor = false,
        QueryLayerMask eventLayerMask = QueryLayerMask.All)
    {
        var desc = new PhysicsBodyDescription(shape, position, rotation, motionType, friction,
            restitution, allowedDOFs, isSensor, eventLayerMask);
        Span<PhysicsBodyHandle> result = stackalloc PhysicsBodyHandle[1];
        return CreateBodies(new ReadOnlySpan<PhysicsBodyDescription>(in desc), result) == 1
            ? result[0]
            : PhysicsBodyHandle.Invalid;
    }

    public void DestroyBody(PhysicsBodyHandle handle)
    {
        if (!handle.IsValid) return;
        ReadOnlySpan<PhysicsBodyHandle> one = new(in handle);
        DestroyBodies(one);
    }

    /// <summary>Cria todos os descritores com um único crossing. A operação nativa é
    /// transacional: sucesso devolve <c>descriptions.Length</c>; falha devolve zero e preenche
    /// todo o prefixo correspondente com <see cref="PhysicsBodyHandle.Invalid"/>.</summary>
    public unsafe int CreateBodies(ReadOnlySpan<PhysicsBodyDescription> descriptions,
        Span<PhysicsBodyHandle> results)
    {
        if (results.Length < descriptions.Length)
            throw new ArgumentException("O buffer de handles precisa comportar todos os descritores.", nameof(results));
        if (descriptions.IsEmpty) return 0;

        int created;
        fixed (PhysicsBodyDescription* descPtr = descriptions)
        fixed (PhysicsBodyHandle* resultPtr = results)
            created = NativePhysics.AetherPhysics_CreateBodiesV2(Handle, descPtr, resultPtr,
                descriptions.Length);

        _bodyInteropCrossings++;
        _bodyInteropBytesSent += (ulong)descriptions.Length * (uint)sizeof(PhysicsBodyDescription);
        _bodyInteropBytesReceived += (ulong)descriptions.Length * (uint)sizeof(PhysicsBodyHandle);
        _bodiesCreated += (ulong)created;

        if (created == descriptions.Length)
            for (int i = 0; i < created; i++)
                if (descriptions[i].MotionType == NativeMotionType.Kinematic)
                    _kinematicSync[results[i].Value] = new KinematicSyncState
                    {
                        TargetPosition = descriptions[i].Position,
                        TargetRotation = descriptions[i].Rotation,
                    };
        return created;
    }

    /// <summary>Remove todos os handles com um único crossing. Handles inválidos são aceitos e
    /// ignorados, permitindo destruir diretamente buffers parcialmente preenchidos.</summary>
    public unsafe void DestroyBodies(ReadOnlySpan<PhysicsBodyHandle> handles)
    {
        if (handles.IsEmpty) return;
        fixed (PhysicsBodyHandle* p = handles)
            NativePhysics.AetherPhysics_DestroyBodies(Handle, p, handles.Length);
        _bodyInteropCrossings++;
        _bodyInteropBytesSent += (ulong)handles.Length * (uint)sizeof(PhysicsBodyHandle);
        for (int i = 0; i < handles.Length; i++)
            if (handles[i].IsValid)
            {
                _kinematicSync.Remove(handles[i].Value);
                _bodiesDestroyed++;
            }
    }

    public PhysicsBodyInteropStatistics GetBodyInteropStatistics() => new(
        _bodyInteropCrossings, _bodyInteropBytesSent, _bodyInteropBytesReceived,
        _bodiesCreated, _bodiesDestroyed);

    public void ResetBodyInteropStatistics()
    {
        _bodyInteropCrossings = 0;
        _bodyInteropBytesSent = 0;
        _bodyInteropBytesReceived = 0;
        _bodiesCreated = 0;
        _bodiesDestroyed = 0;
    }

    public PhysicsUpdateError Step(float deltaTime, int collisionSteps = 1) =>
        (PhysicsUpdateError)NativePhysics.AetherPhysics_StepV2(Handle, deltaTime, collisionSteps);

    /// <summary>Copia a fotografia ordenada dos eventos produzidos pelo último <see cref="Step"/>.
    /// Devolve a contagem real: se for maior que <paramref name="events"/>.Length, o prefixo
    /// coube no buffer e o chamador pode repetir com capacidade suficiente. Ler não consome a
    /// fotografia; o próximo Step a substitui.</summary>
    public unsafe int GetTriggerEvents(Span<PhysicsTriggerEvent> events)
    {
        fixed (PhysicsTriggerEvent* p = events)
            return NativePhysics.AetherPhysics_GetTriggerEvents(Handle, p, events.Length);
    }

    public PhysicsStepStatistics GetStepStatistics()
    {
        var stats = new NativePhysicsStepStatsV2
        {
            StructSize = (uint)System.Runtime.InteropServices.Marshal.SizeOf<NativePhysicsStepStatsV2>(),
            ApiVersion = 2,
        };
        if (NativePhysics.AetherPhysics_GetStepStatsV2(Handle, ref stats) == 0)
            throw new InvalidOperationException("A biblioteca nativa recusou o descritor de estatísticas V2.");

        return new PhysicsStepStatistics(stats.TotalSteps, stats.OverflowSteps,
            stats.ManifoldCacheFullCount, stats.BodyPairCacheFullCount,
            stats.ContactConstraintsFullCount, (PhysicsUpdateError)stats.LastErrorFlags);
    }

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

    /// <summary>Agenda um alvo cinemático para o próximo Step. Alvos idênticos não atravessam
    /// a ABI a cada frame. Depois de um movimento, um único comando idêntico adicional zera a
    /// velocidade calculada pelo Jolt; os frames estáveis seguintes não fazem crossing.</summary>
    public bool MoveKinematic(PhysicsBodyHandle handle, float3 targetPosition,
        quaternion targetRotation, float deltaTime)
    {
        if (!handle.IsValid) return false;
        if (deltaTime <= 0f) throw new ArgumentOutOfRangeException(nameof(deltaTime));
        if (!_kinematicSync.TryGetValue(handle.Value, out var state)) return false;

        bool targetChanged = !state.TargetPosition.Equals(targetPosition) ||
                             !state.TargetRotation.Equals(targetRotation);
        if (!targetChanged && !state.NeedsStopCommand) return false;

        if (NativePhysics.AetherPhysics_MoveKinematicV2(Handle, handle.Value, targetPosition,
                targetRotation, deltaTime) == 0)
            return false;

        state.TargetPosition = targetPosition;
        state.TargetRotation = targetRotation;
        state.NeedsStopCommand = targetChanged;
        _kinematicSync[handle.Value] = state;
        KinematicMoveCallCount++;
        return true;
    }

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

    // ---------------------------------------------------------------- queries (4.1.4)

    /// <summary>
    /// Raycast que acerta TODOS os corpos ao longo do segmento, ordenados do mais próximo ao
    /// mais distante — ao contrário de <see cref="RayCastClosest"/>, que para no primeiro. Os
    /// resultados são escritos em <paramref name="results"/> (buffer fornecido pelo chamador,
    /// zero alocação do lado desta chamada); o valor de retorno é a contagem REAL de hits, que
    /// pode ser maior que <c>results.Length</c> — o excesso simplesmente não cabe no buffer
    /// (mesma convenção de <c>Span&lt;T&gt;.CopyTo</c> truncando, não de exceção). Chame de novo com
    /// um buffer maior se o retorno exceder o tamanho passado.
    /// </summary>
    public unsafe int RayCastAll(float3 origin, float3 direction, Span<PhysicsBodyHandle> resultBodies,
        Span<float> resultFractions, QueryLayerMask layerMask = QueryLayerMask.All, PhysicsBodyHandle? ignoreBody = null)
    {
        // PhysicsBodyHandle.Invalid não é `const` (struct construído em runtime), então não dá
        // para usá-lo como default de parâmetro opcional direto — e default(PhysicsBodyHandle)
        // (Value=0) colidiria com um corpo real de índice 0, então NÃO pode significar "nenhum".
        // PhysicsBodyHandle? com default null resolve isso sem ambiguidade: "não passei nada" é
        // literalmente null, distinto de qualquer handle (válido ou Invalid) que o chamador possa
        // passar explicitamente.
        uint ignoreRaw = ignoreBody?.Value ?? PhysicsBodyHandle.Invalid.Value;
        int maxResults = Math.Min(resultBodies.Length, resultFractions.Length);
        Span<uint> rawBodies = maxResults <= 64 ? stackalloc uint[maxResults] : new uint[maxResults];
        fixed (uint* bodiesPtr = rawBodies)
        fixed (float* fractionsPtr = resultFractions)
        {
            int count = NativePhysics.AetherPhysics_RayCastAll(Handle, origin, direction,
                (NativeQueryLayerMask)layerMask, ignoreRaw, bodiesPtr, fractionsPtr, maxResults);
            int copied = Math.Min(count, maxResults);
            for (int i = 0; i < copied; i++) resultBodies[i] = new PhysicsBodyHandle(rawBodies[i]);
            return count;
        }
    }

    /// <summary>
    /// Varre <paramref name="shape"/> de <paramref name="origin"/> ao longo de
    /// <paramref name="direction"/> (comprimento = alcance, mesma convenção de
    /// <see cref="RayCastClosest"/>) e devolve o hit mais próximo ao longo do caminho — "e se eu
    /// movesse esta forma nesta direção, o que ela tocaria primeiro?". <paramref name="rotation"/>
    /// é fixa durante toda a varredura (o Jolt não modela rotação progressiva num shapecast).
    /// </summary>
    public unsafe bool ShapeCastClosest(in PhysicsShape shape, float3 origin, quaternion rotation, float3 direction,
        out ShapeQueryHit hit, QueryLayerMask layerMask = QueryLayerMask.All, PhysicsBodyHandle? ignoreBody = null)
    {
        NativeShapeDesc nativeShape = shape.ToNative();
        uint ignoreRaw = ignoreBody?.Value ?? PhysicsBodyHandle.Invalid.Value;
        NativeShapeQueryHit nativeHit = default;
        int found = NativePhysics.AetherPhysics_ShapeCastClosest(Handle, in nativeShape, origin, rotation, direction,
            (NativeQueryLayerMask)layerMask, ignoreRaw, &nativeHit);
        hit = new ShapeQueryHit(in nativeHit);
        return found != 0;
    }

    /// <summary>
    /// Quais corpos sobrepõem <paramref name="shape"/> PARADA em <paramref name="origin"/>/
    /// <paramref name="rotation"/> — o "trigger volume" mais comum em jogos (zona de detecção,
    /// raio de explosão, área de efeito). Mesma convenção de buffer/contagem-real de
    /// <see cref="RayCastAll"/>.
    /// </summary>
    public unsafe int OverlapShape(in PhysicsShape shape, float3 origin, quaternion rotation,
        Span<ShapeQueryHit> results, QueryLayerMask layerMask = QueryLayerMask.All, PhysicsBodyHandle? ignoreBody = null)
    {
        NativeShapeDesc nativeShape = shape.ToNative();
        uint ignoreRaw = ignoreBody?.Value ?? PhysicsBodyHandle.Invalid.Value;
        int maxResults = results.Length;
        Span<NativeShapeQueryHit> rawHits = maxResults <= 16 ? stackalloc NativeShapeQueryHit[maxResults] : new NativeShapeQueryHit[maxResults];
        fixed (NativeShapeQueryHit* hitsPtr = rawHits)
        {
            int count = NativePhysics.AetherPhysics_OverlapShape(Handle, in nativeShape, origin, rotation,
                (NativeQueryLayerMask)layerMask, ignoreRaw, hitsPtr, maxResults);
            int copied = Math.Min(count, maxResults);
            for (int i = 0; i < copied; i++) results[i] = new ShapeQueryHit(in rawHits[i]);
            return count;
        }
    }

    // ---------------------------------------------------------------- juntas e motores (4.1.3)

    /// <summary>Cria uma junta entre dois corpos deste mundo. Devolve <see cref="PhysicsJointHandle.Invalid"/>
    /// se algum handle, enum, eixo, valor finito ou limite for inválido — mesma disciplina
    /// defensiva do ABI (nunca deixa um descriptor ruim alcançar um assert do Jolt).</summary>
    public PhysicsJointHandle CreateJoint(PhysicsBodyHandle body1, PhysicsBodyHandle body2, in JointDesc desc)
    {
        if (!body1.IsValid || !body2.IsValid || !desc.IsValid()) return PhysicsJointHandle.Invalid;
        NativeJointDescV2 native = desc.ToNativeV2();
        uint raw = NativePhysics.AetherPhysics_CreateJointV2(Handle, body1.Value, body2.Value, in native);
        return new PhysicsJointHandle(raw);
    }

    public void DestroyJoint(PhysicsJointHandle handle)
    {
        if (!handle.IsValid) return;
        DestroySynchronizedJoint(handle);
        JointSyncSystem.NotifyJointDestroyed(this, handle);
    }

    /// <summary>Caminho do owner declarativo: o <see cref="JointSyncSystem"/> já atualiza seu
    /// mapa antes/depois da chamada, portanto não deve receber uma notificação reentrante.</summary>
    internal void DestroySynchronizedJoint(PhysicsJointHandle handle)
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
        JointSyncSystem.ForgetWorld(this);
        NativePhysics.AetherPhysics_DestroyWorld(_handle);
        _kinematicSync.Clear();
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

    // ---------------------------------------------------------------- character controller (4.1.5)

    /// <summary>Cria um character controller, SEMPRE em pé (StandingHalfHeight) — não há como
    /// nascer já agachado, chame <see cref="SetCharacterCrouching"/> depois se necessário.
    /// AVISO: nascer num espaço apertado demais para a forma de pé faz a resolução de
    /// penetração da criação empurrar o personagem para uma posição inesperada — ver o
    /// comentário completo em jolt_bridge.h (mesmo aviso, mesma causa).</summary>
    public PhysicsCharacterHandle CreateCharacter(in CharacterDesc desc, float3 position, quaternion rotation)
    {
        NativeCharacterDesc native = desc.ToNative();
        uint raw = NativePhysics.AetherPhysics_CreateCharacter(Handle, in native, position, rotation);
        return new PhysicsCharacterHandle(raw);
    }

    public void DestroyCharacter(PhysicsCharacterHandle handle)
    {
        if (!handle.IsValid) return;
        NativePhysics.AetherPhysics_DestroyCharacter(Handle, handle.Value);
    }

    /// <summary>API de baixo nível que define a velocidade linear antes de
    /// <see cref="UpdateCharacter"/>. Runtime comum deve preferir
    /// <see cref="CharacterMotorSystem"/>, que compõe gravidade e plataforma uma
    /// única vez; use este método diretamente apenas para controle especializado.</summary>
    public void SetCharacterVelocity(PhysicsCharacterHandle handle, float3 velocity)
    {
        if (!handle.IsValid) return;
        NativePhysics.AetherPhysics_SetCharacterVelocity(Handle, handle.Value, velocity);
    }

    public float3 GetCharacterVelocity(PhysicsCharacterHandle handle) =>
        handle.IsValid ? NativePhysics.AetherPhysics_GetCharacterVelocity(Handle, handle.Value) : float3.Zero;

    /// <summary>Avança a simulação do personagem em <paramref name="deltaTime"/>, com suporte a
    /// degraus (<c>JPH::CharacterVirtual::ExtendedUpdate</c>). <paramref name="gravity"/> é usada
    /// só internamente pelo Jolt para empurrar objetos abaixo do personagem — NÃO integra a
    /// velocidade vertical do próprio personagem (já deveria ter sido somada antes de
    /// <see cref="SetCharacterVelocity"/>).</summary>
    public void UpdateCharacter(PhysicsCharacterHandle handle, float deltaTime, float3 gravity,
        QueryLayerMask layerMask = QueryLayerMask.All, PhysicsBodyHandle? ignoreBody = null)
    {
        if (!handle.IsValid) return;
        uint ignoreRaw = ignoreBody?.Value ?? PhysicsBodyHandle.Invalid.Value;
        NativePhysics.AetherPhysics_UpdateCharacter(Handle, handle.Value, deltaTime, gravity, (NativeQueryLayerMask)layerMask, ignoreRaw);
    }

    public unsafe void GetCharacterTransform(PhysicsCharacterHandle handle, out float3 position, out quaternion rotation)
    {
        position = default;
        rotation = quaternion.Identity;
        if (!handle.IsValid) return;
        fixed (float3* p = &position)
        fixed (quaternion* r = &rotation)
        {
            NativePhysics.AetherPhysics_GetCharacterTransform(Handle, handle.Value, p, r);
        }
    }

    public CharacterGroundState GetCharacterGroundState(PhysicsCharacterHandle handle) =>
        handle.IsValid ? (CharacterGroundState)NativePhysics.AetherPhysics_GetCharacterGroundState(Handle, handle.Value) : CharacterGroundState.InAir;

    /// <summary>Velocidade do corpo/superfície sob o personagem (0 se InAir ou handle inválido)
    /// — já inclui rotação do corpo de suporte. <see cref="CharacterMotorSystem"/>
    /// a herda automaticamente; consumidores de baixo nível podem compor manualmente.</summary>
    public float3 GetCharacterGroundVelocity(PhysicsCharacterHandle handle) =>
        handle.IsValid ? NativePhysics.AetherPhysics_GetCharacterGroundVelocity(Handle, handle.Value) : float3.Zero;

    /// <summary>Normal da superfície de contato (chão ou rampa) — útil para decidir a direção
    /// de deslizamento quando <see cref="GetCharacterGroundState"/> é <see cref="CharacterGroundState.OnSteepGround"/>.</summary>
    public float3 GetCharacterGroundNormal(PhysicsCharacterHandle handle) =>
        handle.IsValid ? NativePhysics.AetherPhysics_GetCharacterGroundNormal(Handle, handle.Value) : float3.Zero;

    /// <summary>Troca entre a cápsula de pé/agachada de <see cref="CharacterDesc"/>, checando
    /// primeiro se há espaço livre para a forma nova. Devolve <c>false</c> se não havia espaço
    /// (ex.: tentando ficar de pé debaixo de algo baixo) — nesse caso a forma permanece a
    /// anterior, sem efeito colateral.</summary>
    public bool SetCharacterCrouching(PhysicsCharacterHandle handle, bool crouching)
    {
        if (!handle.IsValid) return false;
        return NativePhysics.AetherPhysics_SetCharacterCrouching(Handle, handle.Value, crouching ? 1 : 0) != 0;
    }
}
