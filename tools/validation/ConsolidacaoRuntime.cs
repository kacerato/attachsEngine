using System;
using System.Numerics;
using Astra;
[ComponentId("acceptance.animator")]
public sealed class HierarchyAnimatorProbe : Behavior {
    GameObject? a,b; int phase; Quaternion first;
    void Check(string name,bool ok) { Scene.Log(ObjectId,$"CONSOLIDATION {name} {(ok?"PASS":"FAIL")}"); if(!ok) throw new InvalidOperationException(name); }
    public override void Start() { a=Object.FindInWorld("Mecanismo A"); b=Object.FindInWorld("Mecanismo B"); }
    public override void Update(float dt) {
        double t=Time.TimeSinceStart;
        if(phase==0&&t>.4f) {
            var ta=a!.LocalTransform; var tb=b!.LocalTransform;
            Check("Animator consumes consolidated translation",Math.Abs(ta.Position.Y-20)<.001f);
            Check("Animator consumes consolidated scale",Vector3.Distance(ta.Scale,new Vector3(1.5f))<.001f);
            Check("shared controller independent instance",Math.Abs(tb.Position.Y)<.001f&&Vector3.Distance(tb.Scale,Vector3.One)<.001f);
            Check("state clip override",a.Animator().GetCurrentState().Path=="Fechada");
            first=ta.Rotation;phase=1;
        } else if(phase==1&&t>.65f) {
            Check("consolidated rotation advances",Math.Abs(Quaternion.Dot(first,a!.LocalTransform.Rotation))<.98f);phase=2;
        }
    }
}