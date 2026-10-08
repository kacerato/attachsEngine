using System;
using System.Numerics;
using Astra;
using Astra.Components;

// Aceite no aparelho do bloco I: Animator no Fox (Survey/Walk/Run) com mistura
// 1D por Velocidade, gatilho Assustar vindo de Qualquer estado, retorno por
// tempo de saída e eventos de estado pela fachada gerada.
[ComponentId("acceptance.animator")]
public sealed class AnimatorProbe : Behavior
{
    private GameObject? fox, head;
    private int phase, entered, marks;
    private Quaternion idlePose;
    private bool sawLook, done;

    private void Log(string message) => Scene.Log(ObjectId, "ANIM " + message);
    private static string Pass(bool ok) => ok ? "PASS" : "FAIL";

    public override void Start()
    {
        fox = Object.FindInWorld("Fox");
        head = Object.FindInWorld("b_Head_05");
        var component = fox!.GetComponent<Animator>()!.Value;
        component.OnStateEntered(this, _ => ++entered);
        component.OnStateEvent(this, e => { if (e[2].AsInteger() == 1) ++marks; });
        Log("start");
    }

    public override void Update(float deltaTime)
    {
        if (done) return;
        var t = Time.TimeSinceStart;
        var animator = fox!.Animator();
        if (phase == 0 && t >= .5)
        {
            phase = 1;
            idlePose = head!.LocalTransform.Rotation;
            var state = animator.GetCurrentState();
            Log($"padrao estado={state.Name} {Pass(state.Name == "Locomoção" && !state.IsInTransition)}");
            animator.SetFloat("Velocidade", 1.6f);
        }
        else if (phase == 1 && t >= 1.6)
        {
            phase = 2;
            var moved = Quaternion.Dot(idlePose, head!.LocalTransform.Rotation);
            Log($"mistura velocidade={animator.GetFloat("Velocidade"):F1} pose-mudou={Math.Abs(moved) < .9999f} {Pass(Math.Abs(moved) < .9999f)}");
            try { animator.SetInteger("Velocidade", 2); Log("tipo errado aceito FAIL"); }
            catch (WorldException) { Log("tipo errado recusado PASS"); }
            animator.SetTrigger("Assustar");
        }
        else if (phase == 2 && t < 2.6)
        {
            var state = animator.GetCurrentState();
            if (state.Name == "Olhar" || state.NextName == "Olhar") sawLook = true;
        }
        else if (phase == 2 && t >= 5)
        {
            phase = 3; done = true;
            var state = animator.GetCurrentState();
            Log($"gatilho olhou={sawLook} voltou={state.Name} entradas={entered} {Pass(sawLook && state.Name == "Locomoção" && entered >= 3)}");
            Log($"eventos marca1={marks} {Pass(marks >= 1)}");
        }
    }
}
