using System;
using Astra;
using Astra.Components;

// A real Play Behavior, compiled by the editor and executed by CoreCLR on Android.
[ComponentId("acceptance.clip_cues")]
public sealed class ClipCuesProbe : Behavior
{
    private GameObject legacy = null!, graph = null!, legacyTarget = null!, graphTarget = null!;
    private int legacyCount, graphCount, savedLegacy, savedGraph, phase;
    private bool payloads = true;
    private AnimationState State => legacy.GetComponent<Animation>()!.Value.Component.Animation()["Ciclo de eventos"];
    private void Check(string name, bool passed) => Scene.Log(ObjectId, $"CUES {name} {(passed ? "PASS" : "FAIL")}");
    private void Event(bool isGraph, ComponentEventArgs e)
    {
        var tag = e[0].AsInteger();
        payloads &= e.Count == 3 && e.EmitterInstanceId != 0 && e[2].AsInteger() > 0 &&
                    ((tag == 7 && e[1].AsNumber() == 2.5) || (tag == 8 && e[1].AsNumber() == .125));
        if (isGraph) ++graphCount; else ++legacyCount;
        Scene.Log(ObjectId, $"CUES event {(isGraph ? "Animator" : "Animation")} tag={tag} value={e[1].AsNumber()} cue={e[2].AsInteger()}");
    }
    public override void Start()
    {
        legacy = Object.FindInWorld("Animation")!; graph = Object.FindInWorld("Animator")!;
        legacyTarget = Object.FindInWorld("Alvo Animation")!; graphTarget = Object.FindInWorld("Alvo Animator")!;
        legacy.GetComponent<Animation>()!.Value.OnClipEvent(this, e => Event(false, e));
        graph.GetComponent<Animator>()!.Value.OnClipEvent(this, e => Event(true, e));
        Scene.Log(ObjectId, "CUES READY");
    }
    public override void Update(float deltaTime)
    {
        double t = Time.TimeSinceStart;
        if (phase == 0 && t >= 1.2f)
        {
            Check("forward-payload-marker-isolation", legacyCount == 2 && graphCount == 2 && payloads);
            Check("filtered-scene-connections", !legacyTarget.ActiveSelf && !graphTarget.ActiveSelf);
            savedLegacy = legacyCount; savedGraph = graphCount;
            var state = State; state.Weight = 0;
            graph.Animator().SetLayerWeight(0, 0); phase = 1;
        }
        else if (phase == 1 && t >= 3.5f)
        {
            Check("zero-weight", legacyCount == savedLegacy && graphCount == savedGraph);
            var state = State; state.Time = 0; state.Weight = 1;
            graph.Animator().Play("Ciclo"); graph.Animator().SetLayerWeight(0, 1); phase = 2;
        }
        else if (phase == 2 && t >= 4.9f)
        {
            Check("restart-and-loop", legacyCount == savedLegacy + 2 && graphCount == savedGraph + 2 && payloads);
            savedLegacy = legacyCount; savedGraph = graphCount;
            var state = State; state.Speed = 0;
            var animator = graph.GetComponent<Animator>()!.Value; animator.Speed = 0; phase = 3;
        }
        else if (phase == 3 && t >= 5.7f)
        {
            Check("pause", legacyCount == savedLegacy && graphCount == savedGraph);
            var state = State; state.Speed = -1;
            var animator = graph.GetComponent<Animator>()!.Value; animator.Speed = -1; phase = 4;
        }
        else if (phase == 4 && t >= 7f)
        {
            Check("reverse", legacyCount == savedLegacy + 2 && graphCount == savedGraph + 2 && payloads);
            Scene.Log(ObjectId, $"CUES DONE Animation={legacyCount} Animator={graphCount}"); phase = 5;
        }
    }
}
