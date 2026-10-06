using Astra;
using Astra.Components;
using System;
using System.Collections.Generic;
using System.Numerics;

[ComponentId("project.ForestExpedition")]
public sealed class ForestExpedition : Behavior
{
    [PropertyId("reach")] public float Reach = 4.8f;
    private readonly Dictionary<string,GameObject> nodes=new();
    private readonly bool[] calibrated=new bool[3];
    private GameTimer deadline;
    private TransformTween exitTween;
    private float report, previousProgress;
    private int selected=-1, complete;
    private bool finished, gateReported, audioReported;
    public override void Start()
    {
        var root=Object.Parent ?? throw new InvalidOperationException("Mission root absent");
        Index(root);
        deadline=new GameTimer(Object.GetComponent(GameTimer.TypeId) ?? throw new InvalidOperationException("Mission timer absent"));
        exitTween=new TransformTween(Node("Portão de extração").GetComponent(TransformTween.TypeId) ?? throw new InvalidOperationException("Exit tween absent"));
        if(Input.ActionState("Calibrar").Interaction!=InputInteraction.Hold)throw new InvalidOperationException("Calibrar requires authored Hold interaction");
        deadline.Start();
        foreach(var n in nodes.Values)if(n.Name.StartsWith("Sinal "))new AudioSource(n.GetComponent(AudioSource.TypeId)!.Value).Playback=AudioSource.PlaybackOption.Tocar;
        Say("READY | ECOS DA MATA | siga os sinais; segure Calibrar perto de cada estação por 2s. Soltar/caminhar para longe cancela. 3 estações abrem a saída. Limite: 10 minutos.");
    }
    private void Index(GameObject n){nodes[n.Name]=n;foreach(var child in n.Children())Index(child);}
    private GameObject Node(string name)=>nodes.TryGetValue(name,out var n)?n:throw new InvalidOperationException("Missing "+name);
    private void Say(string text)=>Scene.Log(ObjectId,"FOREST "+text);
    public override void Update(float dt)
    {
        if(finished)return;
        var player=Object.WorldTransform.Position;
        int nearest=-1;float distance=Reach;
        for(int i=0;i<3;i++)if(!calibrated[i]){
            var p=Node("Estação "+(i+1)).WorldTransform.Position;
            float d=Vector2.Distance(new(player.X,player.Z),new(p.X,p.Z));if(d<distance){distance=d;nearest=i;}
        }
        var action=Input.ActionState("Calibrar");
        if(selected!=nearest){
            if(selected>=0){SetSegments(selected,0);if(previousProgress>0)Say("CANCEL RANGE station="+(selected+1));}
            selected=nearest;previousProgress=0;
            // A hold begun outside the station cannot carry its progress into the target.
            Input.SetActionEnabled("Calibrar",false);
            if(nearest>=0)Say("TARGET station="+(nearest+1)+" | segure Calibrar por 2s");
        } else Input.SetActionEnabled("Calibrar",nearest>=0);
        if(selected>=0){
            if(action.Phase==InputPhase.Canceled && previousProgress>0){Say("CANCEL RELEASE station="+(selected+1));previousProgress=0;}
            float progress=action.Phase==InputPhase.Started?action.Progress:action.Phase==InputPhase.Performed?1:0;
            SetSegments(selected,progress);
            if(progress>0 && previousProgress==0)Say("HOLD START station="+(selected+1));
            previousProgress=progress;
            if(action.Phase==InputPhase.Performed && nearest==selected){
                calibrated[selected]=true;complete++;
                new AudioSource(Node("Sinal "+(selected+1)).GetComponent(AudioSource.TypeId)!.Value).Playback=AudioSource.PlaybackOption.Parar;
                new TransformTween(Node("Antena "+(selected+1)).GetComponent(TransformTween.TypeId)!.Value).Restart();
                Node("Luz confirmada "+(selected+1)).SetActive(true);
                Say("CALIBRATED station="+(selected+1)+" total="+complete+"/3 nativeProgress="+action.Progress);
                selected=-1;previousProgress=0;
                Input.SetActionEnabled("Calibrar",false);
                if(complete==3){exitTween.Restart();Say("EXIT OPENING | native tween requested");}
            }
        }
        if(complete==3 && !gateReported && exitTween.State().Status==TweenRuntimeStatus.Completed){
            Node("Colisão do portão").SetActive(false);gateReported=true;Say("EXIT OPEN | tween completed; physical barrier removed");
        }
        if(gateReported && player.Z>23){finished=true;deadline.Stop();Say("WIN | rede calibrada; extração concluída; remaining="+deadline.State().RemainingSeconds.ToString("F1"));}
        if(!audioReported){
            AudioVoiceSnapshot? voice;
            try { voice=new AudioVoice(Node("Sinal 1").GetComponent(AudioSource.TypeId)!.Value).Snapshot; }
            catch(WorldException e) when(e.Status==WorldStatus.NotRunning) { voice=null; }
            if(voice is {} observed && observed.Cursor>.1 && observed.OutputRunning){audioReported=true;Say("AUDIO BACKEND | state="+observed.State+" cursor="+observed.Cursor.ToString("F2")+" output="+observed.OutputRunning);}
        }
        report-=dt;if(report<=0){report=5;if(!audioReported)Say("AUDIO WAIT | backend output/cursor not observed yet");var p=Object.ReadCharacterState();Say($"STATUS position={player.X:F1},{player.Y:F1},{player.Z:F1} grounded={p.IsGrounded} calibrated={complete}/3 target={nearest+1} phase={action.Phase} progress={action.Progress:F2} remaining={deadline.State().RemainingSeconds:F1}");}
    }
    private void SetSegments(int station,float progress){for(int k=0;k<8;k++)Node("LED "+(station+1)+" "+k).SetActive(progress>=(k+1)/8f);}
    public override void TimerElapsed(ulong instance,uint count){if(instance==deadline.InstanceId&&!finished){finished=true;Input.SetActionEnabled("Calibrar",false);Say("LOSE | janela de comunicação encerrada. Stop/Play reinicia a missão.");}}
}
