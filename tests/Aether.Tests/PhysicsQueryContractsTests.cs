using System.Numerics;
using Astra;
using Astra.Runtime;
using System.Runtime.InteropServices;
namespace Aether.Tests;
public static class PhysicsQueryContractsTests
{
    // Typed SDK transport only; Jolt, scene identity and archive/lifecycle have
    // separate executable native acceptance, without pretending this is physics.
    private sealed class Transport : ISceneAccess
    {
        public int Calls, Result=2;
        public bool ChildAlive=true,Removed,Enabled=true;
        public ulong EditedObject;
        public WorldStatus LastStatus=>WorldStatus.NotRunning;
        public uint WorldId=>72;
        public uint GenerationOf(ulong id)=>id==1?1u:id==2?(ChildAlive?7u:0u):id==3?8u:0u;
        public bool Exists(ulong id)=>GenerationOf(id)!=0;
        public int ComponentCount(ulong id)=>id is 2 or 3 && !Removed?1:0;
        public (ulong Instance,string TypeId) ComponentAt(ulong id,uint index)=>(9,Astra.Components.Collider.TypeId);
        public bool SetProperty(ulong id,ulong instance,string property,uint kind,ulong bits){EditedObject=id;Enabled=bits!=0;return id==2&&instance==9&&property=="enabled"&&kind==1;}
        public TransformValue GetTransform(ulong id)=>default;
        public bool SetTransform(ulong id,TransformValue value)=>false;
        public bool SetBodyVelocity(ulong id,Vector3 value)=>false;
        public bool MoveKinematic(ulong id,Vector3 value,Quaternion rotation)=>false;
        public void Log(ulong id,string message){}
        private static RawQueryHit Hit(ulong owner=2)=>new(){Object=1,ColliderObject=owner,Collider=9,PointX=4,NormalX=-1,Distance=4,Fraction=.4f,Flags=RawQueryHit.HasNormalFlag};
        public int RayCast(Vector3 origin,Vector3 direction,in QueryFilter filter,Span<RawQueryHit> output){++Calls;for(int i=0;i<Math.Min(Result,output.Length);++i)output[i]=Hit((ulong)i+2);return Result;}
        public int ShapeCast(in ShapeQuery shape,Vector3 origin,Vector3 direction,in QueryFilter filter,out RawQueryHit hit){++Calls;hit=Hit(3);return Result<0?Result:1;}
        public int Overlap(in ShapeQuery shape,Vector3 origin,in QueryFilter filter,Span<RawQueryHit> output){++Calls;for(int i=0;i<Math.Min(Result,output.Length);++i){output[i]=Hit((ulong)i+2);output[i].Flags=RawQueryHit.SensorFlag;}return Result;}
    }
    [Test] public static void PhysicsQueries_TypedHitsTruncationInvalidInputsAndBackendErrors()
    {
        var scene=new Transport();var physics=new PhysicsAccess(scene);
        var hits=physics.RayCastAll(Vector3.Zero,new(10,0,0),out var truncated,capacity:1);
        Assert.True(truncated);Assert.Equal(1,hits.Count);Assert.Equal(9UL,hits[0].ColliderInstance);
        Assert.Close(4,hits[0].Distance);Assert.True(hits[0].Normal.HasValue);Assert.False(hits[0].IsSensor);
        hits=physics.Overlap(ShapeQuery.Sphere(.5f),Vector3.Zero,out truncated,capacity:1);
        Assert.True(truncated&&hits[0].IsSensor&&!hits[0].Normal.HasValue);
        var before=scene.Calls;
        Assert.Throws<ArgumentOutOfRangeException>(()=>physics.RayCast(Vector3.Zero,Vector3.Zero));
        Assert.Throws<ArgumentOutOfRangeException>(()=>physics.RayCast(new(float.NaN,0,0),Vector3.UnitX));
        Assert.Throws<ArgumentOutOfRangeException>(()=>physics.ShapeCast(ShapeQuery.Sphere(-1),Vector3.Zero,Vector3.UnitX));
        Assert.Throws<ArgumentOutOfRangeException>(()=>physics.Overlap(ShapeQuery.Box(Vector3.One,default(Quaternion)),Vector3.Zero,out _));
        Assert.Throws<ArgumentOutOfRangeException>(()=>physics.RayCastAll(Vector3.Zero,Vector3.UnitX,out _,capacity:0));
        Assert.Throws<ArgumentOutOfRangeException>(()=>QueryFilter.Default.OnlyLayers(1,32));
        Assert.Equal(before,scene.Calls);
        scene.Result=-1;
        Assert.Throws<InvalidOperationException>(()=>physics.RayCast(Vector3.Zero,Vector3.UnitX));
        Assert.Throws<InvalidOperationException>(()=>physics.RayCastAll(Vector3.Zero,Vector3.UnitX,out _));
        Assert.Throws<InvalidOperationException>(()=>physics.ShapeCast(ShapeQuery.Sphere(1),Vector3.Zero,Vector3.UnitX));
        Assert.Throws<InvalidOperationException>(()=>physics.Overlap(ShapeQuery.Sphere(1),Vector3.Zero,out _));
        try { physics.RayCast(Vector3.Zero,Vector3.UnitX); throw new Exception("Query did not reject stopped backend"); }
        catch(WorldException error) { Assert.Equal(WorldStatus.NotRunning,error.Status); }
        scene.Result=0;Assert.False(physics.RayCast(Vector3.Zero,Vector3.UnitX).HasValue);
    }
    [Test] public static void PhysicsQueries_CompoundChildResolutionEditingAndExpiration()
    {
        var scene=new Transport();var physics=new PhysicsAccess(scene);
        var hits=physics.RayCastAll(Vector3.Zero,new(10,0,0),out var truncated);
        Assert.False(truncated);Assert.Equal(2,hits.Count);
        Assert.Equal(hits[0].BodyObject,hits[1].BodyObject);Assert.Equal(1UL,hits[0].Object.ObjectId);
        Assert.Equal(2UL,hits[0].ColliderObject!.ObjectId);Assert.Equal(3UL,hits[1].ColliderObject!.ObjectId);
        Assert.Equal(7u,hits[0].ColliderObject!.Generation);Assert.Equal(8u,hits[1].ColliderObject!.Generation);
        Assert.Equal(9UL,hits[0].ColliderInstance);Assert.Equal(9UL,hits[1].ColliderInstance);
        var exact=hits[0].Collider!.Value;var component=exact.Component;component.Enabled=false;
        Assert.Equal(2UL,scene.EditedObject);Assert.False(scene.Enabled);
        Assert.Equal(2UL,physics.RayCast(Vector3.Zero,new(10,0,0))!.Value.ColliderObject!.ObjectId);
        Assert.Equal(3UL,physics.ShapeCast(ShapeQuery.Sphere(1),Vector3.Zero,new(10,0,0))!.Value.Collider!.Value.Object.ObjectId);
        hits=physics.Overlap(ShapeQuery.Sphere(1),Vector3.Zero,out _);
        Assert.Equal(2UL,hits[0].Collider!.Value.Object.ObjectId);Assert.Equal(3UL,hits[1].Collider!.Value.Object.ObjectId);
        scene.Removed=true;Assert.False(hits[0].Collider.HasValue);scene.Removed=false;
        scene.ChildAlive=false;Assert.False(hits[0].ColliderObject!.IsAlive);Assert.False(hits[0].Collider.HasValue);Assert.False(exact.IsAlive);
        // Existing positional constructor remains source compatible; unknown identity is not guessed.
        var legacy=new RayHit(hits[0].Object,9,default,null,0,0,false);Assert.True(legacy.ColliderObject is null);Assert.False(legacy.Collider.HasValue);
    }
    [Test] public static unsafe void PhysicsQueries_PacketLayoutAndOldNativeAbiRejected()
    {
        Assert.Equal(64,Marshal.SizeOf<RawQueryHit>());Assert.Equal(56L,Marshal.OffsetOf<RawQueryHit>(nameof(RawQueryHit.ColliderObject)).ToInt64());
        NativeBehaviorRuntime.SceneAccess access=default;access.Version=43;access.Size=(uint)sizeof(NativeBehaviorRuntime.SceneAccess);
        byte root=1,json=1;
        delegate* unmanaged<byte*,int,byte*,int,NativeBehaviorRuntime.SceneAccess*,int> start=&NativeBehaviorRuntime.Start;
        Assert.Equal(1,start(&root,1,&json,1,&access));
    }
    [Test] public static void PhysicsQueries_CompoundExampleUsesRealProjectCompiler()
    {
        var root=new DirectoryInfo(AppContext.BaseDirectory);
        while(root is not null&&!File.Exists(Path.Combine(root.FullName,"examples","physics","CompoundQueryProbe.cs"))) root=root.Parent;
        Assert.True(root is not null);
        var result=new Astra.Compilation.ProjectCompiler().Build(Path.Combine(root!.FullName,"examples","physics"));
        Assert.True(result.Success,string.Join("\n",result.Diagnostics.Select(d=>d.Message)));
        Assert.Equal("example.physics.compound-query-probe",result.Project!.Types.Single().Id);
    }
}
