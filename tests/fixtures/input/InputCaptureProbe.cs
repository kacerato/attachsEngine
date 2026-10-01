using System;
using Astra;

[ComponentId("acceptance.input.capture")]
public sealed class InputCaptureProbe : Behavior
{
    private bool sawPress;
    private int presses, releases;
    public override void Start()
    {
        if(!Input.Exists("Saltar")) throw new InvalidOperationException("Authored Saltar action missing");
        Scene.Log(ObjectId,"INPUT READY: waiting for captured Saltar binding");
    }
    public override void Update(float delta)
    {
        if(Input.JustPressed("Saltar")) {
            if(!Input.Pressed("Saltar")) throw new InvalidOperationException("Press edge without held state");
            sawPress=true; ++presses;
            Scene.Log(ObjectId,"INPUT DOWN: presses="+presses);
        }
        if(Input.JustReleased("Saltar")) {
            if(Input.Pressed("Saltar") || !sawPress) throw new InvalidOperationException("Release without matching press");
            sawPress=false; ++releases;
            Scene.Log(ObjectId,"INPUT PASS: captured binding reached C#; presses="+presses+" releases="+releases);
        }
    }
}
