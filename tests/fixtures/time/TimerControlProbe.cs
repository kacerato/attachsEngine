using System;
using Astra;
using Astra.Components;
[ComponentId("acceptance.timer.controls")]
public sealed class TimerControlProbe : Behavior
{
    private GameTimer timer;private double phaseStart,remaining;private int phase;private uint events;
    public override void Start() {
        foreach(var component in Object.Components())if(component.TypeId==GameTimer.TypeId)timer=new(component);
        if(timer.InstanceId==0||timer.AutoStart||timer.State().Running)throw new InvalidOperationException("Expected manual timer");
        timer.Pause();timer.Start(.15f);var state=timer.State();
        if(!state.Running||!state.Paused)throw new InvalidOperationException("Start must preserve pause");remaining=state.RemainingSeconds;
        Scene.Log(ObjectId,"TIMER CONTROL READY: native manual countdown paused after Start");
    }
    public override void Update(float delta) {
        if(phase==0){phaseStart=Time.UnscaledTimeSinceStart;phase=1;}
        if(phase==1&&Time.UnscaledTimeSinceStart-phaseStart>.2){
            if(timer.State().RemainingSeconds!=remaining||events!=0)throw new InvalidOperationException("Paused timer advanced");
            timer.Resume();phase=2;
        } else if(phase==3&&Time.UnscaledTimeSinceStart-phaseStart>.2){
            var state=timer.State();if(state.Running||state.RemainingSeconds!=0||events!=1)throw new InvalidOperationException("Stop restarted or emitted timeout");
            phase=4;Scene.Log(ObjectId,"TIMER CONTROL PASS: snapshot, paused Start, Resume, one-shot, Start/Stop and no extra timeout");
        }
    }
    public override void TimerElapsed(ulong instance,uint count) {
        if(instance!=timer.InstanceId)return;events+=count;var state=timer.State();
        if(phase!=2||count!=1||!state.Completed||state.Running||state.RemainingSeconds!=0)throw new InvalidOperationException("Completion snapshot/order mismatch");
        timer.Start(.1f);timer.Stop();phaseStart=Time.UnscaledTimeSinceStart;phase=3;
    }
}
