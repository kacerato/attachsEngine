using System.Numerics;
namespace Astra;

/// <summary>Observed solver state in world space. Angular velocity is rad/s.
/// Runtime only: capturing this does not change the authored initial velocity.</summary>
public readonly record struct PhysicsBodyState(Vector3 LinearVelocity, Vector3 AngularVelocity,
    Vector3 CenterOfMass, bool IsActive, bool IsSleeping);

public sealed partial class GameObject
{
    public PhysicsBodyRuntime PhysicsBody()
    {
        Require("acessar corpo físico");
        var component = GetComponent("astra.physics.body") ?? throw new WorldException(WorldStatus.ComponentMissing, "acessar corpo físico");
        return new PhysicsBodyRuntime(component);
    }
}

/// <summary>Instance-bound access to the real native body. Recreating or removing
/// the component invalidates this access; rebuilding its solver body preserves it.</summary>
public readonly struct PhysicsBodyRuntime
{
    private readonly Component component;
    internal PhysicsBodyRuntime(Component value) => component = value;
    public PhysicsBodyState ReadState() => Command(0);
    public bool TryReadState(out PhysicsBodyState state) => Execute(0, default, default, out state);
    public Vector3 AngularVelocity { get => ReadState().AngularVelocity; set => Command(1, value); }
    public Vector3 CenterOfMass => ReadState().CenterOfMass;
    public bool IsSleeping => ReadState().IsSleeping;
    public bool IsActive => ReadState().IsActive;
    public Vector3 GetPointVelocity(Vector3 worldPoint) => Command(9, worldPoint).LinearVelocity;
    public Vector3 GetRelativePointVelocity(Vector3 localPoint) => GetPointVelocity(component.Object.TransformPoint(localPoint));
    public void WakeUp() => Command(2);
    /// <summary>Refuses static/kinematic bodies and bodies with AllowSleep false.
    /// Jolt sleep clears velocity; waking does not restore it.</summary>
    public void Sleep() => Command(3);
    public void AddForceAtPosition(Vector3 force, Vector3 worldPoint) => Command(4, force, worldPoint);
    public void AddImpulseAtPosition(Vector3 impulse, Vector3 worldPoint) => Command(5, impulse, worldPoint);
    /// <summary>Both velocities are published under one body lock, capped by its solver limits.</summary>
    public void SetLinearAndAngularVelocity(Vector3 linear, Vector3 angular) => Command(6, linear, angular);
    public void AddVelocityChange(Vector3 delta) => Command(7, delta);
    public void AddAngularVelocityChange(Vector3 delta) => Command(8, delta);

    private bool Execute(uint operation, Vector3 value, Vector3 point, out PhysicsBodyState state)
    {
        state = default;
        if (component.Object is null) throw new WorldException(WorldStatus.InvalidArgument, "corpo não inicializado");
        if (!Finite(value) || !Finite(point)) throw new ArgumentException("Valores do corpo devem ser finitos.");
        var owner = component.Object;
        return component.Scene.BodyCommand(owner.ObjectId, owner.World, owner.Generation,
            component.InstanceId, operation, value, point, out state);
    }
    private PhysicsBodyState Command(uint op, Vector3 value = default, Vector3 point = default) =>
        Execute(op, value, point, out var state) ? state : throw new WorldException(component.Scene.LastStatus, "operação de corpo físico");
    private static bool Finite(Vector3 value) => float.IsFinite(value.X) && float.IsFinite(value.Y) && float.IsFinite(value.Z);
}
