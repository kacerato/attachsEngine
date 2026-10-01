using System.Numerics;
using Astra;
namespace Aether.Tests;
public static class SplineFrameApiTests
{
    // SDK transport recorder only. Native family scenarios exercise GameWorld,
    // ScenePaths, real Play bridge, archive and executed editor separately.
    private sealed class Transport : ISceneAccess
    {
        public uint WorldId=>17;
        public bool Alive=true;
        public bool Exists(ulong id)=>Alive&&id==1;
        public uint GenerationOf(ulong id)=>Exists(id)?2u:0;
        public WorldStatus LastStatus=>WorldStatus.Rejected;
        public TransformValue GetTransform(ulong id)=>new(Vector3.Zero,Quaternion.Identity,Vector3.One);
        public bool SetTransform(ulong id,TransformValue value)=>throw new NotSupportedException();
        public bool SetBodyVelocity(ulong id,Vector3 value)=>throw new NotSupportedException();
        public bool MoveKinematic(ulong id,Vector3 position,Quaternion rotation)=>throw new NotSupportedException();
        public void Log(ulong id,string value)=>throw new NotSupportedException();
        public int ComponentCount(ulong id)=>Exists(id)?1:0;
        public (ulong Instance,string TypeId) ComponentAt(ulong id,uint index)=>(42,ComponentIds.Path);
        public ulong FindComponent(ulong id,string typeId,uint ordinal)=>Exists(id)&&typeId==ComponentIds.Path&&ordinal==0?42ul:0;
        public float[] Point=new float[10];
        public int Writes;
        public Vector3 Up=Vector3.UnitY;
        public bool SetTriple(ulong id,ulong instance,string property,Vector3 value)
        {Assert.Equal("up",property);if(value==Vector3.Zero)return false;Up=value;Writes++;return true;}
        public bool TryGetProperty(ulong id,ulong instance,string property,out uint kind,out ulong bits)
        {kind=0;bits=BitConverter.SingleToUInt32Bits(property switch{"up_x"=>Up.X,"up_y"=>Up.Y,"up_z"=>Up.Z,_=>throw new InvalidOperationException()});return true;}
        public int PathPointCommand(ulong id,uint world,uint generation,ulong instance,uint operation,ulong element,uint index,ReadOnlySpan<float> input,Span<float> output,out ulong identity)
        {
            Assert.Equal(17u,world);Assert.Equal(2u,generation);Assert.Equal(42ul,instance);identity=99;
            switch(operation){
                case 9:case 10:Assert.Equal(10,input.Length);input.CopyTo(Point);Writes++;return 1;
                case 4:Assert.Equal(9,input.Length);input.CopyTo(Point);Writes++;return 1;
                case 7:case 8:Assert.Equal(10,output.Length);Point.CopyTo(output);return 1;
                default:throw new InvalidOperationException("Unexpected protocol operation");
            }
        }
        public bool PathRuntimeCommand(ulong id,uint world,uint generation,ulong instance,uint operation,double distance,bool wrap,Span<float> output,out double scalar)
        {
            Assert.Equal(5u,operation);Assert.Equal(10,output.Length);Assert.True(wrap);Assert.Equal(3d,distance);
            new float[]{0,0,3,0,0,1,-1,0,0,90}.CopyTo(output);scalar=10;return true;
        }
    }
    [Test] public static void SplineFrame_FullPointLegacyGeometryAtomicUpAndFrameTransport()
    {
        var transport=new Transport();var component=GameObject.Resolve(transport,1).GetComponent(ComponentIds.Path)!.Value;var path=new CurvePath(component);
        var identity=path.Insert(0,new(0,0,10),rollDegrees:90);Assert.Equal(99ul,identity);Assert.Equal(1,transport.Writes);
        Assert.Close(90,path.At(0).RollDegrees);Assert.Close(90,path.ById(identity).RollDegrees);
        path.Set(identity,new(0,0,12),Vector3.Zero,Vector3.Zero);Assert.Close(90,path.ById(identity).RollDegrees);
        path.Set(identity,new(0,0,13),Vector3.Zero,Vector3.Zero,135);Assert.Close(135,path.ById(identity).RollDegrees);
        int writes=transport.Writes;path.Up=Vector3.UnitX;Assert.Equal(writes+1,transport.Writes);Assert.Equal(Vector3.UnitX,path.Up);
        Assert.Throws<WorldException>(()=>path.Up=Vector3.Zero);Assert.Equal(Vector3.UnitX,path.Up);
        var frame=path.SampleFrame(3,true);Assert.Equal(new Vector3(0,0,3),frame.Position);Assert.Equal(-Vector3.UnitX,frame.Up);Assert.Equal(Vector3.UnitY,frame.Right);Assert.Close(90,frame.RollDegrees);Assert.Equal(10d,frame.Length);
        writes=transport.Writes;Assert.Throws<ArgumentOutOfRangeException>(()=>path.Insert(0,Vector3.Zero,rollDegrees:float.NaN));Assert.Equal(writes,transport.Writes);
        Assert.Throws<ArgumentOutOfRangeException>(()=>path.SampleFrame(double.PositiveInfinity));
        transport.Alive=false;Assert.Throws<WorldException>(()=>path.At(0));
    }
}
