using System;
using Astra;
using Astra.Components;
[ComponentId("acceptance.number.tween")]
public sealed class NumberTweenProbe : Behavior
{
    private Light light;private Component target;private NumberTween? track;private double start;private int phase;
    public override void Start()
    {
        foreach(var component in Object.Components())if(component.TypeId==Light.TypeId){target=component;light=new(component);}
        light.Intensity=0;
        track=target.TweenFloat("intensity",8,.2f,ignoreTimeScale:true);track.Pause();
        try{using var duplicate=target.TweenFloat("intensity",4,1);throw new Exception("duplicate accepted");}
        catch(WorldException error)when(error.Status==WorldStatus.PropertyAlreadyTweening){}
        start=Time.UnscaledTimeSinceStart;
        Scene.Log(ObjectId,"NUMBER TWEEN READY: native light track paused");
    }
    public override void Update(float delta)
    {
        if(track is null)return;
        if(phase==0&&Time.UnscaledTimeSinceStart-start>.2){
            if(light.Intensity!=0||track.State.ElapsedSeconds!=0)throw new Exception("pause moved light");
            track.Resume();phase=1;
        }else if(phase==1&&track.State.Status==NumberTweenStatus.Completed){
            if(light.Intensity!=8||track.State.LastWrittenValue!=8)throw new Exception("endpoint did not reach component");
            track.Dispose();track=target.TweenFloat("intensity",0,1,ignoreTimeScale:true);phase=2;
        }else if(phase==2&&track.State.ElapsedSeconds>.05){
            track.Cancel();start=Time.UnscaledTimeSinceStart;phase=3;
        }else if(phase==3&&Time.UnscaledTimeSinceStart-start>.2){
            if(track.State.Status!=NumberTweenStatus.Cancelled||light.Intensity!=track.State.LastWrittenValue)throw new Exception("cancel overwrote light");
            track.Dispose();track=null;phase=4;
            Scene.Log(ObjectId,"NUMBER TWEEN PASS: pause, unscaled resume, real light, completion, cancel and release");
        }
    }
    public override void Destroy(){track?.Dispose();track=null;}
}
