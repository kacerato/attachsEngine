using System;
using Astra;
using Astra.Components;

// Aceite no aparelho do bloco H: buses reais com efeitos, envio para um bus de
// reverberação, ducking por sidechain, WAV longo em streaming e Snapshot.
[ComponentId("acceptance.audio_mixer")]
public sealed class AudioMixerProbe : Behavior
{
    private GameObject? music, voices, ambient, speech, pause;
    private int phase;
    private double maxReduction;
    private bool done;

    private void Log(string message) => Scene.Log(ObjectId, "MIX " + message);
    private GameObject? Find(string name) => Object.FindInWorld(name);
    private static string Pass(bool ok) => ok ? "PASS" : "FAIL";

    public override void Start()
    {
        music = Find("Música"); voices = Find("Falas"); ambient = Find("Ambiente");
        speech = Find("Fala"); pause = Find("Pausa");
        Log("start");
    }

    public override void Update(float deltaTime)
    {
        if (done) return;
        var t = Time.TimeSinceStart;
        var bus = music!.GetComponent<AudioBus>()!.Value;
        var duck = music.GetComponent<AudioCompressor>()!.Value;
        if (phase >= 1) maxReduction = Math.Max(maxReduction, duck.ReductionDb());
        if (phase == 0 && t >= 1.0)
        {
            phase = 1;
            var level = bus.PeakDb();
            var ambientLevel = ambient!.GetComponent<AudioBus>()!.Value.PeakDb();
            Log($"musica nivel={level:F1}dB reducao={duck.ReductionDb():F1}dB {Pass(level > -30 && duck.ReductionDb() < 1)}");
            Log($"streaming ambiente nivel={ambientLevel:F1}dB {Pass(ambientLevel > -50)}");
            speech!.GetComponent<AudioSource>()!.Value.Play();
        }
        else if (phase == 1 && t >= 2.2)
        {
            phase = 2;
            var voiceLevel = voices!.GetComponent<AudioBus>()!.Value.PeakDb();
            Log($"ducking fala={voiceLevel:F1}dB reducao-maxima={maxReduction:F1}dB {Pass(voiceLevel > -30 && maxReduction > 6)}");
            pause!.GetComponent<AudioSnapshot>()!.Value.TransitionTo(1);
        }
        else if (phase == 2 && t >= 3.6)
        {
            phase = 3; done = true;
            var cutoff = music.GetComponent<AudioFilter>()!.Value.Cutoff;
            Log($"snapshot ganho={bus.Volume:F2} corte={cutoff:F0}Hz {Pass(Math.Abs(bus.Volume - .3f) < .01f && Math.Abs(cutoff - 800) < 1)}");
        }
    }
}
