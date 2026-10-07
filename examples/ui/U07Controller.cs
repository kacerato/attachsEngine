using System;
using System.Numerics;
using Astra;
using Astra.Components;

[ComponentId("example.gui.u07-control")]
public sealed class U07Controller : Behavior
{
    private GuiAccess hud = null!;
    private GameObject player = null!;
    private MotorControlRuntime control;
    private DynamicBodyMotor settings;
    private PhysicsBodyRuntime body;
    private bool scriptClaim, highAi;
    private float aiDirection=1,scriptDirection=-1;
    private MotorControlSource observed;
    private MotorAnimationDriver locomotion=null!;
    private MotorAnimationPhase observedAnimation;
    public override void Start()
    {
        hud = Gui.ForCanvas(Object);
        var canvas=Object.GetComponent<UiCanvas>() ?? throw new InvalidOperationException("Behavior requer Canvas");
        player = GameObject.Resolve(Scene, canvas.InputReceiver.ObjectId);
        settings = player.GetComponent<DynamicBodyMotor>() ?? throw new InvalidOperationException("Receptor sem motor dinâmico");
        control = player.MotorControl();
        body = player.PhysicsBody();
        var animationComponent=FindAnimation(player) ?? throw new InvalidOperationException("Personagem sem animação importada");
        // Distances extracted from the original Godot root-motion track,
        // after the model's authored .8039915 scale: 1.75 m / 2.666667 m.
        locomotion=new(animationComponent.Animation(),new("Idle","Walk","Run","Jump","Rise","Apex","Fall","Land"),
                       new(WalkCycleMeters:1.75f,RunCycleMeters:8f/3,WalkSpeed:1.4f,RunSpeed:3.2f));
        Scene.Log(ObjectId,"U07 READY: typed ownership and real Body motor");
    }
    public override void FixedUpdate(float seconds)
    {
        // The selected motor intent already carries its camera/world reference.
        // Rotate the real solver body: never overwrite its transform or rotate
        // a collider child independently of its owner.
        var state=control.State;
        if(!state.Focused||state.Move.LengthSquared()<.0025f) {
            body.AngularVelocity=Vector3.Zero;return;
        }
        var sin=MathF.Sin(state.YawRadians);var cos=MathF.Cos(state.YawRadians);
        var x=state.Move.X*cos+state.Move.Y*sin;
        var z=state.Move.Y*cos-state.Move.X*sin;
        var desired=MathF.Atan2(x,z);
        var forward=Vector3.Transform(Vector3.UnitZ,player.WorldTransform.Rotation);var current=MathF.Atan2(forward.X,forward.Z);
        var error=MathF.IEEERemainder(desired-current,2*MathF.PI);
        body.AngularVelocity=new Vector3(0,Math.Clamp(error*12,-8,8),0);
    }
    public override void Update(float seconds)
    {
        if(hud.Input.JustPressed("Fonte")) {
            control.PolicySource=(MotorControlSource)(((uint)control.PolicySource+1)%6);
            Scene.Log(ObjectId,"U07 POLICY "+control.PolicySource);
        }
        if(hud.Input.JustPressed("Script"))scriptClaim=!scriptClaim;
        if(hud.Input.JustPressed("PrioridadeIA")) {
            highAi=!highAi;settings.ControlPriorityAi=highAi?50:5;
        }
        // This is an explicit AI intent, not navigation/pathfinding support.
        var p=player.WorldTransform.Position;
        if(p.Z>3)aiDirection=-1;else if(p.Z<-3)aiDirection=1;
        if(p.X>3)scriptDirection=-1;else if(p.X<-3)scriptDirection=1;
        control.Submit(MotorControlSource.Ai,new Vector2(0,.22f*aiDirection));
        if(scriptClaim)control.Submit(MotorControlSource.Script,new Vector2(.35f*scriptDirection,0));
        else control.Release(MotorControlSource.Script);
    }
    public override void LateUpdate(float seconds)
    {
        var state=control.State;var pose=player.WorldTransform;var p=pose.Position;
        var forward=Vector3.Transform(Vector3.UnitZ,pose.Rotation);var heading=MathF.Atan2(forward.X,forward.Z)*180/MathF.PI;
        var motion=control.MotionState;
        locomotion.Tick(motion,seconds);
        if(locomotion.Phase!=observedAnimation){observedAnimation=locomotion.Phase;Scene.Log(ObjectId,"U07 ANIMATION "+observedAnimation+" grounded="+motion.Grounded);}
        hud.Find("Instrucoes").Text=FormattableString.Invariant(
            $"{(control.PolicySource==MotorControlSource.None?"Automático":SourceName(control.PolicySource))} · posse {SourceName(state.Source)} · prioridade {state.Priority:F0}");
        hud.Find("Movimento").Text=FormattableString.Invariant(
            $"{locomotion.Phase} · {(motion.Grounded?"solo":"ar")} · {locomotion.GroundSpeed:F1} m/s · giro {heading:F0}°");
        if(state.HasMeasuredStep&&state.Source!=observed){observed=state.Source;Scene.Log(ObjectId,"U07 OWNER "+observed+" candidates="+state.Candidates);}
    }
    private static string SourceName(MotorControlSource source)=>source switch {
        MotorControlSource.Ui=>"UI",MotorControlSource.Keyboard=>"Teclado",MotorControlSource.Gamepad=>"Gamepad",
        MotorControlSource.Script=>"Script",MotorControlSource.Ai=>"IA",_=>"sem intenção"};
    private static Component? FindAnimation(GameObject node) {
        var found=node.GetComponent(ComponentIds.Animation);if(found is not null)return found;
        foreach(var child in node.Children()){found=FindAnimation(child);if(found is not null)return found;}
        return null;
    }
    public override void Disable()
    {
        if(player is {IsAlive:true}) {control.Release(MotorControlSource.Script);control.Release(MotorControlSource.Ai);}
    }
}
