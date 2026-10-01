using System;
using Astra;
using Astra.Components;

[ComponentId("acceptance.physics.connection")]
public sealed class PhysicsConnectionProbe : Behavior
{
    private GameObject receiver;
    private ulong incoming;
    public override void Start()
    {
        var found=Object.FindGameObjectsInGroup("physics-event-receiver");
        if(found.Length!=1||found[0].ActiveSelf)throw new InvalidOperationException("Expected one inactive receiver");
        receiver=found[0];bool connected=false;
        foreach(var component in Object.Components())
            if(component.TypeId==PhysicsEventConnection3D.TypeId) {
                var connection=new PhysicsEventConnection3D(component);
                if(connection.Event!=PhysicsEventConnection3D.EventOption.EntradaSensor ||
                   connection.Action!=PhysicsEventConnection3D.ActionOption.AtivarObjeto || connection.Receiver.ObjectId!=receiver.ObjectId)
                    throw new InvalidOperationException("Authored connection mismatch");
                incoming=connection.OtherFilter.ObjectId;connected=true;
            }
        if(!connected||incoming==0)throw new InvalidOperationException("Connection or incoming filter missing");
        Scene.Log(ObjectId,"PHYSICS CONNECTION READY: real sensor, inactive light, authored receiver and body filter");
    }
    public override void TriggerEnter(ObjectReference other)
    {
        if(other.ObjectId!=incoming)return;
        if(!receiver.ActiveSelf)throw new InvalidOperationException("Native action must precede managed TriggerEnter");
        Scene.Log(ObjectId,"PHYSICS CONNECTION PASS: Jolt trigger activated receiver before C# callback");
    }
}
