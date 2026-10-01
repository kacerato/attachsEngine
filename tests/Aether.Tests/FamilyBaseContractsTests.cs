using System.Numerics;
using Astra;
namespace Aether.Tests;
public static class FamilyBaseContractsTests
{
    // Authoring read/write recorder only: mathematical and SDK transport evidence.
    // Native acceptance separately exercises actual GameWorld/physics ownership.
    private sealed class Poses : ISceneAccess
    {
        public uint WorldId=>8;
        public uint GenerationOf(ulong id)=>id<=3?1u:0;
        public bool Exists(ulong id)=>id<=3;
        public WorldStatus LastStatus=>Owned?WorldStatus.TransformOwnedByPhysics:WorldStatus.Ok;
        public bool Owned;
        public int Writes;
        public int Layer=2;
        public TransformValue[] Values=[default,
            new(new(3,-2,1),Quaternion.CreateFromAxisAngle(Vector3.UnitY,.8f),new(2,3,4)),
            new(new(1,2,-1),Quaternion.CreateFromAxisAngle(Vector3.UnitZ,.6f),new(1,2,1)),
            new(new(2,1,3),Quaternion.CreateFromAxisAngle(Vector3.UnitX,.4f),Vector3.One)];
        public ulong ParentOf(ulong id)=>id>1?id-1:0;
        public TransformValue GetTransform(ulong id)=>Values[id];
        public bool SetTransform(ulong id,TransformValue value){if(Owned)return false;Values[id]=value;Writes++;return true;}
        public TransformValue GetWorldTransform(ulong id)=>throw new InvalidOperationException("Sheared world has no exact TRS");
        public bool SetWorldTransform(ulong id,TransformValue value)=>throw new InvalidOperationException("Cannot flatten shear");
        public bool SetBodyVelocity(ulong id,Vector3 value)=>throw new NotSupportedException();
        public bool MoveKinematic(ulong id,Vector3 position,Quaternion rotation)=>throw new NotSupportedException();
        public void Log(ulong id,string value)=>throw new NotSupportedException();
        public int ObjectLayer(ulong id,uint world,uint generation,int layer){Assert.Equal(8u,world);Assert.Equal(1u,generation);if(layer>=0)Layer=layer;return Layer;}
    }
    private static void Near(Vector3 a,Vector3 b)=>Assert.True(Vector3.Distance(a,b)<.0001f,$"{a} != {b}");
    private static void Rotation(Quaternion a,Quaternion b)=>Assert.True(Math.Abs(Quaternion.Dot(a,b))>.99999f,"Quaternion orientation differs");
    [Test] public static void AffineHierarchyWorldPoseAtomicPivotAndLayerTransport()
    {
        var access=new Poses();var child=GameObject.Resolve(access,3);
        var childPose=access.Values[3];var expectedRotation=Quaternion.Normalize(access.Values[1].Rotation*access.Values[2].Rotation*childPose.Rotation);
        Near(child.TransformDirection(Vector3.UnitZ),Vector3.Transform(Vector3.UnitZ,expectedRotation));
        var point=new Vector3(.2f,3,-1);Near(child.InverseTransformPoint(child.TransformPoint(point)),point);
        Near(child.InverseTransformVector(child.TransformVector(point)),point);
        Near(child.InverseTransformDirection(child.TransformDirection(point)),point);
        var matrix=Matrix4x4.Identity;
        for(var id=3;id>=1;--id){var p=access.Values[id];matrix*=Matrix4x4.CreateScale(p.Scale)*Matrix4x4.CreateFromQuaternion(p.Rotation)*Matrix4x4.CreateTranslation(p.Position);}
        Near(child.WorldPosition,matrix.Translation);
        child.WorldPosition=new(11,4,-3);Near(child.WorldPosition,new(11,4,-3));
        Near(access.Values[3].Scale,childPose.Scale);Rotation(access.Values[3].Rotation,childPose.Rotation);
        var desired=Quaternion.CreateFromAxisAngle(Vector3.UnitZ,1.2f);child.WorldRotation=desired;Rotation(child.WorldRotation,desired);
        Near(child.WorldPosition,new(11,4,-3));
        var pivot=new Vector3(2,1,0);var before=child.WorldPosition;var angle=Quaternion.CreateFromAxisAngle(Vector3.UnitY,.5f);var writes=access.Writes;
        child.RotateAround(pivot,Vector3.UnitY,.5f);Near(child.WorldPosition,pivot+Vector3.Transform(before-pivot,angle));
        Assert.Equal(writes+1,access.Writes,"Pivot position+rotation is one native write");Rotation(child.WorldRotation,angle*desired);
        child.Translate(new(1,0,0),TransformSpace.World);var position=child.WorldPosition;
        child.LookAt(position+new Vector3(2,1,4));Near(child.Forward,Vector3.Normalize(new Vector3(2,1,4)));
        child.Layer=7;Assert.Equal(7u,child.Layer);Assert.Throws<ArgumentOutOfRangeException>(()=>child.Layer=32);
        access.Owned=true;var unchanged=access.Values[3];
        Assert.Throws<WorldException>(()=>child.RotateAround(pivot,Vector3.UnitX,.2f));Assert.Equal(unchanged,access.Values[3]);
        access.Owned=false;access.Values[2]=access.Values[2] with {Scale=new(0,2,1)};
        Assert.Throws<InvalidOperationException>(()=>child.WorldPosition=Vector3.Zero);
        access.Values[1]=access.Values[1] with {Scale=new(1e30f)};access.Values[2]=access.Values[2] with {Scale=new(1e30f)};
        Assert.Throws<InvalidOperationException>(()=>{_ = child.LocalToWorldMatrix;});
    }
}
