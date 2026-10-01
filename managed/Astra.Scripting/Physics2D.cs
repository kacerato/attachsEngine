using System.Numerics;
namespace Astra;
public enum Body2DCommandKind:uint {GetVelocity,SetVelocity,Force,Impulse,Torque,AngularImpulse,MoveKinematic}
public readonly record struct PhysicsHit2D(GameObject Object,ulong ColliderInstance,Vector2 Point,Vector2? Normal,float Distance,float Fraction,bool IsSensor);
public readonly record struct BodyVelocity2D(Vector2 Linear,float AngularDegrees);
/// <summary>Independent XY solver access. Forces use N, impulse N.s, rotation degrees.</summary>
public readonly struct Physics2DAccess(ISceneAccess scene)
{
    public Body2DAccess Body(GameObject owner) => new(scene,owner);
    public int RayCast(Vector2 origin,Vector2 translation,Span<PhysicsHit2D> results,QueryFilter? filter=null) => Query(false,origin,translation,0,results,filter??QueryFilter.Default);
    public int OverlapCircle(Vector2 center,float radius,Span<PhysicsHit2D> results,QueryFilter? filter=null) => Query(true,center,default,radius,results,filter??QueryFilter.Default);
    private int Query(bool overlap,Vector2 origin,Vector2 translation,float radius,Span<PhysicsHit2D> results,in QueryFilter filter)
    {
        if(results.Length>4096||!float.IsFinite(origin.X)||!float.IsFinite(origin.Y)||!float.IsFinite(translation.X)||!float.IsFinite(translation.Y)||!float.IsFinite(radius)||radius<0||(overlap&&radius<=0))throw new ArgumentOutOfRangeException(nameof(radius));
        Span<RawQueryHit> raw=results.Length<=32?stackalloc RawQueryHit[results.Length]:new RawQueryHit[results.Length];
        var count=scene.Query2D(scene.WorldId,overlap,origin,translation,radius,filter,raw);
        if(count<0)throw new WorldException(scene.LastStatus,"consultar física 2D");
        for(int i=0;i<Math.Min(count,results.Length);++i){var hit=raw[i];results[i]=new(GameObject.Resolve(scene,hit.Object),hit.Collider,new(hit.PointX,hit.PointY),(hit.Flags&RawQueryHit.HasNormalFlag)!=0?new Vector2(hit.NormalX,hit.NormalY):null,hit.Distance,hit.Fraction,(hit.Flags&RawQueryHit.SensorFlag)!=0);}
        return count;
    }
}
public readonly struct Body2DAccess
{
    private readonly ISceneAccess scene;private readonly GameObject owner;
    internal Body2DAccess(ISceneAccess scene,GameObject owner){ArgumentNullException.ThrowIfNull(owner);if(!owner.BelongsTo(scene))throw new WorldException(WorldStatus.ForeignWorld,"corpo 2D");this.scene=scene;this.owner=owner;}
    public BodyVelocity2D Velocity {get=>Command(Body2DCommandKind.GetVelocity,default,0);set=>Command(Body2DCommandKind.SetVelocity,value.Linear,value.AngularDegrees);}
    public void AddForce(Vector2 force)=>Command(Body2DCommandKind.Force,force,0);
    public void AddImpulse(Vector2 impulse)=>Command(Body2DCommandKind.Impulse,impulse,0);
    public void AddTorque(float torque)=>Command(Body2DCommandKind.Torque,new(torque,0),0);
    public void AddAngularImpulse(float impulse)=>Command(Body2DCommandKind.AngularImpulse,new(impulse,0),0);
    public void MoveKinematic(Vector2 position,float rotationDegrees)=>Command(Body2DCommandKind.MoveKinematic,position,rotationDegrees);
    private BodyVelocity2D Command(Body2DCommandKind command,Vector2 value,float angular)
    {
        if(scene is null||owner is null)throw new InvalidOperationException("Uninitialized 2D body access.");
        if(!owner.IsAlive)throw new WorldException(WorldStatus.StaleHandle,"corpo 2D");
        if(!float.IsFinite(value.X)||!float.IsFinite(value.Y)||!float.IsFinite(angular))throw new ArgumentOutOfRangeException(nameof(value));
        if(!scene.Body2DCommand(owner.ObjectId,owner.World,owner.Generation,command,value,angular,out var linear,out var spin))throw new WorldException(scene.LastStatus,"comando corpo 2D");
        return new(linear,spin);
    }
}
