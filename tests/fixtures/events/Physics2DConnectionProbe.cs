using System;
using Astra;
using Astra.Components;

[ComponentId("acceptance.physics2d.connection")]
public sealed class Physics2DConnectionProbe : Behavior
{
    private GameObject receiver;
    private ulong incoming;
    public override void Start()
    {
        var found=Object.FindGameObjectsInGroup("physics2d-event-receiver");
        if(found.Length!=1||found[0].ActiveSelf)throw new InvalidOperationException("Expected one inactive receiver");
        receiver=found[0];bool connected=false;
        foreach(var component in Object.Components())
            if(component.TypeId==PhysicsEventConnection2D.TypeId) {
                var connection=new PhysicsEventConnection2D(component);
                if(connection.Event!=PhysicsEventConnection2D.EventOption.EntradaSensor ||
                   connection.Action!=PhysicsEventConnection2D.ActionOption.AtivarObjeto || connection.Receiver.ObjectId!=receiver.ObjectId)
                    throw new InvalidOperationException("Authored connection mismatch");
                incoming=connection.OtherFilter.ObjectId;connected=true;
            }
        if(!connected||incoming==0)throw new InvalidOperationException("Connection or incoming filter missing");
        Scene.Log(ObjectId,"PHYSICS2D CONNECTION READY: real sensor, inactive light, authored receiver and body filter");
    }
    public override void TriggerEnter(ObjectReference other)
    {
        if(other.ObjectId!=incoming)return;
        if(!receiver.ActiveSelf)throw new InvalidOperationException("Native action must precede managed TriggerEnter");
        Scene.Log(ObjectId,"PHYSICS2D CONNECTION PASS: Box2D trigger activated receiver before C# callback");
    }
}
