namespace Astra;

public enum NumberTweenEasing : uint {Linear,Smoothstep,QuadraticIn,QuadraticOut}
public enum NumberTweenStatus : uint {Running,Completed,Cancelled,Failed}
public readonly record struct NumberTweenState(NumberTweenStatus Status,WorldStatus Failure,double ElapsedSeconds,float LastWrittenValue,float DurationSeconds,bool Paused,bool ActiveInHierarchy,bool Enabled);

/// <summary>World-bound native track. Dispose releases its retained snapshot; terminal tracks otherwise count toward the 256-track limit.</summary>
public sealed class NumberTween : IDisposable
{
    private readonly ISceneAccess scene;
    private readonly int ownerThread=Environment.CurrentManagedThreadId;
    private bool disposed;
    public ulong Id {get;}
    internal NumberTween(ISceneAccess scene,ulong id){this.scene=scene;Id=id;}
    private void RequireThread(){if(Environment.CurrentManagedThreadId!=ownerThread)throw new InvalidOperationException("Tween controls must run on their scene thread");}
    private NumberTweenState Command(uint operation)
    {
        RequireThread();ObjectDisposedException.ThrowIf(disposed,this);
        if(!scene.NumberTweenCommand(Id,operation,out var state))throw new WorldException(scene.LastStatus,"controlar tween numérico");
        return state;
    }
    public NumberTweenState State=>Command(0);
    public void Pause()=>Command(1);
    public void Resume()=>Command(2);
    public void Cancel()=>Command(3);
    public void Dispose()
    {
        RequireThread();if(disposed)return;
        // Stop already clears the native session store. Never dispatch an old id to a new world.
        if(scene.WorldId!=0&&scene.WorldId==(uint)(Id>>32))Command(4);
        disposed=true;
    }
}

public static class NumberTweenCreation
{
    /// <summary>Interpolates an explicitly eligible numeric PropertyId using the native session scheduler. Competing writers fail instead of silently overwriting.</summary>
    public static NumberTween TweenFloat(this Component component,string propertyId,float destination,float duration,NumberTweenEasing easing=NumberTweenEasing.Linear,bool ignoreTimeScale=false)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(propertyId);
        if(System.Text.Encoding.UTF8.GetByteCount(propertyId)>127||propertyId.Contains('\0'))throw new ArgumentOutOfRangeException(nameof(propertyId));
        if(!float.IsFinite(destination))throw new ArgumentOutOfRangeException(nameof(destination));
        if(!float.IsFinite(duration)||duration<.001f||duration>36000)throw new ArgumentOutOfRangeException(nameof(duration));
        if((uint)easing>3)throw new ArgumentOutOfRangeException(nameof(easing));
        if(!component.IsAlive)throw new WorldException(component.Scene is null?WorldStatus.InvalidArgument:component.Scene.LastStatus,"criar tween numérico");
        if(!component.Scene.NumberTweenCreate(component.Object.ObjectId,component.InstanceId,propertyId,destination,duration,(uint)easing,ignoreTimeScale,out var id))throw new WorldException(component.Scene.LastStatus,"criar tween numérico");
        return new(component.Scene,id);
    }
}
