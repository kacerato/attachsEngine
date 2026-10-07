using System.Numerics;
namespace Astra;
/// <summary>Origin of locomotion intent. None selects automatic arbitration in
/// PolicySource and denotes no active intent in a measured State.</summary>
public enum MotorControlSource:uint {None,Ui,Keyboard,Gamepad,Script,Ai}
public readonly record struct MotorControlState(MotorControlSource Source,uint Candidates,bool Focused,bool HasMeasuredStep,bool JumpAttempt,Vector2 Move,float YawRadians,float Priority);
/// <summary>Measured solver motion. Ground velocity is contact-point velocity;
/// subtract it to animate relative motion on a moving platform. SupportObjectId
/// is resolved for dynamic motors; Character's native ground-body handle is not an object ID.</summary>
public readonly record struct MotorMotionState(bool HasMeasuredStep,bool Grounded,Vector3 Velocity,Vector3 GroundVelocity,Vector3 GroundNormal,Vector3 GroundPoint,ulong SupportObjectId);
public sealed partial class GameObject
{
    public MotorControlRuntime MotorControl()
    {
        Require("acessar posse de controle");
        var component=GetComponent("astra.physics.dynamic_motor")??GetComponent("astra.physics.character")??throw new WorldException(WorldStatus.ComponentMissing,"acessar posse de controle");
        return new(component);
    }
}
/// <summary>Five bounded origin channels per motor. Submit Script/AI every
/// Update/FixedUpdate. Zero explicitly claims a stop for that frame. Equal-source
/// submissions use actual script execution order. Release cancels its queued jump.
/// Commands preserve the object's mesh, collision, and physical authority.</summary>
public readonly struct MotorControlRuntime
{
    private readonly Component component;
    internal MotorControlRuntime(Component value)=>component=value;
    public MotorControlSource PolicySource
    {
        get=>(MotorControlSource)component.GetEnum("control_source");
        set {if((uint)value>5)throw new ArgumentOutOfRangeException(nameof(value));component.SetEnum("control_source",(uint)value);}
    }
    public MotorControlState State=>Command(2,MotorControlSource.None,default,false);
    public MotorMotionState MotionState {
        get {
            if(component.Object is null)throw new WorldException(WorldStatus.InvalidArgument,"motor não inicializado");
            var o=component.Object;
            if(!component.Scene.ReadMotorMotion(o.ObjectId,o.World,o.Generation,component.InstanceId,out var state))throw new WorldException(component.Scene.LastStatus,"movimento físico do motor");
            return state;
        }
    }
    public void Submit(MotorControlSource source,Vector2 input,float yawRadians=0,bool jump=false)=>Command(0,source,new(input,yawRadians),jump);
    public void Release(MotorControlSource source)=>Command(1,source,default,false);
    private MotorControlState Command(uint operation,MotorControlSource source,Vector3 input,bool jump)
    {
        if(component.Object is null)throw new WorldException(WorldStatus.InvalidArgument,"controle não inicializado");
        if(operation<2&&source is not (MotorControlSource.Script or MotorControlSource.Ai))throw new ArgumentException("Scripts só podem publicar nos canais Script e IA.",nameof(source));
        if(!float.IsFinite(input.X)||!float.IsFinite(input.Y)||!float.IsFinite(input.Z)||Math.Abs(input.X)>1||Math.Abs(input.Y)>1)throw new ArgumentException("Entrada deve ser finita e estar em [-1,1].",nameof(input));
        var owner=component.Object;
        if(!component.Scene.MotorControlCommand(owner.ObjectId,owner.World,owner.Generation,component.InstanceId,operation,source,input,jump,out var state))throw new WorldException(component.Scene.LastStatus,"posse de controle");
        return state;
    }
}
