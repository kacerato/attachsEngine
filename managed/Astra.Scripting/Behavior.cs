using System.Numerics;

namespace Astra;

[AttributeUsage(AttributeTargets.Class, Inherited = false)]
public sealed class ComponentIdAttribute(string id) : Attribute
{
    public string Id { get; } = id;
}

[AttributeUsage(AttributeTargets.Field | AttributeTargets.Property, Inherited = true)]
public sealed class PropertyIdAttribute(string id) : Attribute
{
    public string Id { get; } = id;
}

public readonly record struct ObjectReference(ulong ObjectId);
public readonly record struct AssetReference(string AssetId);
public readonly record struct TransformValue(Vector3 Position, Quaternion Rotation, Vector3 Scale);

/// <summary>
/// A superfície de baixo nível do mundo de execução: uma tradução direta da ABI
/// nativa v8 (native/scene/script_runtime.h). O documento autoral nunca cruza
/// esta API.
///
/// Os comportamentos do projeto usam <see cref="GameObject"/> e
/// <see cref="Component"/>, que embrulham isto com identidade verificada; estes
/// membros existem para quem precisa do id cru e para os testes.
///
/// Membros com implementação padrão que lança existiam antes da v3 ou podem não
/// estar disponíveis num hospedeiro mais velho: um duplo de teste só precisa
/// implementar o que usa.
/// </summary>
public interface ISceneAccess
{
    bool Exists(ulong objectId);
    TransformValue GetTransform(ulong objectId);
    bool SetTransform(ulong objectId, TransformValue value);
    bool SetBodyVelocity(ulong objectId, Vector3 velocity);
    bool MoveKinematic(ulong objectId, Vector3 position, Quaternion rotation);
    bool AddForce(ulong objectId, Vector3 force) => throw new NotSupportedException();
    bool AddImpulse(ulong objectId, Vector3 impulse) => throw new NotSupportedException();
    bool AddTorque(ulong objectId, Vector3 torque) => throw new NotSupportedException();
    bool AddAngularImpulse(ulong objectId, Vector3 impulse) => throw new NotSupportedException();
    Vector3 GetBodyVelocity(ulong objectId) => throw new NotSupportedException();
    void Log(ulong objectId, string message);

    // --- v3: identidade -----------------------------------------------------
    /// <summary>Identidade desta sessão de Play; muda a cada Play.</summary>
    uint WorldId => throw new NotSupportedException();
    /// <summary>Zero quando o objeto já foi destruído.</summary>
    uint GenerationOf(ulong objectId) => throw new NotSupportedException();
    /// <summary>Motivo da última recusa nesta thread.</summary>
    WorldStatus LastStatus => WorldStatus.Rejected;

    // --- v3: hierarquia -----------------------------------------------------
    ulong ParentOf(ulong objectId) => throw new NotSupportedException();
    int ChildCount(ulong objectId) => throw new NotSupportedException();
    ulong ChildAt(ulong objectId, uint index) => throw new NotSupportedException();
    ulong FindChild(ulong objectId, string name, bool recursive) => throw new NotSupportedException();
    string GetName(ulong objectId) => throw new NotSupportedException();
    bool SetName(ulong objectId, string name) => throw new NotSupportedException();
    /// <summary>1 ativo, 0 inativo, -1 objeto indisponível.</summary>
    int GetActive(ulong objectId) => throw new NotSupportedException();
    bool SetActive(ulong objectId, bool active) => throw new NotSupportedException();

    // --- v3: ciclo de vida --------------------------------------------------
    ulong CreateObject(ulong parent, string name) => throw new NotSupportedException();
    bool DestroyObject(ulong objectId) => throw new NotSupportedException();
    bool SetParent(ulong objectId, ulong parent, uint childIndex) => throw new NotSupportedException();

    // --- v3: componentes ----------------------------------------------------
    int ComponentCount(ulong objectId) => throw new NotSupportedException();
    (ulong Instance, string TypeId) ComponentAt(ulong objectId, uint index) => throw new NotSupportedException();
    ulong FindComponent(ulong objectId, string typeId, uint ordinal) => throw new NotSupportedException();
    ulong AddComponent(ulong objectId, string typeId) => throw new NotSupportedException();
    bool RemoveComponent(ulong objectId, ulong instanceId) => throw new NotSupportedException();
    bool TryGetProperty(ulong objectId, ulong instanceId, string propertyId, out uint kind, out ulong bits)
        => throw new NotSupportedException();
    bool SetProperty(ulong objectId, ulong instanceId, string propertyId, uint kind, ulong bits)
        => throw new NotSupportedException();

