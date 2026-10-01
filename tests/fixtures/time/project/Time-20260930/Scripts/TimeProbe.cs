using System;
using Astra;
using Astra.Components;

[ComponentId("acceptance.time")]
public sealed class TimeProbe : Behavior
{
    private Component scaled, unscaled;
    private uint scaledCount, unscaledCount, fixedCount;
    private double phaseStarted, frozenTime;
    private float frameDelta;
    private int phase;
    private void Fail(string reason) { phase=-1; Scene.Log(ObjectId,"TIME FAIL: "+reason); throw new InvalidOperationException(reason); }
    public override void Start()
    {
        foreach(var component in Object.Components())
            if(component.TypeId==ComponentIds.Timer) {
                if(component.GetBool("ignore_time_scale")) unscaled=component; else scaled=component;
            }
        if(scaled.InstanceId==0 || unscaled.InstanceId==0) { Fail("author one scaled and one unscaled Timer on this object"); return; }
        scaled.SetBool("enabled",false); unscaled.SetBool("enabled",false);
        if(Math.Abs(scaled.GetFloat("interval_seconds")-.1f)>.0001f || Math.Abs(unscaled.GetFloat("interval_seconds")-.1f)>.0001f)
            { Fail("both authored intervals must be 0.1 seconds"); return; }
        Time.Scale=0;
        Scene.Log(ObjectId,"TIME READY: native scale=0; waiting for dispatched frames");
    }
    public override void Update(float delta)
    {
        if(phase<0 || phase==3) return;
        frameDelta=delta;
        if(phase==0) {
            if(delta!=0 || Time.Scale!=0) { Fail("first dispatched frame did not freeze simulation"); return; }
            phaseStarted=Time.UnscaledTimeSinceStart; frozenTime=Time.TimeSinceStart;
            scaled!.SetBool("enabled",true); unscaled!.SetBool("enabled",true); phase=1;
        } else if(phase==1 && Time.UnscaledTimeSinceStart-phaseStarted>=.35) {
            if(Time.TimeSinceStart!=frozenTime || scaledCount!=0 || fixedCount!=0 || unscaledCount<2)
                { Fail("freeze contract: scaled="+scaledCount+" unscaled="+unscaledCount+" fixed="+fixedCount); return; }
            unscaled!.SetBool("enabled",false);
            Time.Scale=.5f; phaseStarted=Time.UnscaledTimeSinceStart; phase=2;
            Scene.Log(ObjectId,"TIME FROZEN: native simulation and FixedUpdate stopped; unscaled Timer fired="+unscaledCount);
        } else if(phase==2 && Time.UnscaledTimeSinceStart-phaseStarted>=.4) {
            double real=Time.UnscaledTimeSinceStart-phaseStarted, simulation=Time.TimeSinceStart-frozenTime;
            if(scaledCount<1 || fixedCount<1 || simulation<=0 || simulation>real*.5+.0001)
                { Fail("resume contract: scaled="+scaledCount+" fixed="+fixedCount+" simulation="+simulation+" real="+real); return; }
            scaled!.SetBool("enabled",false); Time.Scale=1; phase=3;
            Scene.Log(ObjectId,"TIME PASS: resumed half-speed; scaled="+scaledCount+" fixed="+fixedCount+" simulation="+simulation+" unscaled="+real);
        }
    }
    public override void FixedUpdate(float delta)
    {
        if(phase==1) { Fail("FixedUpdate dispatched while frozen"); return; }
        if(phase==2) {
            if(Math.Abs(Time.UnscaledDeltaTime-delta*2)>.0001f) { Fail("physical interval did not use captured half-speed scale"); return; }
            ++fixedCount;
        }
    }
    public override void TimerElapsed(ulong instance,uint count)
    {
        if(instance==scaled.InstanceId) scaledCount+=count;
        else if(instance==unscaled.InstanceId) unscaledCount+=count;
    }
    public override void LateUpdate(float delta)
    {
        if(phase>0 && phase<3 && (delta!=frameDelta || Time.DeltaTime!=frameDelta)) Fail("Update/LateUpdate clocks diverged");
    }
}
