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
