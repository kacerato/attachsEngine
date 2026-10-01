using System.Numerics;
namespace Astra;
/// <summary>XY sample from the same evaluator consumed by Box2D. Wind drag
/// is kg/s; damping rates are per second. Centers of mass define membership.</summary>
public readonly record struct PhysicsFieldSample2D(bool ContainsPoint,bool IsEnabled,float Weight,
    Vector2 Acceleration,Vector2 WindVelocity,float WindDrag,float LinearDrag,float AngularDrag,
    float WorldGravityReplacement,uint AffectedBodies,float AffectedMass);
public sealed partial class GameObject
{
    public PhysicsField2DRuntime PhysicsField2D(string typeId)
    {
        Require("acessar campo físico 2D");
        if(typeId is not ("astra.physics2d.field.gravity" or "astra.physics2d.field.wind" or
            "astra.physics2d.field.drag" or "astra.physics2d.field.radial"))throw new ArgumentException("Tipo de campo 2D inválido.",nameof(typeId));
        return new(GetComponent(typeId)??throw new WorldException(WorldStatus.ComponentMissing,"acessar campo 2D"));
    }
}
/// <summary>Independent Box2D queries; expired worlds and removed components
/// fail explicitly. Counts are current eligible dynamic bodies.</summary>
public readonly struct PhysicsField2DRuntime
{
    private readonly Component component;
    internal PhysicsField2DRuntime(Component value)=>component=value;
    public PhysicsFieldSample2D Sample(Vector2 point,uint layer=0)=>Query(0,point,layer);
    public bool TrySample(Vector2 point,uint layer,out PhysicsFieldSample2D sample)=>Execute(0,point,layer,out sample);
    public bool Contains(Vector2 point)=>Query(1,point,0).ContainsPoint;
    public uint AffectedBodyCount=>Query(2,default,0).AffectedBodies;
    public float AffectedMass=>Query(2,default,0).AffectedMass;
    private PhysicsFieldSample2D Query(uint operation,Vector2 point,uint layer)=>Execute(operation,point,layer,out var sample)?sample:throw new WorldException(component.Scene.LastStatus,"consultar campo 2D");
    private bool Execute(uint operation,Vector2 point,uint layer,out PhysicsFieldSample2D sample)
    {
        sample=default;
        if(component.Object is null)throw new WorldException(WorldStatus.InvalidArgument,"campo 2D não inicializado");
        if(layer>=32||!float.IsFinite(point.X)||!float.IsFinite(point.Y))throw new ArgumentException("Ponto XY finito e camada de 0 a 31 obrigatórios.");
        var owner=component.Object;
        if(!component.Scene.FieldQuery(owner.ObjectId,owner.World,owner.Generation,component.InstanceId,operation,new Vector3(point,0),layer,out var raw))return false;
        sample=new(raw.ContainsPoint,raw.IsEnabled,raw.Weight,new(raw.Acceleration.X,raw.Acceleration.Y),new(raw.WindVelocity.X,raw.WindVelocity.Y),raw.WindDrag,raw.LinearDrag,raw.AngularDrag,raw.WorldGravityReplacement,raw.AffectedBodies,raw.AffectedMass);return true;
    }
}
