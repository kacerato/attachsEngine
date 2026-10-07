using System;
using Astra;

// Aceite no aparelho do Material físico: duas bolas iguais (restituição 0,9)
// caem num chão sem quique. "Borracha" combina restituição pelo Máximo e
// quica; "Massa" combina pelo Mínimo e para no chão. A sonda só observa.
[ComponentId("acceptance.physics_material")]
public sealed class MaterialProbe : Behavior
{
    private GameObject? bouncy, dull;
    private bool landedBouncy, landedDull, done;
    private float bestBouncy, bestDull;

    private void Log(string message) => Scene.Log(ObjectId, "MATERIAL " + message);

    public override void Start()
    {
        bouncy = Object.FindInWorld("Bola de borracha");
        dull = Object.FindInWorld("Bola de massa");
        Log($"start bouncy={(bouncy != null)} dull={(dull != null)}");
    }

    private static void Track(GameObject? o, ref bool landed, ref float best)
    {
        if (o == null) return;
        var y = o.LocalTransform.Position.Y;
        if (y < .6f) landed = true;
        if (landed && y > best) best = y;
    }

    public override void Update(float deltaTime)
    {
        if (done) return;
        Track(bouncy, ref landedBouncy, ref bestBouncy);
        Track(dull, ref landedDull, ref bestDull);
        if (Time.TimeSinceStart < 4) return;
        done = true;
        var pass = landedBouncy && landedDull && bestBouncy > 1.2f && bestDull < .6f;
        Log($"borracha={bestBouncy:F2} massa={bestDull:F2} {(pass ? "PASS" : "FAIL")}");
    }
}
