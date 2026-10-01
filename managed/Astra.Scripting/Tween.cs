using Astra.Components;
namespace Astra;

public enum TweenRuntimeStatus : uint { Idle,Delayed,Running,Completed,Cancelled,Authority,CompetingWriter,InvalidPose,NoChannels }
public readonly record struct TweenRuntimeState(TweenRuntimeStatus Status,double ElapsedSeconds,bool Paused,bool Enabled,bool ActiveInHierarchy);

/// <summary>Controls the session-owned transform evaluator. Runtime state is not serialized.</summary>
public static class TransformTweenRuntime
{
    private static TweenRuntimeState Command(TransformTween tween,uint operation)
    {
        var component=tween.Component;
        if(!component.IsAlive)throw new WorldException(component.Scene is null?WorldStatus.InvalidArgument:component.Scene.LastStatus,"controlar tween");
        if(!component.Scene.TweenCommand(component.Object.ObjectId,component.InstanceId,operation,out var state))
            throw new WorldException(component.Scene.LastStatus,"controlar tween");
        return state;
    }
    public static TweenRuntimeState State(this TransformTween tween)=>Command(tween,0);
    /// <summary>Captures the current local pose on the next evaluation and restarts delay. Keeps local pause.</summary>
    public static void Restart(this TransformTween tween)=>Command(tween,1);
    /// <summary>Stops writing and preserves the current runtime pose.</summary>
    public static void Cancel(this TransformTween tween)=>Command(tween,2);
    public static void Pause(this TransformTween tween)=>Command(tween,3);
    public static void Resume(this TransformTween tween)=>Command(tween,4);
}
