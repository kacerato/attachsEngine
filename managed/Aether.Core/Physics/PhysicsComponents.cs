namespace Aether.Physics;

/// <summary>
/// Componente que marca uma entidade como corpo físico simulado pelo Jolt. Junto com
/// <see cref="Collider"/> (obrigatório) descreve tudo que <see cref="PhysicsSyncSystem"/> precisa
/// para criar o corpo nativo — a entidade em si não sabe que existe um <c>AetherPhysicsWorld*</c>
/// do outro lado, só carrega dados; toda a sincronização é responsabilidade do sistema.
/// <para>
/// Todo componente aqui é <c>unmanaged</c>/blittable (requisito do ECS, ver <c>World.cs</c>) —
/// <see cref="Handle"/> guarda só o <see cref="PhysicsBodyHandle"/> (um uint), nunca um ponteiro
/// de mundo ou objeto gerenciado.
/// </para>
/// </summary>
public struct RigidBody
{
    public NativeMotionType MotionType;
    public float Friction;
    public float Restitution;

    /// <summary>Preenchido por <see cref="PhysicsSyncSystem"/> na primeira sincronização; inválido
    /// antes disso. Não escreva aqui fora do sistema.</summary>
    public PhysicsBodyHandle Handle;

    public RigidBody(NativeMotionType motionType, float friction = 0.5f, float restitution = 0.0f)
    {
        MotionType = motionType;
        Friction = friction;
        Restitution = restitution;
        Handle = PhysicsBodyHandle.Invalid;
    }

    public static RigidBody Static(float friction = 0.5f, float restitution = 0.0f) => new(NativeMotionType.Static, friction, restitution);
    public static RigidBody Kinematic(float friction = 0.5f, float restitution = 0.0f) => new(NativeMotionType.Kinematic, friction, restitution);
    public static RigidBody Dynamic(float friction = 0.5f, float restitution = 0.0f) => new(NativeMotionType.Dynamic, friction, restitution);
}

/// <summary>
/// Forma de colisão de uma entidade com <see cref="RigidBody"/>. Componente separado (não
/// embutido em <see cref="RigidBody"/>) espelhando a separação corpo/forma do próprio Jolt.
/// </summary>
public struct Collider
{
    public NativeShapeKind Kind;
    public float3 BoxHalfExtent;
    public float SphereRadius;

    private Collider(NativeShapeKind kind, float3 boxHalfExtent, float sphereRadius)
    { Kind = kind; BoxHalfExtent = boxHalfExtent; SphereRadius = sphereRadius; }

    public static Collider Box(float3 halfExtent) => new(NativeShapeKind.Box, halfExtent, 0f);
    public static Collider Sphere(float radius) => new(NativeShapeKind.Sphere, default, radius);

    internal PhysicsShape ToShape() => Kind == NativeShapeKind.Box ? PhysicsShape.Box(BoxHalfExtent) : PhysicsShape.Sphere(SphereRadius);
}

/// <summary>Transforma o <see cref="Collider"/> da entidade num sensor persistente: participa
/// da broad/narrow phase, mas não produz resposta física. O filtro escolhe quais categorias do
/// outro corpo geram eventos; ele não desliga a detecção interna do Jolt.</summary>
/// <remarks>O componente deve estar presente antes da primeira chamada a
/// <see cref="PhysicsSyncSystem.Step"/>. Para mudar sensor/filtro em runtime, destrua e recrie o
/// corpo via <see cref="PhysicsSyncSystem.DestroyBody"/>; a próxima sincronização aplica os novos
/// dados sem conservar estado nativo incompatível.</remarks>
public struct Trigger
{
    public bool Enabled;

    // uint, e não enum, mantém o componente serializável pelo metadata registry atual.
    // A propriedade tipada abaixo é a API de uso; o campo é o formato persistido estável.
    public uint EventLayerMaskBits;

    public QueryLayerMask EventLayerMask
    {
        readonly get => (QueryLayerMask)EventLayerMaskBits;
        set => EventLayerMaskBits = (uint)value;
    }

    public Trigger(QueryLayerMask eventLayerMask = QueryLayerMask.All, bool enabled = true)
    {
        if ((eventLayerMask & ~QueryLayerMask.All) != 0)
            throw new ArgumentOutOfRangeException(nameof(eventLayerMask));
        Enabled = enabled;
        EventLayerMaskBits = (uint)eventLayerMask;
    }
}

