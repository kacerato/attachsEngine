using System;
using Astra;
using Astra.Components;
[ComponentId("acceptance.tween.connection")]
public sealed class TweenConnectionProbe : Behavior
{
    private TransformTween tween;private GameObject receiver;private int phase;private double started;
    public override void Start()
    {
        foreach(var c in Object.Components())if(c.TypeId==TransformTween.TypeId)tween=new(c);
        receiver=Resolve(tween.Component.GetReference("finished_target"))??throw new InvalidOperationException("Missing persistent receiver");
        if(receiver.ActiveSelf||tween.Component.GetEnum("finished_action")!=1)throw new InvalidOperationException("Expected inactive receiver and activation action");
        Scene.Log(ObjectId,"TWEEN CONNECTION READY: actual receiver inactive");
    }
    public override void Update(float delta)
    {
        if(phase==0&&tween.State().Status==TweenRuntimeStatus.Completed){
            if(!receiver.ActiveSelf||Math.Abs(Transform.Position.Y-2)>.001f)throw new InvalidOperationException("Completion did not apply final pose and receiver");
            receiver.SetActive(false);tween.Restart();tween.Cancel();started=Time.UnscaledTimeSinceStart;phase=1;
        }else if(phase==1&&Time.UnscaledTimeSinceStart-started>.3){
            if(receiver.ActiveSelf||tween.State().Status!=TweenRuntimeStatus.Cancelled)throw new InvalidOperationException("Cancellation fired completion");
            phase=2;Scene.Log(ObjectId,"TWEEN CONNECTION PASS: persistent native completion activated receiver after final pose; cancellation did not fire");
        }
    }
}
