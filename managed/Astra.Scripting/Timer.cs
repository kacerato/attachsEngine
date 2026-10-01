using Astra.Components;
namespace Astra;

public readonly record struct TimerRuntimeState(double RemainingSeconds,bool Running,bool Paused,bool Completed,bool Enabled,bool ActiveInHierarchy)
{
}
/// <summary>Controls the session scheduler; countdown state is never authored or serialized.</summary>
public static class GameTimerRuntime
{
    private static TimerRuntimeState Command(GameTimer timer,uint operation,float seconds=0)
    {
        var component=timer.Component;
        if(!component.IsAlive)throw new WorldException(component.Scene is null?WorldStatus.InvalidArgument:component.Scene.LastStatus,"controlar timer");
        if(!component.Scene.TimerCommand(component.Object.ObjectId,component.InstanceId,operation,seconds,out var state))
            throw new WorldException(component.Scene.LastStatus,"controlar timer");
        return state;
    }
    public static TimerRuntimeState State(this GameTimer timer) => Command(timer,0);
    /// <summary>Starts/restarts; positive seconds also updates the runtime interval. Zero uses the configured interval. Keeps per-timer pause.</summary>
    public static void Start(this GameTimer timer,float seconds=0) {
        if(!float.IsFinite(seconds)||seconds<0||(seconds>0&&(seconds<.05f||seconds>3600)))throw new ArgumentOutOfRangeException(nameof(seconds));
        Command(timer,1,seconds);
    }
    public static void Stop(this GameTimer timer) => Command(timer,2);
    public static void Pause(this GameTimer timer) => Command(timer,3);
    public static void Resume(this GameTimer timer) => Command(timer,4);
}
