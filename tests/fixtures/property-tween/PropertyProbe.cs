using System;
using Astra;
using Astra.Components;

// Aceite no aparelho do Tween de propriedade: "Acender palco" leva a
// intensidade da "Luz do palco" de 0 a 50 em 1 s; a Sequência "Show" tem uma
// etapa cujo objeto anda (Transform Tween) e acende (Tween de propriedade).
[ComponentId("acceptance.property_tween")]
public sealed class PropertyProbe : Behavior
{
    private GameObject? stage, spot;
    private bool middle, done;

    private void Log(string message) => Scene.Log(ObjectId, "PROPERTY " + message);
    private static float Intensity(GameObject? o) => o?.GetComponent<Light>()?.Intensity ?? float.NaN;

    public override void Start()
    {
        stage = Object.FindInWorld("Luz do palco");
        spot = Object.FindInWorld("Holofote");
        var show = Object.FindInWorld("Show")!.GetComponent<TweenSequence>()!.Value;
        show.OnCompleted(this, _ =>
        {
            var x = spot?.LocalTransform.Position.X ?? float.NaN;
            var pass = Math.Abs(x - 1) < .01f && Math.Abs(Intensity(spot) - 30) < .01f;
            Log($"show completed x={x:F2} holofote={Intensity(spot):F1} {(pass ? "STEP PASS" : "STEP FAIL")}");
        });
        Log($"start palco={Intensity(stage):F1}");
    }

    public override void Update(float deltaTime)
    {
        var t = Time.TimeSinceStart;
        if (!middle && t > .5) { middle = true; Log($"t={t:F2} palco={Intensity(stage):F1}"); }
        if (!done && t > 1.6)
        {
            done = true;
            var value = Intensity(stage);
            Log($"t={t:F2} palco={value:F1} {(Math.Abs(value - 50) < .01f ? "TWEEN PASS" : "TWEEN FAIL")}");
        }
    }
}
