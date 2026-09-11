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
/// nativa v3 (native/scene/script_runtime.h). O documento autoral nunca cruza
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
}

public abstract class Behavior
{
    private ISceneAccess? _scene;
    public ulong ObjectId { get; private set; }
    public ulong InstanceId { get; private set; }
    public bool Enabled { get; set; } = true;
    protected ISceneAccess Scene => _scene ?? throw new InvalidOperationException("Behavior is not attached to an execution world.");

    /// <summary>O objeto a que este comportamento está anexado.</summary>
    protected GameObject Object => GameObject.Wrap(Scene, ObjectId);

    /// <summary>Resolve uma referência autorada no inspetor para um objeto vivo.</summary>
    protected GameObject? Resolve(ObjectReference reference)
    {
        if (reference.ObjectId == 0 || !Scene.Exists(reference.ObjectId)) return null;
        return GameObject.Wrap(Scene, reference.ObjectId);
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
    internal void Attach(ISceneAccess scene, ulong objectId, ulong instanceId)
    {
        if (_scene is not null || objectId == 0 || instanceId == 0)
            throw new InvalidOperationException("Invalid or duplicate behavior attachment.");
        _scene = scene; ObjectId = objectId; InstanceId = instanceId;
    }
    internal void Detach() { _scene = null; ObjectId = 0; InstanceId = 0; }
    public virtual void Start() { }
    public virtual void Update(float deltaTime) { }
    public virtual void FixedUpdate(float deltaTime) { }
    // Dispatched after each fixed step to behaviors on the sensor's owning body.
    // One event per body pair, regardless of how many compound parts overlap.
    public virtual void TriggerEnter(ObjectReference other) { }
    public virtual void TriggerStay(ObjectReference other) { }
    public virtual void TriggerExit(ObjectReference other) { }
    public virtual void Stop() { }
}
