using System;
using System.Numerics;
using Astra;
using Astra.Components;

[ComponentId("acceptance.animator")]
public sealed class GeneralAnimatorProbe : Behavior
{
    private GameObject? mechanism, body, panel;
    private Quaternion initial;
    private int phase;
    private bool priorManual;
    private float priorFloat;
    private void Log(string text) => Scene.Log(ObjectId, "UNIVERSAL " + text);
    public override void Start()
    {
        mechanism=Object.FindInWorld("Mecanismo"); body=Object.FindInWorld("Fonte física"); panel=Object.FindInWorld("Painel");
        initial=panel!.LocalTransform.Rotation;
        var component=mechanism!.GetComponent<Animator>()!.Value;
        component.OnStateEntered(this, e => Log($"estado={e[1].AsInteger()}"));
        component.OnStateEvent(this, e => Log($"evento={e[2].AsInteger()}"));
        Log("start mecanismo sem skin, fonte fisica independente");
    }
    public override void Update(float deltaTime)
    {
        var graph=mechanism!.Animator(); var t=Time.TimeSinceStart;
        if(phase==0 && t>=.5) { phase=1;body!.PhysicsBody().SetLinearAndAngularVelocity(new Vector3(1.5f,0,0),Vector3.Zero); }
        if(phase==1 && t>=2) {
            phase=2;var value=graph.GetFloat("Velocidade");var changed=Math.Abs(Quaternion.Dot(initial,panel!.LocalTransform.Rotation))<.9999f;
            Log($"medicao={value:F3} pose={changed} {(Math.Abs(value-1.5f)<.05f&&changed?"PASS":"FAIL")}");
            try { graph.SetFloat("Velocidade",0);Log("setter vinculado aceito FAIL"); }
            catch(WorldException e) {Log($"setter vinculado {e.Status} {(e.Status==WorldStatus.Rejected?"PASS":"FAIL")}");}
        }
        if(phase==2 && t>=4) {phase=3;body!.PhysicsBody().SetLinearAndAngularVelocity(Vector3.Zero,Vector3.Zero);}
        if(phase==3 && t>=6) {
            phase=4;var value=graph.GetFloat("Velocidade");var restored=Math.Abs(Quaternion.Dot(initial,panel!.LocalTransform.Rotation))>.9999f;
            Log($"parada={value:F3} restaurou={restored} {(Math.Abs(value)<.01f&&restored?"PASS":"FAIL")}");
        }
        var manualFloat=graph.GetFloat("Auxiliar 1");
        if(manualFloat!=priorFloat) {priorFloat=manualFloat;Log($"float vivo={manualFloat:F3} PASS");}
        var manual=graph.GetBool("Teste manual");
        if(manual!=priorManual) {priorManual=manual;Log($"manual vivo={manual} PASS");}
    }
}
