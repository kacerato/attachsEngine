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
