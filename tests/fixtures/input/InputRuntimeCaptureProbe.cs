using System;
using Astra;

[ComponentId("acceptance.input.runtime-capture")]
public sealed class InputRuntimeCaptureProbe : Behavior
{
    private int phase;
    public override void Start()
    {
        if(!Input.BeginBindingCapture("Saltar",0,InputSource.Key))throw new InvalidOperationException("Capture refused");
        Scene.Log(ObjectId,"RUNTIME CAPTURE READY: release controls, then press a new key; Escape cancels");
    }
    public override void Update(float delta)
    {
        if(phase==0 && Input.BindingCaptureStatus==InputCaptureStatus.Completed) {
            var binding=Input.GetBinding("Saltar");
            if(binding.Source!=InputSource.Key || Input.Pressed("Saltar"))throw new InvalidOperationException("Captured event leaked into gameplay");
            Input.SaveProfile("/storage/emulated/0/Android/data/dev.aether.editor/files/Projetos/RuntimeCapture-20261001/UserData/input.profile");
            Scene.Log(ObjectId,"RUNTIME CAPTURE COMMITTED: code="+binding.Code+"; release then press again");phase=1;
        }
        if(phase==0 && Input.BindingCaptureStatus==InputCaptureStatus.Cancelled) {
            Scene.Log(ObjectId,"RUNTIME CAPTURE CANCELLED: authored binding preserved");phase=2;
        }
        if(phase==1 && Input.JustPressed("Saltar")) {
            Scene.Log(ObjectId,"RUNTIME CAPTURE PASS: fresh press reached rebound gameplay action");phase=2;
        }
    }
}
