using System;
using Astra;

[ComponentId("acceptance.input.mouse")]
public sealed class MouseProbe : Behavior
{
    private bool down;
    public override void Start()
    {
        foreach(var action in new[]{"Saltar","MouseLook","MouseWheel"})
            if(!Input.Exists(action)) throw new InvalidOperationException("Missing action: "+action);
        Scene.Log(ObjectId,"MOUSE READY: primary button, viewport motion and wheel");
    }
    public override void Update(float delta)
    {
        if(Input.JustPressed("Saltar")) {
            if(!Input.Pressed("Saltar")) throw new InvalidOperationException("Mouse edge without held state");
            down=true; Scene.Log(ObjectId,"MOUSE DOWN");
        }
        if(Input.JustReleased("Saltar") && down) {
            if(Input.Pressed("Saltar")) throw new InvalidOperationException("Mouse release remains held");
            down=false; Scene.Log(ObjectId,"MOUSE PASS: primary press/release reached C#");
        }
        var motion=Input.Axis2("MouseLook");
        if(motion.LengthSquared()>0) Scene.Log(ObjectId,"MOUSE MOTION: "+motion);
        var wheel=Input.Axis("MouseWheel");
        if(wheel!=0) Scene.Log(ObjectId,"MOUSE WHEEL: "+wheel);
    }
}
