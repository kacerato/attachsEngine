using System;
using Astra;
using Astra.Components;

[ComponentId("acceptance.audio")]
public sealed class AudioProbe : Behavior
{
    private AudioSource source;
    private string waitReason = "";
    private double nextSearch, stageStarted, cursor;
    private int stage;
    private bool ready, finished;

    private void Log(string marker, string detail) => Scene.Log(ObjectId, marker + " " + detail);
    private void Wait(string detail)
    {
        if (waitReason == detail) return;
        waitReason = detail;
        Log("AUDIOWAIT", detail);
    }

    public override void Start() => Wait("finding project-authored Source + Listener + imported WAV; no objects or clips will be created");

    public override void Update(float dt)
    {
        if (finished) return;
        try
        {
            var now = Time.TimeSinceStart;
            if (!ready)
            {
                if (now < nextSearch) return;
                nextSearch = now + .5;
                FindAuthoredAudio(now);
                return;
            }
            if (!source.IsAlive) throw new Exception("authored source removed during acceptance");
            var voice = new AudioVoice(source.Component);
            var observed = voice.Snapshot;
            if ((int)observed.State >= (int)AudioVoiceState.MissingClip) throw new Exception("backend error: " + observed.State);
            if ((stage == 0 || stage == 2) && !observed.OutputRunning)
            { Wait("device output suspended/unavailable; backend cursor is not evidence of audibility"); return; }
            if (now - stageStarted < .6) return;
            if (stage == 0)
            {
                if (observed.State != AudioVoiceState.Playing || observed.Cursor <= .1) throw new Exception("Play did not advance real voice cursor");
                cursor = observed.Cursor; voice.Pause();
            }
            else if (stage == 1)
            {
                if (observed.State != AudioVoiceState.Paused || Math.Abs(observed.Cursor - cursor) > .15) throw new Exception("Pause failed to preserve backend cursor");
                cursor = observed.Cursor; voice.Play();
            }
            else if (stage == 2)
            {
                if (observed.State != AudioVoiceState.Playing || observed.Cursor <= cursor + .1) throw new Exception("Resume did not advance backend cursor");
                voice.Stop();
            }
            else
            {
                if (observed.State != AudioVoiceState.Stopped || Math.Abs(observed.Cursor) > .02) throw new Exception("Stop did not reset backend cursor");
                Log("AUDIOPASS", "real voice play/pause/resume/stop and cursor observed; seek unsupported; audibility requires separate human evidence");
                finished = true; return;
            }
            ++stage; stageStarted = now;
        }
        catch (Exception failure)
        {
            Log("AUDIOFAIL", failure.GetType().Name + ": " + failure.Message);
            finished = true;
        }
    }

    private void FindAuthoredAudio(double now)
    {
        var root = Object;
        while (root.Parent is GameObject parent) root = parent;
        var listeners = root.GetComponentsInChildren<AudioListener>(true);
        AudioListener? listener = null;
        foreach (var candidate in listeners)
            if (candidate.Enabled && candidate.Object.ActiveInHierarchy && candidate.Volume > 0)
            { listener = candidate; break; }
        if (listener is null) { Wait("author an enabled active AudioListener with positive volume"); return; }
        var sources = root.GetComponentsInChildren<AudioSource>(true);
        foreach (var candidate in sources)
        {
            if (!candidate.Enabled || !candidate.Object.ActiveInHierarchy || candidate.Mute || candidate.Volume <= 0 || !candidate.GetClip().IsValid) continue;
            if (candidate.Loop || Math.Abs(candidate.Pitch - 1) > .001)
            { Wait("use a nonlooping WAV >=6s and pitch=1 for this command cycle"); continue; }
            source = candidate;
            Log("AUDIOREADY", "source=" + source.Object.ObjectId + " instance=" + source.InstanceId
                + " clip=" + source.GetClip() + " listener=" + listener.Value.Object.ObjectId
                + " dimension=" + source.Dimension + "; ABI23 observed voice available");
            source.Playback = AudioSource.PlaybackOption.Tocar;
            Log("AUDIOREADY", "Play requested; cursor must advance through backend, not this clock");
            ready = true; stage = 0; stageStarted = now;
            return;
        }
        Wait("author one enabled, unmuted Source with imported valid WAV, volume>0, loop=false, pitch=1");
    }
}
