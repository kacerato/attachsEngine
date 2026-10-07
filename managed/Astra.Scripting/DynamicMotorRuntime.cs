using System.Numerics;
namespace Astra;

public sealed partial class GameObject
{
    public DynamicMotorRuntime DynamicMotor()
    {
        Require("acessar motor dinâmico");
        var component = GetComponent("astra.physics.dynamic_motor") ?? throw new WorldException(WorldStatus.ComponentMissing, "acessar motor dinâmico");
        return new DynamicMotorRuntime(component);
    }
}

/// <summary>Instance-bound commands on the real Body motor. Move applies for the
/// current script frame; call in Update or FixedUpdate. FixedUpdate can override
/// Update; ReleaseMove relinquishes the Script channel to the motor's authored
/// ownership policy. Commands never write pose.</summary>
public readonly struct DynamicMotorRuntime
{
    private readonly Component component;
    internal DynamicMotorRuntime(Component value) => component=value;
    public PhysicsBodyState ReadBodyState() => Command(103);
    public void Move(Vector2 input, float yawRadians=0) => Command(100,new Vector3(input,yawRadians));
    /// <summary>Queues one attempt for the next fixed step. Support decides acceptance;
    /// true means queued, not that an airborne jump was allowed.</summary>
    public bool TryJump() => Execute(101,default,out _);
    public void ReleaseMove() => Command(102);
    private PhysicsBodyState Command(uint op,Vector3 value=default) => Execute(op,value,out var state)?state:throw new WorldException(component.Scene.LastStatus,"motor dinâmico");
    private bool Execute(uint op,Vector3 value,out PhysicsBodyState state)
    {
        if(component.Object is null)throw new WorldException(WorldStatus.InvalidArgument,"motor não inicializado");
        if(!float.IsFinite(value.X)||!float.IsFinite(value.Y)||!float.IsFinite(value.Z))throw new ArgumentException("Comando deve ser finito.");
        var owner=component.Object;
        return component.Scene.BodyCommand(owner.ObjectId,owner.World,owner.Generation,component.InstanceId,op,value,default,out state);
    }
}
