using System;
using System.Numerics;
using Astra;
using Astra.Components;

// Aceite no aparelho do bloco G: Cérebro com corte inicial, órbita bloqueada
// por uma parede real (desoclusão), troca por prioridade com transição suave,
// eventos do Cérebro e da câmera virtual, lente da câmera virtual na Câmera.
[ComponentId("acceptance.virtual_camera")]
public sealed class VirtualCameraProbe : Behavior
{
    private GameObject? player, main, orbit, aerial;
    private bool cutAtStart, aerialActivated, aerialFinished, orbitDeactivated, sawBlend, done;
    private int phase;

    private void Log(string message) => Scene.Log(ObjectId, "VCAM " + message);
    private GameObject? Find(string name) => Object.FindInWorld(name);
    private static string Pass(bool ok) => ok ? "PASS" : "FAIL";

    public override void Start()
    {
        player = Find("Jogador"); main = Find("Câmera principal"); orbit = Find("Câmera de órbita"); aerial = Find("Câmera aérea");
        // Cenário visível: chão, o jogador e uma parede entre ele e a órbita.
        var floor = Object.CreatePrimitive(PrimitiveType.Plane);
        floor.LocalTransform = new TransformValue(new Vector3(0, 0, 0), Quaternion.Identity, new Vector3(12, 1, 12));
        var body = player!.CreatePrimitive(PrimitiveType.Capsule);
        body.LocalTransform = new TransformValue(new Vector3(0, 1, 0), Quaternion.Identity, Vector3.One);
        var wall = Object.CreatePrimitive(PrimitiveType.Cube);
        wall.LocalTransform = new TransformValue(new Vector3(0, 1.5f, -3.5f), Quaternion.Identity, new Vector3(4, 3, .4f));
        for (var i = 0; i < 6; ++i)
        {
            var pillar = Object.CreatePrimitive(PrimitiveType.Cylinder);
            var angle = i * MathF.PI / 3;
            pillar.LocalTransform = new TransformValue(new Vector3(MathF.Cos(angle) * 9, 1.5f, MathF.Sin(angle) * 9), Quaternion.Identity, new Vector3(.8f, 3, .8f));
        }
        var brain = main!.GetComponent<CameraBrain>()!.Value;
        brain.OnCameraCut(this, e => cutAtStart = e.GetObject(0).ObjectId == orbit!.ObjectId);
        brain.OnCameraActivated(this, e =>
        {
            if (e.GetObject(0).ObjectId == aerial!.ObjectId && e.GetObject(1).ObjectId == orbit!.ObjectId) aerialActivated = true;
        });
        brain.OnBlendFinished(this, e => { if (e.GetObject(0).ObjectId == aerial!.ObjectId) aerialFinished = true; });
        orbit!.GetComponent<VirtualCamera>()!.Value.OnDeactivated(this, e => orbitDeactivated = e.GetObject(0).ObjectId == aerial!.ObjectId);
        Log("start");
    }

    public override void Update(float deltaTime)
    {
        if (done) return;
        var t = Time.TimeSinceStart;
        var brain = main!.GetComponent<CameraBrain>()!.Value;
        var camera = main!.LocalTransform.Position;
        if (phase == 0 && t >= 1.5)
        {
            phase = 1;
            var distance = Vector3.Distance(camera, new Vector3(0, 1, 0));
            var live = brain.LiveCamera().ObjectId == orbit!.ObjectId;
            Log($"orbita ao vivo={live} corte={cutAtStart} distancia={distance:F2} {Pass(live && cutAtStart && distance > 2.4f && distance < 3.6f)}");
            var promoted = aerial!.GetComponent<VirtualCamera>()!.Value; promoted.Priority = 20;
        }
        else if (phase == 1 && t >= 2.3 && t < 2.9)
        {
            if (brain.Blending()) sawBlend = true;
        }
        else if (phase == 1 && t >= 3.6)
        {
            phase = 2;
            var lens = main.GetComponent<Camera>()!.Value.VerticalFov;
            var at = Vector3.Distance(camera, new Vector3(8, 10, 8));
            var live = brain.LiveCamera().ObjectId == aerial!.ObjectId;
            Log($"aerea ao vivo={live} transicao={sawBlend} ativada={aerialActivated} concluida={aerialFinished} saiu-orbita={orbitDeactivated} pose={at:F2} lente={lens:F1} {Pass(live && sawBlend && aerialActivated && aerialFinished && orbitDeactivated && at < .05f && Math.Abs(lens - 45) < .01f)}");
            var demoted = aerial.GetComponent<VirtualCamera>()!.Value; demoted.Priority = 0;
        }
        else if (phase == 2 && t >= 5.2)
        {
            phase = 3; done = true;
            var live = orbit!.GetComponent<VirtualCamera>()!.Value.IsLive();
            Log($"volta orbita ao vivo={live} lente={main.GetComponent<Camera>()!.Value.VerticalFov:F1} {Pass(live && !brain.Blending())}");
        }
    }
}