    // --- v3: transform de mundo --------------------------------------------
    TransformValue GetWorldTransform(ulong objectId) => throw new NotSupportedException();
    bool SetWorldTransform(ulong objectId, TransformValue value) => throw new NotSupportedException();

    // --- v4: consultas físicas ----------------------------------------------
    /// <summary>Contagem REAL de acertos, que pode exceder o buffer recebido.</summary>
    int RayCast(Vector3 origin, Vector3 direction, in QueryFilter filter, Span<RawQueryHit> results)
        => throw new NotSupportedException();
    int ShapeCast(in ShapeQuery shape, Vector3 origin, Vector3 direction, in QueryFilter filter, out RawQueryHit hit)
        => throw new NotSupportedException();
    int Overlap(in ShapeQuery shape, Vector3 origin, in QueryFilter filter, Span<RawQueryHit> results)
        => throw new NotSupportedException();
    int LayerByName(string name) => throw new NotSupportedException();
    string LayerName(uint layer) => throw new NotSupportedException();

    // --- v5: entrada por ações -----------------------------------------------
    bool InputAxis(string action, out Vector2 value) => throw new NotSupportedException();
    /// <summary>0 pressionado agora, 1 acabou de descer, 2 acabou de subir; -1 ação desconhecida.</summary>
    int InputButton(string action, uint query) => throw new NotSupportedException();
    bool InputContext(string context, int enabled) => throw new NotSupportedException();
    /// <summary>Nome da ação que cumpre o papel: 0 mover, 1 olhar, 2 saltar.</summary>
    string InputRole(uint role) => throw new NotSupportedException();

    // --- v6: gráficos globais e recursos tipados --------------------------
    GraphicsSnapshot GetGraphicsState(uint expectedWorld) => throw new NotSupportedException();
    bool SetGraphicsSettings(uint expectedWorld, GraphicsSettings settings, out ulong requestId)
        => throw new NotSupportedException();
    bool TryGetResource(ulong objectId, ulong instanceId, string propertyId, uint slot, out AssetGuid value)
        => throw new NotSupportedException();
    bool SetResource(ulong objectId, ulong instanceId, string propertyId, uint slot, AssetGuid value)
        => throw new NotSupportedException();
    // --- v7: propriedades tipadas endereçadas por slot --------------------
    bool TryGetSlotProperty(ulong objectId, ulong instanceId, string propertyId, uint slot,
        out uint kind, out ulong bits) => throw new NotSupportedException();
    bool SetSlotProperty(ulong objectId, ulong instanceId, string propertyId, uint slot, uint kind, ulong bits)
        => throw new NotSupportedException();

    // --- v8: comandos dos consumidores de personagem/câmera -------------
    bool CharacterMove(ulong objectId, Vector2 input, float yawRadians) => throw new NotSupportedException();
    bool CharacterJump(ulong objectId) => throw new NotSupportedException();
    bool CameraLook(ulong objectId, Vector2 normalizedDelta) => throw new NotSupportedException();

    // --- v9: animação ---------------------------------------------------------
    bool AnimationCommand(ulong objectId, ulong instanceId, AnimationCommandKind op, AssetGuid clip,
                          float seconds, float targetWeight, AnimationPlayMode mode) => throw new NotSupportedException();
    bool TryGetAnimationState(ulong objectId, ulong instanceId, AssetGuid clip, out AnimationStateValue value)
        => throw new NotSupportedException();
    bool SetAnimationState(ulong objectId, ulong instanceId, in AnimationStateValue value) => throw new NotSupportedException();
    /// <summary>Quantos clipes o componente lista (-1 sem componente); com índice válido, o clipe e o nome.</summary>
    int AnimationClipAt(ulong objectId, ulong instanceId, uint index, out AssetGuid clip, out string name)
        => throw new NotSupportedException();
}

