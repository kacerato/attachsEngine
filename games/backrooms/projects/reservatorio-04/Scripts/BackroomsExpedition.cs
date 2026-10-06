using Astra;
using Astra.Components;
using System;
using System.Collections.Generic;
using System.Numerics;

// Shared typed world consumer. Concrete projects select a different mission protocol.
public abstract class BackroomsExpedition : Behavior
{
    [PropertyId("interaction_reach")] public float InteractionReach = 2.0f;
    protected abstract int Mode { get; }
    private readonly Dictionary<string,GameObject> nodes = new();
    private readonly Light[] circuits = new Light[6];
    private readonly MeshRenderer[] diffusers = new MeshRenderer[6];
    private readonly AudioSource[] hum = new AudioSource[3];
    private readonly bool[] collected = new bool[3], completed = new bool[3];
    private readonly int[] valveOrder = {1,0,2};
    private Light flashlight;
    private Light exitLight;
    private MeshRenderer exitSign;
    private TransformTween gate;
    private int selected = -1, steps;
    private float elapsed, remaining = 180, lightingTick, report;
    private bool won, lost, evacuation, opened, opening, holdArmed, flashlightKeyDown;

    public override void Start()
    {
        Index(Object.Parent ?? throw new InvalidOperationException("Mission root missing"));
        for(int i=0;i<6;i++) {
            circuits[i]=new Light(Require("Circuito "+i,Light.TypeId));
            diffusers[i]=new MeshRenderer(Require("Difusor "+i,MeshRenderer.TypeId));
        }
        for(int i=0;i<3;i++) hum[i]=new AudioSource(Require("Zumbido "+i,AudioSource.TypeId));
        flashlight=new Light(Require("Lanterna móvel",Light.TypeId));
        exitLight=new Light(Require("Luz saída",Light.TypeId));
        exitSign=new MeshRenderer(Require("Sinal de saída",MeshRenderer.TypeId));
        gate=new TransformTween(Require("Porta de saída",TransformTween.TypeId));
        if(Input.ActionState("Operar").Interaction!=InputInteraction.Hold)
            throw new InvalidOperationException("Operar requires native Hold input");
        Input.SetActionEnabled("Operar",true);
        holdArmed=true;
        var settings=Graphics.State.Requested;
        settings.Preset=Mode==1?GraphicsQuality.Ultra:GraphicsQuality.High;
        settings.Shadows=Mode==1?GraphicsShadows.UltraSoft:GraphicsShadows.Soft;
        settings.ResolutionScale=1;
        settings.MaximumRenderHz=60;
        settings.Textures=GraphicsTextures.Full;
        settings.AntiAliasing=GraphicsAntiAliasing.Temporal;
        settings.Ambient=GraphicsAmbient.HemisphericSpecular;
        settings.Post=GraphicsPost.Bloom;
        settings.DynamicResolution=GraphicsFeature.Disabled;
        settings.MaterialShaderVariants=GraphicsFeature.Enabled;
        settings.EnvironmentSplitSumBrdf=GraphicsFeature.Enabled;
        if(Mode==1) {
            settings.ShadowCascadeCount=4;
            settings.ShadowCascadeResolution=2048;
            settings.ShadowFilterTaps=25;
            settings.ShadowFarFilterTaps=9;
            settings.ShadowMaximumDistance=65;
            settings.NormalMapMaximumDistance=90;
            settings.MetallicRoughnessMaximumDistance=90;
            settings.SpecularProbeMaximumDistance=90;
            settings.EmissiveMaximumDistance=90;
        }
        Graphics.ApplyRuntime(settings);
        ApplyLights();
        Say("READY mode="+Mode+" | "+(Mode==0?"recolha fusíveis; restaure três circuitos":Mode==1?"válvulas na ordem 2, 1, 3":"painéis 1, 2, 3; 180s após ativar o primeiro"));
    }
    private void Index(GameObject node) { nodes.Add(node.Name,node); foreach(var child in node.Children())Index(child); }
    private GameObject Node(string name)=>nodes.TryGetValue(name,out var node)?node:throw new InvalidOperationException("Missing "+name);
    private Component Require(string name,string type)=>Node(name).GetComponent(type)??throw new InvalidOperationException(name+" lacks "+type);
    private void Say(string text)=>Scene.Log(ObjectId,"BACKROOMS "+text);
    private float Distance(GameObject node) {
        var a=Object.WorldTransform.Position;var b=node.WorldTransform.Position;
        return Vector2.Distance(new(a.X,a.Z),new(b.X,b.Z));
    }
    public override void Update(float dt)
    {
        elapsed+=dt;
        bool keyDown=Input.ActionState("Lanterna").Phase==InputPhase.Performed;
        var operation=Input.ActionState("Operar");
        bool shortTap=operation.Phase==InputPhase.Canceled && operation.Elapsed>.03f && operation.Elapsed<.35f;
        if((keyDown&&!flashlightKeyDown)||shortTap) {
            flashlight.Enabled=!flashlight.Enabled;
            Say("FLASHLIGHT enabled="+flashlight.Enabled);
        }
        flashlightKeyDown=keyDown;
        lightingTick-=dt;
        if(lightingTick<=0) {lightingTick=.1f;ApplyLights();}
        if(won||lost)return;
        if(evacuation) {
            remaining=Math.Max(0,remaining-dt);
            if(remaining<=0) {
                lost=true;Input.SetActionEnabled("Operar",false);
                Say("LOSE | tempo de evacuação esgotado; Stop/Play reinicia");return;
            }
        }
        // Pickups require proximity, are removed from the active hierarchy, and gate real circuits.
        if(Mode==0)for(int i=0;i<3;i++)if(!collected[i]&&Distance(Node("Fusível "+(i+1)))<1.25f) {
            collected[i]=true;Node("Fusível "+(i+1)).SetActive(false);
            Say("FUSE collected="+(i+1));
        }
        int nearest=-1;float reach=InteractionReach;
        for(int i=0;i<3;i++)if(!completed[i]) {
            float d=Distance(Node("Painel "+(i+1)));
            if(d<reach){reach=d;nearest=i;}
        }
        if(Mode==0 && nearest>=0 && !collected[nearest])nearest=-1;
        var action=Input.ActionState("Operar");
        if(selected!=nearest) {
            if(selected>=0)SetProgress(selected,0);
            selected=nearest;holdArmed=false;Input.SetActionEnabled("Operar",false);
            if(nearest>=0)Say("TARGET panel="+(nearest+1)+" | segure Operar por 2s");
        } else if(!holdArmed) {
            // Re-enabling resets the native interaction. Entry cannot inherit an old hold.
            Input.SetActionEnabled("Operar",true);holdArmed=true;
        }
        if(selected>=0 && holdArmed) {
            SetProgress(selected,action.Phase==InputPhase.Started?action.Progress:action.Phase==InputPhase.Performed?1:0);
            if(action.Phase==InputPhase.Performed) {
                bool valid=Mode==0 || selected==(Mode==1?valveOrder[steps]:steps);
                if(!valid) {
                    Array.Clear(completed);steps=0;
                    for(int i=0;i<3;i++)SetProgress(i,0);
                    Say("RESET | sequência incorreta");
                } else {
                    completed[selected]=true;steps++;
                    if(Mode==2 && !evacuation){evacuation=true;Say("EVACUATION START 180s");}
                    if(Mode==1)new TransformTween(Require("Válvula "+(selected+1),TransformTween.TypeId)).Restart();
                    Say("OPERATED panel="+(selected+1)+" steps="+steps+"/3");
                    if(steps==3){
                        gate.Restart();opening=true;
                        if(Mode==1){
                            for(int i=1;i<=3;i++)new TransformTween(Require("Persiana "+i,TransformTween.TypeId)).Restart();
                            new Light(Require("Direcional / claraboias",Light.TypeId)).Intensity=.04f;
                        }
                        Say("EXIT OPENING");
                    }
                }
                Input.SetActionEnabled("Operar",false);holdArmed=false;selected=-1;
                ApplyLights();
            }
        }
        if(opening&&!opened&&gate.State().Status==TweenRuntimeStatus.Completed) {
            Node("Colisão saída").SetActive(false);opened=true;
            Say("EXIT OPEN | native tween completed and collision removed");
        }
        if(opened&&Object.WorldTransform.Position.Z>47) {
            won=true;evacuation=false;Input.SetActionEnabled("Operar",false);
            Say("WIN | extração concluída remaining="+remaining.ToString("F1"));
        }
        report-=dt;if(report<=0) {
            report=5;
            string objective=Mode==0?"Circuitos":Mode==1?"Válvulas":"Painéis";
            string next=steps<3?" · próximo "+(Mode==1?valveOrder[steps]+1:Mode==2?steps+1:selected+1):" · saída liberada";
            Say(objective+" "+steps+"/3"+next+(evacuation?" · "+remaining.ToString("F0")+"s":"")+" · lanterna "+(flashlight.Enabled?"ligada":"desligada"));
        }
    }
    private void SetProgress(int sector,float amount) {
        for(int k=0;k<8;k++)new MeshRenderer(Require($"Progresso {sector+1} {k}",MeshRenderer.TypeId)).EmissionStrength=amount>=(k+1)/8f?3:0;
    }
    private void ApplyLights() {
        bool blackout=Mode==2&&evacuation&&!won&&(elapsed%18)>13;
        for(int i=0;i<6;i++) {
            int sector=i/2;
            bool on=!lost&&!blackout&&completed[sector];
            float ripple=Mode==0&&!completed[sector]?0:1;
            // One faulty ballast, short interruptions only; the world is otherwise stable.
            if(on&&i==3&&elapsed%11>10.7f)ripple=.12f;
            circuits[i].Enabled=on;
            circuits[i].Intensity=(Mode==1?(i%2==0?100:25):30)*ripple;
            if(Mode==1)circuits[i].Color=completed[sector]?new(.72f,1,.82f):new(.65f,.8f,1);
            diffusers[i].EmissionStrength=on?1.8f*ripple:0;
        }
        exitLight.Enabled=steps==3&&!lost;
        exitSign.EmissionStrength=steps==3&&!lost?1.2f:0;
        // Practical wall lights share the circuit's powered state.
        if(Mode==1)for(int i=1;i<6;i+=2)
            new MeshRenderer(Require("Arandela de serviço "+i,MeshRenderer.TypeId)).EmissionStrength=circuits[i].Enabled?1.2f:0;
        for(int i=0;i<3;i++) {
            bool on=circuits[i*2].Enabled;
            var playback=on?AudioSource.PlaybackOption.Tocar:AudioSource.PlaybackOption.Parar;
            if(hum[i].Playback!=playback)hum[i].Playback=playback;
        }
    }
}
