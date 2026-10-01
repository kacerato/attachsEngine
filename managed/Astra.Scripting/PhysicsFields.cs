using System.Numerics;
namespace Astra;

/// <summary>Current field response at a world point. Wind drag is kg/s;
/// linear/angular drag are exponential rates per second. Acceleration is m/s².
/// Membership is evaluated at solver centers of mass, not collider surfaces.</summary>
public readonly record struct PhysicsFieldSample(bool ContainsPoint,bool IsEnabled,float Weight,
    Vector3 Acceleration,Vector3 WindVelocity,float WindDrag,float LinearDrag,float AngularDrag,
    float WorldGravityReplacement,uint AffectedBodies,float AffectedMass);

public sealed partial class GameObject
{
    /// <summary>Access one specific field component on this object. Select its
    /// type explicitly when several kinds share the same object.</summary>
    public PhysicsFieldRuntime PhysicsField(string typeId)
    {
        Require("acessar campo físico");
        if(typeId is not ("astra.physics.field.gravity" or "astra.physics.field.wind" or
            "astra.physics.field.drag" or "astra.physics.field.radial"))throw new ArgumentException("Tipo de campo físico inválido.",nameof(typeId));
        return new PhysicsFieldRuntime(GetComponent(typeId)??throw new WorldException(WorldStatus.ComponentMissing,"acessar campo físico"));
    }
}

/// <summary>Queries share the exact evaluator used by the native solver.
/// Removed components and expired worlds are rejected; disabled fields can be
/// inspected but return zero influence. Counts include dynamic bodies eligible
/// now, respecting layer and WakeBodies. Queries require a running physics session.</summary>
public readonly struct PhysicsFieldRuntime
{
    private readonly Component component;
    internal PhysicsFieldRuntime(Component value)=>component=value;
    public PhysicsFieldSample Sample(Vector3 worldPoint,uint layer=0)=>Query(0,worldPoint,layer);
    public bool TrySample(Vector3 worldPoint,uint layer,out PhysicsFieldSample sample)=>Execute(0,worldPoint,layer,out sample);
    /// <summary>Geometric containment, independent of Enabled and layer filters.</summary>
    public bool Contains(Vector3 worldPoint)=>Query(1,worldPoint,0).ContainsPoint;
    public uint AffectedBodyCount=>Query(2,default,0).AffectedBodies;
    public float AffectedMass=>Query(2,default,0).AffectedMass;
    private PhysicsFieldSample Query(uint operation,Vector3 point,uint layer)=>Execute(operation,point,layer,out var sample)?sample:throw new WorldException(component.Scene.LastStatus,"consultar campo físico");
    private bool Execute(uint operation,Vector3 point,uint layer,out PhysicsFieldSample sample)
    {
        sample=default;
        if(component.Object is null)throw new WorldException(WorldStatus.InvalidArgument,"campo não inicializado");
        if(layer>=32||!float.IsFinite(point.X)||!float.IsFinite(point.Y)||!float.IsFinite(point.Z))throw new ArgumentException("Ponto finito e camada entre 0 e 31 são obrigatórios.");
        var owner=component.Object;
        return component.Scene.FieldQuery(owner.ObjectId,owner.World,owner.Generation,component.InstanceId,operation,point,layer,out sample);
    }
}
