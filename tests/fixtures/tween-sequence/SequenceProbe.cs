using System;
using System.Numerics;
using Astra;
using Astra.Components;

// Aceite no aparelho do bloco D: a Sequência de tweens autorada (Porta, depois
// Luz e Placa juntas após 0,3 s, duas passagens) avisa cada etapa e o fim; a
// sonda só observa, sem mover nada.
[ComponentId("acceptance.tween_sequence")]
public sealed class SequenceProbe : Behavior
{
    private GameObject? door, lamp, sign;
    private int steps;
    private bool done;

    private void Log(string message) => Scene.Log(ObjectId, "SEQUENCE " + message);
    private static float Y(GameObject? o) => o?.LocalTransform.Position.Y ?? float.NaN;
    private string Poses() => $"porta={Y(door):F2} luz={Y(lamp):F2} placa={Y(sign):F2}";

    public override void Start()
    {
        door = Object.FindInWorld("Porta");
        lamp = Object.FindInWorld("Luz do corredor");
        sign = Object.FindInWorld("Placa");
        var sequence = Object.GetComponent<TweenSequence>()!.Value;
        sequence.OnStepStarted(this, e => { ++steps; Log($"step={e[0].AsInteger()} t={Time.TimeSinceStart:F2} {Poses()}"); });
        sequence.OnCompleted(this, _ =>
        {
            done = true;
            // Porta sobe 1 por passagem relativa; luz e placa param no destino absoluto.
            var pass = steps == 6 && Y(lamp) > .99f && Y(sign) > .99f;
            Log($"completed steps={steps} {Poses()} {(pass ? "PASS" : "FAIL")}");
        });
        Log("start " + Poses());
    }

    public override void Update(float deltaTime)
    {
        if (!done && Time.TimeSinceStart > 10) { done = true; Log($"FAIL timeout steps={steps} {Poses()}"); }
    }
}
