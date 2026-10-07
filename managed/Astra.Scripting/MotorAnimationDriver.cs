using System.Numerics;
namespace Astra;

/// <summary>Real solver-driven locomotion. Clip names and stride lengths belong
/// to the authored rig; no animation clip, ground contact or velocity is guessed.</summary>
public sealed record MotorAnimationClips(string Idle, string Walk, string Run,
    string Jump, string Rise, string Apex, string Fall, string Land);
public enum MotorAnimationPhase { Idle, Locomotion, Jump, Rise, Apex, Fall, Land }
public sealed record MotorAnimationSettings(float WalkCycleMeters=1.6f, float RunCycleMeters=3.2f,
    float WalkSpeed=1.28f, float RunSpeed=3.84f, float FadeSeconds=.16f,
    float StartMovingSpeed=.2f, float StopMovingSpeed=.12f, float ApexSpeed=.8f);

/// <summary>Eight clips, idle/moving hysteresis, phase-synchronized walk/run
/// blend and airborne/landing states. Tick after physics with measured motion.
/// Uses existing animation resources and playback; never moves the rigid body.</summary>
public sealed class MotorAnimationDriver
{
    private readonly AnimationPlayer player;
    private readonly MotorAnimationSettings settings;
    private readonly AnimationState[] clips;
    private readonly string[] names;
    private float phase, airTime, stateTime, previousVertical, blend=-1;
    private bool wasGrounded, initialized;
    public MotorAnimationPhase Phase { get; private set; }=MotorAnimationPhase.Idle;
    public float GroundSpeed { get; private set; }
    public MotorAnimationDriver(AnimationPlayer player, MotorAnimationClips clips,
                                MotorAnimationSettings? settings=null)
    {
        this.player=player;this.settings=settings??new();
        var s=this.settings;
        if(!float.IsFinite(s.WalkCycleMeters+s.RunCycleMeters+s.WalkSpeed+s.RunSpeed+s.FadeSeconds+
                           s.StartMovingSpeed+s.StopMovingSpeed+s.ApexSpeed)||s.WalkCycleMeters<=0||s.RunCycleMeters<=0||
           s.RunSpeed<=s.WalkSpeed||s.WalkSpeed<=0||s.FadeSeconds<0||s.StopMovingSpeed<0||
           s.StartMovingSpeed<=s.StopMovingSpeed||s.ApexSpeed<=0)throw new ArgumentException("Invalid locomotion settings");
        names=[clips.Idle,clips.Walk,clips.Jump,clips.Rise,clips.Apex,clips.Fall,clips.Land,clips.Run];
        if(names.Any(string.IsNullOrWhiteSpace)||names.Distinct(StringComparer.Ordinal).Count()!=8)throw new ArgumentException("Eight distinct authored clips are required");
        this.clips=names.Select(name=>player[name]).ToArray();
        for(var i=0;i<this.clips.Length;++i) {
            var clip=this.clips[i];
            if(clip.Length<=0)throw new ArgumentException("Empty locomotion clip: "+names[i]);
            clip.WrapMode=i is 2 or 6?AnimationWrapMode.ClampForever:AnimationWrapMode.Loop;
        }
        player.Play(names[0]);
    }
    public void Tick(in MotorMotionState motion,float seconds)
    {
        if(!float.IsFinite(seconds)||seconds<0)throw new ArgumentOutOfRangeException(nameof(seconds));
        if(!motion.HasMeasuredStep||seconds==0)return;
        var relative=motion.Grounded?motion.Velocity-motion.GroundVelocity:motion.Velocity;
        GroundSpeed=new Vector2(relative.X,relative.Z).Length();
        var next=Phase;stateTime+=seconds;
        if(!motion.Grounded) {
            if(!initialized||wasGrounded)airTime=0;
            airTime+=seconds;
            next=relative.Y < -settings.ApexSpeed?MotorAnimationPhase.Fall:
                 relative.Y<=settings.ApexSpeed?MotorAnimationPhase.Apex:
                 airTime<clips[2].Length?MotorAnimationPhase.Jump:MotorAnimationPhase.Rise;
        } else {
            var moving=GroundSpeed>(Phase==MotorAnimationPhase.Locomotion?settings.StopMovingSpeed:settings.StartMovingSpeed);
            if(initialized&&!wasGrounded&&airTime>.12f&&previousVertical<-1.5f&&!moving)next=MotorAnimationPhase.Land;
            else if(Phase==MotorAnimationPhase.Land&&stateTime<clips[6].Length&&!moving)next=Phase;
            else next=moving?MotorAnimationPhase.Locomotion:MotorAnimationPhase.Idle;
            airTime=0;
        }
        if(next!=Phase) {
            Phase=next;stateTime=0;blend=-1;
            var index=(int)Phase;
            if(Phase!=MotorAnimationPhase.Locomotion) {
                player.Rewind(names[index]);player.CrossFade(names[index],settings.FadeSeconds);
            } else player.CrossFade(names[1],settings.FadeSeconds);
        }
        if(Phase==MotorAnimationPhase.Locomotion) {
            var amount=Math.Clamp((GroundSpeed-settings.WalkSpeed)/(settings.RunSpeed-settings.WalkSpeed),0,1);
            if(Math.Abs(amount-blend)>.01f) {
                player.Blend(names[1],1-amount,settings.FadeSeconds);
                player.Blend(names[7],amount,settings.FadeSeconds);blend=amount;
            }
            // One normalized gait phase; each cycle advances by real travel,
            // including acceleration/braking and moving support subtraction.
            var stride=settings.WalkCycleMeters+(settings.RunCycleMeters-settings.WalkCycleMeters)*amount;
            phase=(phase+GroundSpeed*seconds/stride)%1;
            for(var n=0;n<2;++n) {var index=n==0?1:7;
                var clip=clips[index];clip.Speed=0;clip.NormalizedTime=phase;
            }
        }
        previousVertical=relative.Y;wasGrounded=motion.Grounded;initialized=true;
    }
}
