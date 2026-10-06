using System;
using System.Numerics;
using Astra;
using Astra.Components;

// Aceite no aparelho dos blocos B, C e C2: o sensor dispara, a Conexão de
// evento toca o próprio som, a vista de jogo projeta um raio que acerta o alvo,
// o vibrador real é acionado e a cena é trocada por script. Nada é criado pela
// sonda: tudo vem da cena autorada.
[ComponentId("acceptance.services")]
public sealed class ServicesProbe : Behavior
{
    private int stage;
    private double started;
    private bool entered;

    private void Log(string message) => Scene.Log(ObjectId, "SERVICES " + message);

    public override void Start()
    {
        var collider = Object.GetComponent<Collider>()!.Value;
        collider.OnTriggerEnter(this, e => { entered = true; Log("sensor-enter other=" + e.GetObject(0).ObjectId); });
        Log("scene=" + Scenes.Active + " project=" + string.Join(",", Scenes.Names));
    }

    public override void Update(float deltaTime)
    {
        try
        {
            var now = Time.TimeSinceStart;
            if (stage == 0 && entered)
            {
                var voice = new AudioVoice(Object.GetComponent<AudioSource>()!.Value.Component);
                var observed = voice.Snapshot;
                Log($"audio state={observed.State} cursor={observed.Cursor:F2} output={observed.OutputRunning}");
                // Estado Playing sozinho não prova som: exige a saída do dispositivo
                // ligada e o cursor do backend avançando.
                if (observed.State == AudioVoiceState.Playing && observed.OutputRunning && observed.Cursor > .2)
                { Log("AUDIO PASS"); stage = 1; started = now; }
            }
            else if (stage == 1)
            {
                var view = View.State;
                Log($"view {view.Width}x{view.Height} dpi={view.Dpi} camera={view.CameraObjectId} safeReported={view.SafeAreaReported}");
                var ray = View.ScreenPointToRay(new Vector2(view.Width / 2, view.Height / 2));
                var hit = Physics.RayCast(ray.Origin, ray.Direction * 50);
                Log(hit is { } h ? $"RAY PASS hit={h.Object.Name} distance={h.Distance:F2}" : "RAY FAIL no hit");
                Debug.DrawRay(ray.Origin, ray.Direction * 12, new Color(1, .8f, 0), 4);
                if (Haptics.Available) { Haptics.Vibrate(300, 1); Log("HAPTICS requested 300ms"); }
                else Log("HAPTICS unavailable");
                stage = 2; started = now;
            }
            else if (stage == 2 && now - started > 6)
            {
                Log("loading Fase2");
                Scenes.Load("Fase2");
                stage = 3;
            }
        }
        catch (Exception error) { Log("FAIL " + error.Message); stage = 99; }
    }
}
