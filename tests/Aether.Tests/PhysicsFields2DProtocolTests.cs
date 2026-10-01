using System.Numerics;
using Astra;
using Astra.Components;
namespace Aether.Tests;
public static class PhysicsFields2DProtocolTests
{
    // Protocol recorder only. Actual solver effects are exercised by native acceptance.
    private sealed class Recorder : ISceneAccess
    {
        public uint WorldId=>7;
        public uint GenerationOf(ulong id)=>1;
        public bool Exists(ulong id)=>true;
        public WorldStatus LastStatus=>WorldStatus.ComponentMissing;
        public Vector3 Written,Point;
        public string Property="";
        public uint Operation,Layer;
        public bool Accept=true;
        public TransformValue GetTransform(ulong id)=>throw new NotSupportedException();
        public bool SetTransform(ulong id,TransformValue value)=>throw new NotSupportedException();
        public bool SetBodyVelocity(ulong id,Vector3 value)=>throw new NotSupportedException();
        public bool MoveKinematic(ulong id,Vector3 value,Quaternion rotation)=>throw new NotSupportedException();
        public void Log(ulong id,string message)=>throw new NotSupportedException();
        public ulong FindComponent(ulong id,string type,uint ordinal)=>type==WindField2D.TypeId?55UL:0;
        public bool SetTriple(ulong id,ulong instance,string property,Vector3 value){Property=property;Written=value;return true;}
        public bool FieldQuery(ulong id,uint world,uint generation,ulong instance,uint operation,Vector3 point,uint layer,out PhysicsFieldSample sample)
        {
            Assert.Equal(7u,world);Assert.Equal(1u,generation);Assert.Equal(55UL,instance);
            Point=point;Operation=operation;Layer=layer;
            sample=new(true,true,.5f,new(1,2,0),new(4,5,0),2,3,4,.5f,2,3);return Accept;
        }
    }
    [Test] public static void Vector2FacadePaddingQueryProjectionAndFailureContract()
    {
        var recorder=new Recorder();var owner=GameObject.Resolve(recorder,1);
        var component=owner.GetComponent(WindField2D.TypeId)??throw new Exception("Missing test component");
        var facade=new WindField2D(component);facade.Vector=new(4,5);
        Assert.Equal("vector",recorder.Property);Assert.Equal(new Vector3(4,5,0),recorder.Written);
        var field=owner.PhysicsField2D(WindField2D.TypeId);var point=new Vector2(6,7);
        var sample=field.Sample(point,3);Assert.Equal(new Vector3(6,7,0),recorder.Point);Assert.Equal(3u,recorder.Layer);
        Assert.Equal(new Vector2(1,2),sample.Acceleration);Assert.Equal(new Vector2(4,5),sample.WindVelocity);
        Assert.True(field.Contains(point));Assert.Equal(1u,recorder.Operation);
        Assert.Equal(2u,field.AffectedBodyCount);Assert.Equal(3f,field.AffectedMass);Assert.Equal(2u,recorder.Operation);
        Assert.True(field.TrySample(point,0,out _));recorder.Accept=false;
        Assert.False(field.TrySample(point,0,out _));Assert.Throws<WorldException>(()=>field.Sample(point));
        Assert.Throws<ArgumentException>(()=>field.Sample(new(float.NaN,0)));
        Assert.Throws<ArgumentException>(()=>field.Sample(point,32));
        Assert.Throws<ArgumentException>(()=>owner.PhysicsField2D("astra.physics.field.wind"));
    }
}