/// <summary>Descrição declarativa de uma junta entre duas entidades que possuem
/// <see cref="RigidBody"/>. O componente não guarda handle nativo: esse estado transitório
/// pertence ao <see cref="JointSyncSystem"/> e portanto nunca é persistido em cena.
/// Referências ausentes são aceitas como estado de autoria e resolvidas tardiamente quando os
/// dois corpos passam a existir.</summary>
public struct Joint : IEquatable<Joint>
{
    public EntityId Body1;
    public EntityId Body2;

    // Bits primitivos mantêm o componente compatível com o metadata registry atual, que ainda
    // não serializa enums diretamente. As propriedades tipadas são a API pública de uso.
    public uint KindBits;
    public uint SpaceBits;
    public float3 Point1;
    public float3 Point2;
    public float3 Axis1;
    public float3 Axis2;
    public float LimitsMin;
    public float LimitsMax;
    public uint MotorStateBits;
    public float MotorTargetVelocity;
    public float MotorTargetPosition;
    public float MotorMaxForceOrTorque;
    public float MotorSpringFrequency;
    public float MotorSpringDamping;

    public JointKind Kind
    {
        readonly get => (JointKind)KindBits;
        set
        {
            if (value is < JointKind.Point or > JointKind.Distance)
                throw new ArgumentOutOfRangeException(nameof(value));
            KindBits = (uint)value;
        }
    }

    public JointSpace Space
    {
        readonly get => (JointSpace)SpaceBits;
        set
        {
            if (value is < JointSpace.World or > JointSpace.LocalToBody2)
                throw new ArgumentOutOfRangeException(nameof(value));
            SpaceBits = (uint)value;
        }
    }

    public JointMotor Motor
    {
        readonly get => new()
        {
            State = (NativeMotorState)MotorStateBits,
            TargetVelocity = MotorTargetVelocity,
            TargetPosition = MotorTargetPosition,
            MaxForceOrTorque = MotorMaxForceOrTorque,
            SpringFrequency = MotorSpringFrequency,
            SpringDamping = MotorSpringDamping,
        };
        set
        {
            if (value.State is < NativeMotorState.Off or > NativeMotorState.PositionAndVelocity)
                throw new ArgumentOutOfRangeException(nameof(value));
            MotorStateBits = (uint)value.State;
            MotorTargetVelocity = value.TargetVelocity;
            MotorTargetPosition = value.TargetPosition;
            MotorMaxForceOrTorque = value.MaxForceOrTorque;
            MotorSpringFrequency = value.SpringFrequency;
            MotorSpringDamping = value.SpringDamping;
        }
    }

    public Joint(EntityId body1, EntityId body2, JointKind kind = JointKind.Point,
        JointSpace space = JointSpace.World)
    {
        this = default;
        Body1 = body1;
        Body2 = body2;
        Kind = kind;
        Space = space;
    }

    internal readonly bool TryGetDescription(out JointDesc description)
    {
        description = new JointDesc
        {
            Kind = (JointKind)KindBits,
            Space = (JointSpace)SpaceBits,
            Point1 = Point1,
            Point2 = Point2,
            Axis1 = Axis1,
            Axis2 = Axis2,
            LimitsMin = LimitsMin,
            LimitsMax = LimitsMax,
            Motor = Motor,
        };
        return description.IsValid();
    }

    public readonly bool Equals(Joint other) =>
        Body1 == other.Body1 && Body2 == other.Body2 &&
        KindBits == other.KindBits && SpaceBits == other.SpaceBits &&
        Point1.Equals(other.Point1) && Point2.Equals(other.Point2) &&
        Axis1.Equals(other.Axis1) && Axis2.Equals(other.Axis2) &&
        LimitsMin.Equals(other.LimitsMin) && LimitsMax.Equals(other.LimitsMax) &&
        MotorStateBits == other.MotorStateBits &&
        MotorTargetVelocity.Equals(other.MotorTargetVelocity) &&
        MotorTargetPosition.Equals(other.MotorTargetPosition) &&
        MotorMaxForceOrTorque.Equals(other.MotorMaxForceOrTorque) &&
        MotorSpringFrequency.Equals(other.MotorSpringFrequency) &&
        MotorSpringDamping.Equals(other.MotorSpringDamping);

    public override readonly bool Equals(object? obj) => obj is Joint other && Equals(other);
    public override readonly int GetHashCode() => HashCode.Combine(Body1, Body2, KindBits, SpaceBits);
    public static bool operator ==(Joint left, Joint right) => left.Equals(right);
    public static bool operator !=(Joint left, Joint right) => !left.Equals(right);
}
