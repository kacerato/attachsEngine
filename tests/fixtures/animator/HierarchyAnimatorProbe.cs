using System;
using System.Numerics;
using Astra;
using Astra.Components;

[ComponentId("acceptance.animator")]
public sealed class HierarchyAnimatorProbe : Behavior
{
    private GameObject? a,b,panelA,panelB;
    private int phase,entered,exited,interrupted;
    private Quaternion closed,previous;
    private bool continuity=true;
    private float maxStep;
    private const string Half="Mecanismo/Ciclo/Fase/Meia", Open="Mecanismo/Ciclo/Fase/Aberta";
    private void Check(string name,bool ok) => Scene.Log(ObjectId,$"HIERARCHY {name} {(ok?"PASS":"FAIL")}");
    private static bool Same(Quaternion x,Quaternion y) => Math.Abs(Quaternion.Dot(x,y))>.9999f;
    public override void Start()
    {
        a=Object.FindInWorld("Mecanismo A");b=Object.FindInWorld("Mecanismo B");
        panelA=a!.Find("Painel");panelB=b!.Find("Painel");
        var component=a.GetComponent<Animator>()!.Value;
        Check("shared source",component.GetController().IsValid&&component.GetController()==b.GetComponent<Animator>()!.Value.GetController());
        component.OnMachineEntered(this,_=>++entered);
        component.OnMachineExited(this,_=>++exited);
        component.OnTransitionInterrupted(this,_=>++interrupted);
    }
    public override void Update(float dt)
    {
        var ga=a!.Animator();var gb=b!.Animator();var t=Time.TimeSinceStart;
        var q=panelA!.LocalTransform.Rotation;
        if(phase>=4&&phase<=6) {
            var degrees=2*MathF.Acos(Math.Clamp(Math.Abs(Quaternion.Dot(previous,q)),0,1))*180/MathF.PI;
            maxStep=Math.Max(maxStep,degrees);continuity&=degrees<=Math.Max(2,dt*160+2);
        }
        previous=q;
        if(phase==0&&t>=.5f) {
            phase=1;closed=q;Check("root default",ga.GetCurrentState().Path=="Fechada"&&gb.GetCurrentState().Path=="Fechada");ga.Play("Mecanismo");
        } else if(phase==1&&t>=.9f) {
            phase=2;var info=ga.GetCurrentState();
            Check("three-level group entry",info.Path==Half&&info.LeafName=="Meia"&&info.IsInMachine("Mecanismo/Ciclo")&&!Same(q,closed)&&entered==3);
            Check("independent instance",gb.GetCurrentState().Path=="Fechada"&&Same(panelB!.LocalTransform.Rotation,closed));
            ga.SetBool("Alternativa",true);ga.Play("Mecanismo");
        } else if(phase==2&&t>=1.3f) {
            phase=3;Check("conditional entry",ga.GetCurrentState().Path==Open);ga.SetBool("Alternativa",false);ga.Play("Fechada");ga.CrossFade(Half,1);
        } else if(phase==3&&t>=1.7f) {
            phase=4;Check("crossfade path",ga.GetCurrentState().NextPath==Half&&ga.GetCurrentState().IsInTransition);ga.CrossFade(Open,1);
        } else if(phase==4&&t>=2.1f) {
            phase=5;Check("first interruption",ga.GetCurrentState().NextPath==Open&&interrupted>=1);ga.CrossFade("Fechada",1);
        } else if(phase==5&&t>=3.3f) {
            phase=6;Check($"repeated pose continuity max-step={maxStep:F2}",continuity&&interrupted>=2&&ga.GetCurrentState().Path=="Fechada"&&Same(q,closed));
            Check("fade leaves groups",exited>=3&&gb.GetCurrentState().Path=="Fechada");ga.SetTrigger("Acionar");
        } else if(phase==6&&t>=3.65f) {
            phase=7;Check("authored group transition",ga.GetCurrentState().NextPath==Half&&!ga.GetBool("Acionar"));ga.SetTrigger("Avançar");
        } else if(phase==7&&t>=3.9f) {
            phase=8;Check("next source interruption",ga.GetCurrentState().NextPath==Open&&!ga.GetBool("Avançar"));
        } else if(phase==8&&t>=5.1f) {
            phase=9;Check("interrupted destination",ga.GetCurrentState().Path==Open);ga.SetTrigger("Avançar");
        } else if(phase==9&&t>=5.8f) {
            phase=10;Check("three-level Exit route",ga.GetCurrentState().Path=="Fechada"&&Same(q,closed));ga.Play(Half);ga.SetTrigger("Emergência");
        } else if(phase==10&&t>=6.5f) {
            phase=11;Check("global Any State",ga.GetCurrentState().Path=="Fechada"&&!ga.GetBool("Emergência"));
            Check("isolation and restoration",gb.GetCurrentState().Path=="Fechada"&&Same(panelB!.LocalTransform.Rotation,closed));
        }
    }
}