/// <summary>
/// Como um comportamento encontra OUTRO comportamento por capacidade.
///
/// É o que torna "interação" um contrato do projeto em vez de um tipo especial
/// do núcleo: a porta implementa uma interface, e quem interage procura por essa
/// interface, sem saber que existe uma porta.
/// </summary>
public interface IBehaviorRegistry
{
    /// <summary>Primeiro comportamento desse objeto atribuível ao tipo pedido.</summary>
    object? FindBehavior(ulong objectId, Type contract);
    /// <summary>Todos os comportamentos desse objeto atribuíveis ao tipo pedido.</summary>
    IEnumerable<object> FindBehaviors(ulong objectId, Type contract);
}

public abstract class Behavior
{
    private ISceneAccess? _scene;
    private IBehaviorRegistry? _registry;
    public ulong ObjectId { get; private set; }
    public ulong InstanceId { get; private set; }
    public bool Enabled { get; set; } = true;
    protected ISceneAccess Scene => _scene ?? throw new InvalidOperationException("Behavior is not attached to an execution world.");

    private GameObject? _object;

    /// <summary>
    /// O objeto a que este comportamento está anexado. Resolvido uma vez: um
    /// `Object` novo a cada quadro seria lixo alocado por comportamento e por
    /// quadro, e o handle não muda enquanto o comportamento existe.
    /// </summary>
    protected GameObject Object => _object ??= GameObject.Resolve(Scene, ObjectId);

    /// <summary>As consultas físicas do mundo de execução.</summary>
    protected PhysicsAccess Physics => new(Scene);

    /// <summary>As ações de entrada configuradas no projeto.</summary>
    protected InputAccess Input => new(Scene);

    /// <summary>Resolve uma referência autorada no inspetor para um objeto vivo.</summary>
    protected GameObject? Resolve(ObjectReference reference)
    {
        if (reference.ObjectId == 0 || !Scene.Exists(reference.ObjectId)) return null;
        return GameObject.Resolve(Scene, reference.ObjectId);
    }

    protected TransformValue Transform
    {
        get => Scene.GetTransform(ObjectId);
        set
        {
            if (!Scene.SetTransform(ObjectId, value))
                throw new InvalidOperationException("A execução recusou a transformação.");
        }
    }
    /// <summary>
    /// O comportamento de <paramref name="target"/> que cumpre o contrato
    /// <typeparamref name="T"/>, ou null. `T` pode ser uma interface: é assim
    /// que um objeto interage com outro sem conhecer o tipo concreto dele.
    /// </summary>
    protected T? FindBehavior<T>(GameObject? target) where T : class =>
        target is { IsAlive: true } ? _registry?.FindBehavior(target.ObjectId, typeof(T)) as T : null;

    protected IEnumerable<T> FindBehaviors<T>(GameObject? target) where T : class =>
        target is { IsAlive: true } && _registry is not null
            ? _registry.FindBehaviors(target.ObjectId, typeof(T)).OfType<T>()
            : [];

    internal void Attach(ISceneAccess scene, ulong objectId, ulong instanceId, IBehaviorRegistry? registry = null)
    {
        if (_scene is not null || objectId == 0 || instanceId == 0)
            throw new InvalidOperationException("Invalid or duplicate behavior attachment.");
        _scene = scene; ObjectId = objectId; InstanceId = instanceId; _registry = registry;
    }
    internal void Detach() { _scene = null; _registry = null; _object = null; ObjectId = 0; InstanceId = 0; }
    public virtual void Start() { }
    public virtual void Update(float deltaTime) { }
    public virtual void FixedUpdate(float deltaTime) { }
    // Dispatched after each fixed step to behaviors on the sensor's owning body.
    // One event per body pair, regardless of how many compound parts overlap.
    public virtual void TriggerEnter(ObjectReference other) { }
    public virtual void TriggerStay(ObjectReference other) { }
    public virtual void TriggerExit(ObjectReference other) { }
    // Contato SÓLIDO, o par em que nenhum dos dois é sensor. Entregue por passo
    // físico, agregado por par de corpos. A normal acompanha Enter/Stay; o fim
    // de um contato não traz geometria, e por isso `Collision.Normal` é nulo lá.
    public virtual void CollisionEnter(Collision collision) { }
    public virtual void CollisionStay(Collision collision) { }
    public virtual void CollisionExit(Collision collision) { }
    public virtual void Stop() { }
}
