using Astra;
using Astra.Components;
using System;
using System.Collections.Generic;

// Explicitly selected acceptance scene; never attached to the playable main scene.
[ComponentId("project.LightProbe")]
public sealed class LightProbe : Behavior
{
    private readonly List<Light> local=new();
    private readonly List<MeshRenderer> diffusers=new();
    private Light flashlight, sun;
    private Light exit;
    private readonly List<MeshRenderer> otherEmission=new();
    private float elapsed, report;
    private int stage=-1;
    private void Index(GameObject node) {
        if(node.Name.StartsWith("Circuito "))local.Add(new Light(node.GetComponent(Light.TypeId)!.Value));
        if(node.Name.StartsWith("Difusor "))diffusers.Add(new MeshRenderer(node.GetComponent(MeshRenderer.TypeId)!.Value));
        if(node.Name=="Lanterna móvel")flashlight=new Light(node.GetComponent(Light.TypeId)!.Value);
        if(node.Name=="Direcional / claraboias")sun=new Light(node.GetComponent(Light.TypeId)!.Value);
        if(node.Name=="Luz saída")exit=new Light(node.GetComponent(Light.TypeId)!.Value);
        if(node.Name=="Sinal de saída"||node.Name.StartsWith("Arandela de serviço ")||node.Name.StartsWith("Progresso "))otherEmission.Add(new MeshRenderer(node.GetComponent(MeshRenderer.TypeId)!.Value));
        foreach(var child in node.Children())Index(child);
    }
    public override void Start() {
        Index(Object.Parent!);
        var settings=Graphics.State.Requested;
        settings.Preset=GraphicsQuality.Ultra;
        settings.Shadows=GraphicsShadows.UltraSoft;
        settings.ResolutionScale=1;settings.MaximumRenderHz=60;
        settings.DynamicResolution=GraphicsFeature.Disabled;
        settings.AntiAliasing=GraphicsAntiAliasing.Temporal;
        settings.Post=GraphicsPost.Bloom;
        Graphics.ApplyRuntime(settings);
    }
    public override void Update(float dt) {
        elapsed+=dt;
        int next=Math.Min(4,(int)(elapsed/8));
        if(next!=stage) {
            stage=next;
            foreach(var light in local)light.Enabled=stage==0||stage==3;
            foreach(var mesh in diffusers)mesh.EmissionStrength=stage==0||stage==3?1.8f:0;
            flashlight.Enabled=stage==2;
            sun.Intensity=stage==4?.18f:0;
            exit.Enabled=false;
            foreach(var mesh in otherEmission)mesh.EmissionStrength=0;
            Scene.Log(ObjectId,"LIGHTPROBE STAGE="+stage+" 0=spots 1=dark 2=flashlight 3=spots-restored 4=directional");
        }
        report-=dt;if(report<=0) {
            report=2;var state=Graphics.State;
            Scene.Log(ObjectId,$"LIGHTPROBE stage={stage} elapsed={elapsed:F1} GPU={state.Frame.GpuFrameMs:F2} frame={state.Frame.FrameIntervalMs:F2} shadow={state.Effective.ShadowsEnabled} aa={state.Effective.AntiAliasing} effective={state.EffectiveAvailable} clamps={state.Effective.ClampCount}");
        }
    }
}
