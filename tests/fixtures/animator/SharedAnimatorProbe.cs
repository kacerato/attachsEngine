using System;
using System.Numerics;
using Astra;
using Astra.Components;

[ComponentId("acceptance.animator")]
public sealed class SharedAnimatorProbe : Behavior
{
    private GameObject? a, b, panelA, panelB;
    private AssetGuid source;
    private int phase;
    private void Log(string text) => Scene.Log(ObjectId, "SHARED " + text);
    public override void Start()
    {
        a=Object.FindInWorld("Mecanismo A");b=Object.FindInWorld("Mecanismo B");
        panelA=a!.Find("Painel");panelB=b!.Find("Painel");
        source=a.GetComponent<Animator>()!.Value.GetController();
        var same=source==b.GetComponent<Animator>()!.Value.GetController();
        Log($"source={source.IsValid} same={same} no-skin {(source.IsValid&&same?"PASS":"FAIL")}");
    }
    public override void Update(float deltaTime)
    {
        var ga=a!.Animator();var gb=b!.Animator();var t=Time.TimeSinceStart;
        if(phase==0&&t>=.5f) {phase=1;ga.SetFloat("Abertura",1);gb.SetFloat("Abertura",1);}
        if(phase==1&&t>=1.5f) {
            phase=2;var different=Math.Abs(Quaternion.Dot(panelA!.LocalTransform.Rotation,panelB!.LocalTransform.Rotation))<.999f;
            Log($"override poses-different={different} {(different?"PASS":"FAIL")}");ga.SetFloat("Abertura",0);
        }
        if(phase==2&&t>=2.5f) {
            phase=3;var av=ga.GetFloat("Abertura");var bv=gb.GetFloat("Abertura");
            Log($"independent A={av:F2} B={bv:F2} {(av==0&&bv==1?"PASS":"FAIL")}");
            b.GetComponent<Animator>()!.Value.SetController(source);
            try {b.GetComponent<Animator>()!.Value.SetController(new AssetGuid(42,42));Log("missing accepted FAIL");}
            catch(WorldException e) {Log($"missing {e.Status} {(e.Status==WorldStatus.UnknownResource?"PASS":"FAIL")}");}
        }
        if(phase==3&&t>=3.5f) {
            phase=4;b.GetComponent<Animator>()!.Value.SetController(default);gb.SetFloat("Abertura",1);
        }
        if(phase==4&&t>=4.5f) {
            phase=5;var changed=Math.Abs(Quaternion.Dot(Quaternion.Identity,panelB!.LocalTransform.Rotation))<.999f;
            var detached=!b.GetComponent<Animator>()!.Value.GetController().IsValid;
            Log($"detach baked-pose={changed} local={detached} {(changed&&detached?"PASS":"FAIL")}");
            b.GetComponent<Animator>()!.Value.SetController(source);ga.SetFloat("Abertura",1);gb.SetFloat("Abertura",1);ga.Play("Abertura");gb.Play("Abertura");
        }
        if(phase==5&&t>=5.5f) {
            phase=6;var same=Math.Abs(Quaternion.Dot(panelA!.LocalTransform.Rotation,panelB!.LocalTransform.Rotation))>.999f;
            Log($"reassign clears-overrides={same} {(same?"PASS":"FAIL")}");
        }
    }
}
