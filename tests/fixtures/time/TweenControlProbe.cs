using System;
using Astra;
using Astra.Components;
[ComponentId("acceptance.tween.controls")]
public sealed class TweenControlProbe : Behavior
{
    private TransformTween tween;private double phaseStart;private float origin,frozen;private int phase;
    public override void Start()
    {
        foreach(var c in Object.Components())if(c.TypeId==TransformTween.TypeId)tween=new(c);
        if(tween.InstanceId==0||tween.Autoplay||tween.State().Status!=TweenRuntimeStatus.Idle)throw new InvalidOperationException("Expected manual tween");
        origin=Transform.Position.X;tween.Pause();tween.Restart();
        if(!tween.State().Paused)throw new InvalidOperationException("Restart lost pause");
        Scene.Log(ObjectId,"TWEEN CONTROL READY: manual evaluator paused before restart");
    }
    public override void Update(float delta)
    {
        if(phase==0){phaseStart=Time.UnscaledTimeSinceStart;phase=1;}
        var state=tween.State();
        if(phase==1&&Time.UnscaledTimeSinceStart-phaseStart>.2){
            if(state.ElapsedSeconds!=0||Transform.Position.X!=origin)throw new InvalidOperationException("Paused tween moved");
            tween.Resume();phase=2;
        }else if(phase==2&&state.ElapsedSeconds>.1){
            frozen=Transform.Position.X;if(frozen<=origin)throw new InvalidOperationException("Resume did not write pose");
            tween.Cancel();phaseStart=Time.UnscaledTimeSinceStart;phase=3;
        }else if(phase==3&&Time.UnscaledTimeSinceStart-phaseStart>.2){
            if(state.Status!=TweenRuntimeStatus.Cancelled||Transform.Position.X!=frozen)throw new InvalidOperationException("Cancel did not preserve pose");
            tween.Restart();if(tween.State().ElapsedSeconds!=0)throw new InvalidOperationException("Restart did not reset elapsed");phase=4;
        }else if(phase==4&&state.Status==TweenRuntimeStatus.Completed){
            if(Math.Abs(Transform.Position.X-4)>.001f)throw new InvalidOperationException("Tween endpoint mismatch");
            phase=5;Scene.Log(ObjectId,"TWEEN CONTROL PASS: ABI snapshot, pause/restart, resume, real pose, cancel and completion");
        }
    }
}
