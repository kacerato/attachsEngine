using System.Collections;

namespace Astra;

public enum CoroutineStatus { Running, Completed, Cancelled, Failed }

/// <summary>Session-owned operation handle. Completed handles retain neither the Behavior nor its enumerators.</summary>
public sealed class Coroutine
{
    internal Coroutine(object scope, ulong objectId, ulong instanceId)
    { Scope = scope; ObjectId = objectId; InstanceId = instanceId; }
    internal object Scope { get; }
    internal ulong ObjectId { get; }
    internal ulong InstanceId { get; }
    public CoroutineStatus Status { get; internal set; }
    public bool IsRunning => Status == CoroutineStatus.Running;
    public string? Failure { get; internal set; }
}

/// <summary>Waits for simulation seconds delivered by the native Update callback. Resume is always on a later frame.</summary>
public sealed class WaitForSeconds
{
    public WaitForSeconds(double seconds) { Validate(seconds); Seconds = seconds; }
    public double Seconds { get; }
    internal static void Validate(double seconds)
    { if (!double.IsFinite(seconds) || seconds < 0) throw new ArgumentOutOfRangeException(nameof(seconds)); }
}

/// <summary>Waits for monotonic real time. When dispatch is paused, completion is delivered on the next running Update.</summary>
public sealed class WaitForSecondsRealtime
{
    public WaitForSecondsRealtime(double seconds) { WaitForSeconds.Validate(seconds); Seconds = seconds; }
    public double Seconds { get; }
}

public sealed class WaitUntil
{
    public WaitUntil(Func<bool> predicate) { ArgumentNullException.ThrowIfNull(predicate); Predicate = predicate; }
    internal Func<bool> Predicate { get; }
}
public sealed class WaitWhile
{
    public WaitWhile(Func<bool> predicate) { ArgumentNullException.ThrowIfNull(predicate); Predicate = predicate; }
    internal Func<bool> Predicate { get; }
}

/// <summary>Resumes after the next dispatched physical fixed step, never from Update.</summary>
public sealed class WaitForFixedUpdate { }

internal interface ICoroutineHost
{
    Coroutine StartCoroutine(Behavior owner, IEnumerator routine);
    void StopCoroutine(Behavior owner, Coroutine coroutine);
    void StopCoroutine(Behavior owner, IEnumerator routine);
    void StopAllCoroutines(Behavior owner);
}
