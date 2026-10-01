using System.Collections;
using System.Runtime.CompilerServices;

namespace Astra;

/// <summary>Awaitable routes bound to one Behavior. The provided waits resume on the script dispatch thread.
/// Arbitrary Tasks (Task.Run/Delay/network) do not acquire that guarantee.</summary>
public readonly struct BehaviorAwaitables
{
    private readonly Behavior _owner;
    private readonly ICoroutineHost _host;
    internal BehaviorAwaitables(Behavior owner, ICoroutineHost host) { _owner = owner; _host = host; }
    public BehaviorAwaitable NextFrame(CancellationToken cancellation = default) => Schedule(null, cancellation);
    public BehaviorAwaitable Seconds(double seconds, CancellationToken cancellation = default) => Schedule(new WaitForSeconds(seconds), cancellation);
    public BehaviorAwaitable SecondsRealtime(double seconds, CancellationToken cancellation = default) => Schedule(new WaitForSecondsRealtime(seconds), cancellation);
    public BehaviorAwaitable FixedUpdate(CancellationToken cancellation = default) => Schedule(new WaitForFixedUpdate(), cancellation);
    private BehaviorAwaitable Schedule(object? wait, CancellationToken cancellation)
    {
        var completion = new BehaviorAwaitable();
        var iterator = new AwaitIterator(completion, wait, cancellation);
        try { _host.StartCoroutine(_owner, iterator); }
        catch { iterator.Dispose(); throw; }
        return completion;
    }
}

/// <summary>Single-consumer await operation. Completion drops its continuation so handles do not retain scripts.</summary>
public sealed class BehaviorAwaitable : INotifyCompletion
{
    private readonly int _thread = Environment.CurrentManagedThreadId;
    private Action? _continuation;
    private bool _registered, _claimed, _completed, _cancelled;
    private CancellationToken _token;
    internal BehaviorAwaitable() { }
    public BehaviorAwaitable GetAwaiter()
    {
        CheckThread();
        if (_claimed) throw new InvalidOperationException("Behavior await operations have one consumer.");
        _claimed = true; return this;
    }
    public bool IsCompleted { get { CheckThread(); return _completed; } }
    public void OnCompleted(Action continuation)
    {
        CheckThread(); ArgumentNullException.ThrowIfNull(continuation);
        if (_registered) throw new InvalidOperationException("Behavior await operations have one consumer.");
        _registered = true;
        if (_completed) continuation(); else _continuation = continuation;
    }
    public void GetResult()
    {
        CheckThread();
        if (!_completed) throw new InvalidOperationException("Await operation is still pending.");
        if (_cancelled) throw new OperationCanceledException("Behavior wait was cancelled by its token or Play lifecycle.", _token);
    }
    internal void Finish(bool cancelled, CancellationToken token = default)
    {
        CheckThread(); if (_completed) return;
        _completed = true; _cancelled = cancelled; _token = token;
        var continuation = _continuation; _continuation = null;
        continuation?.Invoke();
    }
    private void CheckThread()
    {
        if (Environment.CurrentManagedThreadId != _thread)
            throw new InvalidOperationException("Behavior awaits must be created, awaited and resumed on the script dispatch thread.");
    }
}

internal sealed class AwaitIterator(BehaviorAwaitable completion, object? wait, CancellationToken cancellation) : IEnumerator, IDisposable
{
    private bool _yielded;
    public bool CancellationRequested => cancellation.IsCancellationRequested;
    public object? Current => wait;
    public bool MoveNext()
    {
        if (!_yielded) { _yielded = true; return true; }
        completion.Finish(cancellation.IsCancellationRequested, cancellation); return false;
    }
    public void Reset() => throw new NotSupportedException();
    public void Dispose() => completion.Finish(true, cancellation);
}
