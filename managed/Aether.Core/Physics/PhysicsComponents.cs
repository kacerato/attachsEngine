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
/// <para>
/// <c>Trigger</c> (volume de overlap sem resposta física, citado no plano junto com
/// RigidBody/Collider) NÃO está implementado nesta fatia — deliberadamente, não por descuido: um
/// trigger de verdade exige que o corpo nativo seja criado como "sensor"
/// (<c>JPH::BodyCreationSettings::mIsSensor</c>), flag que a fronteira C ABI atual
/// (<c>native/physics/jolt_bridge.h</c>, item 4.1.1) não expõe — adicioná-la seria estender o
/// lado nativo, fora do escopo de "fachada C#" deste item. E mesmo com a flag, não haveria como
/// consumir o resultado: eventos de entrada/saída de overlap são o item 4.1.4 ("queries expostas
/// a script/nós"), ainda não implementado. Ver <c>docs/ESTADO.md</c>.
/// </para>
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
