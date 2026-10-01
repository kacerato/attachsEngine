using System;
using Astra;
using Astra.Components;

[ComponentId("acceptance.timer.connection")]
public sealed class TimerConnectionProbe : Behavior
{
    private GameObject receiver;
    private ulong timerId;
    public override void Start()
    {
        var found=Object.FindGameObjectsInGroup("timeout-receiver");
        if(found.Length!=1 || found[0].ActiveSelf) throw new InvalidOperationException("Author one inactive receiver");
        receiver=found[0];
        foreach(var component in Object.Components())
            if(component.TypeId==ComponentIds.Timer) {
                if(component.GetEnum("elapsed_action")!=1 || component.GetReference("elapsed_target").ObjectId!=receiver.ObjectId)
                    throw new InvalidOperationException("Persistent timeout receiver/action missing");
                timerId=component.InstanceId;
            }
        if(timerId==0) throw new InvalidOperationException("Author a Timer on this emitter");
        Scene.Log(ObjectId,"CONNECTION READY: receiver inactive; persistent action and native reference found");
    }
    public override void TimerElapsed(ulong instanceId,uint count)
    {
        if(instanceId!=timerId)return;
        if(count!=1 || !receiver.ActiveSelf) throw new InvalidOperationException("Timeout must activate receiver before C# callback");
        Scene.Log(ObjectId,"CONNECTION PASS: serialized Timer action activated receiver before managed TimerElapsed");
    }
}
