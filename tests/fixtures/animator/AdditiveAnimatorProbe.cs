using System;
using System.Numerics;
using Astra;

[ComponentId("acceptance.animator")]
public sealed class AdditiveAnimatorProbe : Behavior
{
    private GameObject? a,b,pa,pb;
    private Quaternion baseline,half,full;
    private AssetGuid reference;
    private float authoredWeight,authoredTime;
    private int phase;
    private static bool Same(Quaternion x,Quaternion y) => Math.Abs(Quaternion.Dot(x,y))>.9999f;
    private void Check(string name,bool ok) => Scene.Log(ObjectId,$"ADDITIVE {name} {(ok?"PASS":"FAIL")}");
    public override void Start()
    {
        a=Object.FindInWorld("Mecanismo A");b=Object.FindInWorld("Mecanismo B");pa=a!.Find("Painel");pb=b!.Find("Painel");
        reference=a.Animator().GetLayerReferenceClip(1);
        authoredWeight=a.Animator().GetLayerWeight(1);authoredTime=a.Animator().GetLayerReferenceTime(1);
        Check("shared composition API",reference.IsValid&&a.Animator().GetLayerBlendMode(1)==AnimationLayerBlendMode.Additive&&authoredWeight>0&&authoredWeight<1&&b.Animator().GetLayerWeight(1)==authoredWeight);
    }
    public override void Update(float dt)
    {
        var ga=a!.Animator();var gb=b!.Animator();var t=Time.TimeSinceStart;
        if(phase==0&&t>=.75f) {phase=1;half=pb!.LocalTransform.Rotation;ga.SetLayerWeight(1,0);}
        else if(phase==1&&t>=1.5f) {phase=2;baseline=pa!.LocalTransform.Rotation;Check("zero removes delta",!Same(baseline,half)&&Same(pb!.LocalTransform.Rotation,half));ga.SetLayerWeight(1,1);}
        else if(phase==2&&t>=2.25f) {
            phase=3;full=pa!.LocalTransform.Rotation;
            Check("weighted relative quaternion",Same(Quaternion.Slerp(baseline,full,authoredWeight),half)&&!Same(full,half));
            Check("instance isolation",gb.GetLayerWeight(1)==authoredWeight&&Same(pb!.LocalTransform.Rotation,half));ga.SetLayerReferenceClip(1,default);
        }
        else if(phase==3&&t>=3f) {
            phase=4;Check("initial pose reference",!Same(pa!.LocalTransform.Rotation,full)&&!ga.GetLayerReferenceClip(1).IsValid);
            ga.SetLayerReferenceClip(1,reference);ga.SetLayerReferenceTime(1,.25f);ga.SetLayerBlendMode(1,AnimationLayerBlendMode.Override);
        }
        else if(phase==4&&t>=3.75f) {
            phase=5;Check("mode and reference controls",Same(pa!.LocalTransform.Rotation,full)&&ga.GetLayerBlendMode(1)==AnimationLayerBlendMode.Override&&ga.GetLayerReferenceClip(1)==reference&&ga.GetLayerReferenceTime(1)==.25f);
            try {ga.SetLayerReferenceClip(1,new AssetGuid(42,42));Check("missing reference rejected",false);}
            catch(WorldException e) {Check("missing reference rejected",e.Status==WorldStatus.UnknownResource);}
            ga.ResetLayerOverrides(1);
        }
        else if(phase==5&&t>=4.5f) {
            phase=6;Check("reset authored composition",ga.GetLayerWeight(1)==authoredWeight&&ga.GetLayerBlendMode(1)==AnimationLayerBlendMode.Additive&&ga.GetLayerReferenceClip(1)==reference&&ga.GetLayerReferenceTime(1)==authoredTime&&Same(pa!.LocalTransform.Rotation,half)&&Same(pb!.LocalTransform.Rotation,half));
        }
    }
}
