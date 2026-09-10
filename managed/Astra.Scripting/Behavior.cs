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

/// <summary>Commands address the execution world. The authoring document never crosses this API.</summary>
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
}

public abstract class Behavior
{
    private ISceneAccess? _scene;
    public ulong ObjectId { get; private set; }
    public ulong InstanceId { get; private set; }
    public bool Enabled { get; set; } = true;
    protected ISceneAccess Scene => _scene ?? throw new InvalidOperationException("Behavior is not attached to an execution world.");
    protected TransformValue Transform
    {
        get => Scene.GetTransform(ObjectId);
        set
        {
            if (!Scene.SetTransform(ObjectId, value))
                throw new InvalidOperationException("The execution world rejected the transform.");
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
