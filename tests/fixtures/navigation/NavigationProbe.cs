using System;
using System.Numerics;
using Astra;
using Astra.Components;

// Aceite no aparelho do bloco J (docs/planos/NAVEGACAO-2026-10-09.md): a malha
// é assada pela interface do editor; no Play, um Personagem persegue o Alvo
// contornando a parede (sem script), um agente de pose anda até um destino
// pedido pela API, desvia do Caixote recortado, chega (evento), é teleportado
// e recusa um destino fora da malha (evento e estado 3).
[ComponentId("acceptance.navigation")]
public sealed class NavigationProbe : Behavior
{
    private GameObject? chaser, walker, target, surface;
    private bool reached, failed, done;
    private int phase;
    private float bestChase = float.MaxValue;

    private void Log(string message) => Scene.Log(ObjectId, "NAV " + message);
    private static string Pass(bool ok) => ok ? "PASS" : "FAIL";
    private static float Flat(Vector3 a, Vector3 b) => Vector2.Distance(new Vector2(a.X, a.Z), new Vector2(b.X, b.Z));
    // Sob o Personagem a primitiva de script nasce só visual (sem corpo próprio).
    private static GameObject Visual(GameObject parent, PrimitiveType type) => parent.CreatePrimitive(type);

    public override void Start()
    {
        chaser = Object.FindInWorld("Perseguidor"); walker = Object.FindInWorld("Andarilho");
        target = Object.FindInWorld("Alvo"); surface = Object.FindInWorld("Superfície");
        // Visual do cenário: os colisores estáticos já existem na cena autorada.
        var floor = Visual(Object.FindInWorld("Chão")!, PrimitiveType.Plane);
        floor.LocalTransform = new TransformValue(new Vector3(0, .5f, 0), Quaternion.Identity, new Vector3(20, 1, 20));
        var wall = Visual(Object.FindInWorld("Parede")!, PrimitiveType.Cube);
        wall.LocalTransform = new TransformValue(Vector3.Zero, Quaternion.Identity, new Vector3(.8f, 2, 12));
        var crate = Visual(Object.FindInWorld("Caixote")!, PrimitiveType.Cube);
        crate.LocalTransform = new TransformValue(Vector3.Zero, Quaternion.Identity, new Vector3(2, 2, 2));
        // O Alvo fica sem primitiva: um corpo estático filho travaria a pose que a
        // sonda move na segunda fase (regra da engine para objetos livres).
        var body = Visual(chaser!, PrimitiveType.Capsule);
        body.LocalTransform = new TransformValue(new Vector3(0, 1, 0), Quaternion.Identity, Vector3.One);
        var runner = Visual(walker!, PrimitiveType.Cylinder);
        runner.LocalTransform = new TransformValue(new Vector3(0, .5f, 0), Quaternion.Identity, new Vector3(.8f, 1, .8f));

        var agent = walker.GetComponent<NavAgent>()!.Value;
        agent.OnDestinationReached(this, _ => reached = true);
        agent.OnPathFailed(this, _ => failed = true);
        Log("start");
    }

    public override void Update(float deltaTime)
    {
        if (done) return;
        var t = Time.TimeSinceStart;
        var agent = walker!.GetComponent<NavAgent>()!.Value;
        bestChase = MathF.Min(bestChase, Flat(chaser!.WorldTransform.Position, target!.WorldTransform.Position));
        if (phase == 0 && t >= .5)
        {
            phase = 1;
            var ready = surface!.GetComponent<NavSurface>()!.Value;
            var accepted = agent.SetDestination(new Vector3(7, 0, -7));
            Log($"malha pronta={ready.IsReady()} poligonos={ready.PolygonCount()} destino={accepted} {Pass(ready.IsReady() && ready.PolygonCount() > 0 && accepted)}");
        }
        else if (phase == 1 && t >= 8)
        {
            phase = 2;
            var at = Flat(walker.WorldTransform.Position, new Vector3(7, 0, -7));
            Log($"andarilho chegou evento={reached} distancia={at:F2} caminho={agent.HasPath()} {Pass(reached && at < .6f && !agent.HasPath())}");
            Log($"perseguidor menor distancia={bestChase:F2} {Pass(bestChase < 1.6f)}");
            // O alvo muda de lado: o perseguidor refaz o caminho sozinho.
            target.WorldTransform = new TransformValue(new Vector3(-7, 0, 7), Quaternion.Identity, Vector3.One);
            bestChase = float.MaxValue;
        }
        else if (phase == 2 && t >= 16)
        {
            phase = 3;
            Log($"perseguidor refez caminho menor distancia={bestChase:F2} {Pass(bestChase < 1.6f)}");
            var warped = agent.Warp(new Vector3(-7, 0, -7));
            var at = Flat(walker.WorldTransform.Position, new Vector3(-7, 0, -7));
            var refused = !agent.SetDestination(new Vector3(0, 50, 0));
            Log($"teleporte={warped} distancia={at:F2} recusa fora da malha={refused} estado={agent.PathStatus()} {Pass(warped && at < .3f && refused && agent.PathStatus() == 3)}");
        }
        else if (phase == 3 && t >= 16.5)
        {
            // Eventos chegam pela fila no quadro seguinte ao pedido.
            phase = 4; done = true;
            Log($"evento caminho falhou entregue={failed} {Pass(failed)}");
        }
    }
}
