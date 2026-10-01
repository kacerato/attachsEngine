using System.Numerics;
using Astra;
namespace Aether.Tests;
public static class PhysicsQueryContractsTests
{
    // Typed SDK transport only; Jolt, scene identity and archive/lifecycle have
    // separate executable native acceptance, without pretending this is physics.
    private sealed class Transport : ISceneAccess
    {
        public int Calls, Result=2;
        public uint WorldId=>72;
        public uint GenerationOf(ulong id)=>1;
        public bool Exists(ulong id)=>id==1;
        public TransformValue GetTransform(ulong id)=>default;
        public bool SetTransform(ulong id,TransformValue value)=>false;
        public bool SetBodyVelocity(ulong id,Vector3 value)=>false;
        public bool MoveKinematic(ulong id,Vector3 value,Quaternion rotation)=>false;
        public void Log(ulong id,string message){}
        private static RawQueryHit Hit()=>new(){Object=1,Collider=9,PointX=4,NormalX=-1,Distance=4,Fraction=.4f,Flags=RawQueryHit.HasNormalFlag};
        public int RayCast(Vector3 origin,Vector3 direction,in QueryFilter filter,Span<RawQueryHit> output){++Calls;if(Result>0&&output.Length>0)output[0]=Hit();return Result;}
        public int ShapeCast(in ShapeQuery shape,Vector3 origin,Vector3 direction,in QueryFilter filter,out RawQueryHit hit){++Calls;hit=Hit();return Result<0?Result:1;}
        public int Overlap(in ShapeQuery shape,Vector3 origin,in QueryFilter filter,Span<RawQueryHit> output){++Calls;if(Result>0&&output.Length>0){output[0]=Hit();output[0].Flags=RawQueryHit.SensorFlag;}return Result;}
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
        scene.Result=0;Assert.False(physics.RayCast(Vector3.Zero,Vector3.UnitX).HasValue);
    }
}
