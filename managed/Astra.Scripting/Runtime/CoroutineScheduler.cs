using System.Collections;
using System.Diagnostics;

namespace Astra.Runtime;

/// <summary>One main-thread scheduler per Play session. No Tasks, native handles or global script roots.</summary>
internal sealed class CoroutineScheduler(Func<Behavior, bool> mayRun, Action<Coroutine, string, Exception> report)
{
    private sealed class Routine(Behavior owner, IEnumerator root, Coroutine handle)
    {
        public readonly Behavior Owner = owner;
        public readonly IEnumerator Root = root;
        public readonly Stack<IEnumerator> Stack = new();
        public readonly Coroutine Handle = handle;
        public bool Executing;
        public long ResumeFrame;
        public long ResumeFixedStep;
        public object? Wait;
        public double Deadline;
    }
    private readonly object _scope = new();
    private readonly List<Routine> _routines = [];
    private readonly Dictionary<IEnumerator, Routine> _iterators = new(ReferenceEqualityComparer.Instance);
    private readonly HashSet<Behavior> _blocked = new(ReferenceEqualityComparer.Instance);
    private readonly long _origin = Stopwatch.GetTimestamp();
    private readonly int _thread = Environment.CurrentManagedThreadId;
    private long _frame;
    private long _fixedStep;
    private double _simulationTime;
    private bool _ticking, _stopped;
    private int _cleanupDepth, _executionDepth;
    private double Realtime => Stopwatch.GetElapsedTime(_origin).TotalSeconds;

    public void BeginFrame(float delta)
    {
        CheckThread();
        if (_ticking || _executionDepth != 0) throw new InvalidOperationException("Recursive coroutine frame dispatch.");
        ++_frame; _simulationTime += delta;
    }

    public Coroutine Start(Behavior owner, IEnumerator root)
    {
        CheckThread();
        ArgumentNullException.ThrowIfNull(root);
        if (_stopped || _cleanupDepth != 0 || _blocked.Contains(owner) || !mayRun(owner))
            throw new InvalidOperationException("Cannot start a coroutine on an inactive, retired or cleaning-up Behavior.");
        if (_routines.Count >= 4096 || _executionDepth >= 32) throw new InvalidOperationException("Coroutine limit reached.");
        if (_iterators.ContainsKey(root)) throw new InvalidOperationException("IEnumerator already belongs to a running coroutine.");
        var handle = new Coroutine(_scope, owner.ObjectId, owner.InstanceId);
        var routine = new Routine(owner, root, handle); routine.Stack.Push(root);
        _routines.Add(routine); _iterators.Add(root, routine);
        if (root is AwaitIterator awaitRoot && awaitRoot.CancellationRequested) Cancel(routine); else Advance(routine);
        Compact(); return handle;
    }

    public void Stop(Behavior owner, Coroutine handle)
    {
        CheckThread();
        ArgumentNullException.ThrowIfNull(handle);
        if (!ReferenceEquals(handle.Scope, _scope) || handle.ObjectId != owner.ObjectId || handle.InstanceId != owner.InstanceId)
            throw new ArgumentException("Coroutine belongs to a different Behavior or Play session.", nameof(handle));
        var routine = _routines.Find(r => ReferenceEquals(r.Handle, handle));
        if (routine is not null) Cancel(routine);
        Compact();
    }
    public void Stop(Behavior owner, IEnumerator root)
    {
        CheckThread();
        ArgumentNullException.ThrowIfNull(root);
        if (_iterators.TryGetValue(root, out var routine))
        {
            if (!ReferenceEquals(routine.Owner, owner) || !ReferenceEquals(routine.Root, root))
                throw new ArgumentException("StopCoroutine requires this Behavior's root IEnumerator.", nameof(root));
            Cancel(routine); Compact();
        }
    }
    public void CancelOwner(Behavior owner)
    {
        CheckThread();
        var added = _blocked.Add(owner);
        try
        {
            // Complete owner awaits before disposing Task observers: their async finally
            // blocks finish on this thread and a fault can still reach the observer diagnostic.
            for (int i = 0, count = _routines.Count; i < count; ++i)
                if (ReferenceEquals(_routines[i].Owner, owner) && _routines[i].Root is AwaitIterator) Cancel(_routines[i]);
            for (int i = 0, count = _routines.Count; i < count; ++i)
                if (ReferenceEquals(_routines[i].Owner, owner)) Cancel(_routines[i]);
        }
        finally { if (added) _blocked.Remove(owner); Compact(); }
    }
    public void Shutdown()
    {
        CheckThread();
        _stopped = true;
        for (int i = 0, count = _routines.Count; i < count; ++i)
            if (_routines[i].Root is AwaitIterator) Cancel(_routines[i]);
        for (int i = 0, count = _routines.Count; i < count; ++i) Cancel(_routines[i]);
        Compact();
    }

    public void Tick() => DispatchWaits(false);
    public void BeginFixedStep()
    {
        CheckThread();
        if (_ticking || _executionDepth != 0) throw new InvalidOperationException("Recursive coroutine fixed-step dispatch.");
        ++_fixedStep;
    }
    public void TickFixed() => DispatchWaits(true);
    private void DispatchWaits(bool physicalStep)
    {
        CheckThread();
        if (_stopped) return;
        if (_ticking) throw new InvalidOperationException("Recursive coroutine dispatch.");
        _ticking = true;
        try
        {
            for (int i = 0, count = _routines.Count; i < count; ++i)
            {
                var routine = _routines[i];
                if (!routine.Handle.IsRunning || routine.Executing) continue;
                try
                {
                    if (!mayRun(routine.Owner)) { Cancel(routine); continue; }
                    if (routine.Root is AwaitIterator awaiting && awaiting.CancellationRequested) { Cancel(routine); continue; }
                    if (physicalStep != (routine.Wait is WaitForFixedUpdate)) continue;
                    if (physicalStep && routine.ResumeFixedStep > _fixedStep) continue;
                    if (!physicalStep && routine.ResumeFrame > _frame) continue;
                    if (Ready(routine) && routine.Handle.IsRunning) Advance(routine);
                }
                catch (Exception error) { Fail(routine, "Coroutine.Wait", error); }
            }
        }
        finally { _ticking = false; Compact(); }
    }
    private bool Ready(Routine routine) => routine.Wait switch
    {
        WaitForSeconds => _simulationTime >= routine.Deadline,
        WaitForSecondsRealtime => Realtime >= routine.Deadline,
        WaitUntil until => until.Predicate(),
        WaitWhile whileWait => !whileWait.Predicate(),
        Coroutine child => !child.IsRunning,
        _ => true
    };

    private void Advance(Routine routine)
    {
        if (routine.Executing || !routine.Handle.IsRunning) return;
        routine.Executing = true; ++_executionDepth;
        try
        {
            routine.Wait = null;
            for (var steps = 0; routine.Handle.IsRunning; ++steps)
            {
                if (steps >= 1024) throw new InvalidOperationException("Coroutine exceeded 1024 immediate steps without a frame yield.");
                if (!mayRun(routine.Owner)) { Cancel(routine); break; }
                if (routine.Stack.Count == 0) { routine.Handle.Status = CoroutineStatus.Completed; break; }
                var iterator = routine.Stack.Peek(); var moved = iterator.MoveNext();
                if (!routine.Handle.IsRunning) break; // Stop/Remove may be called from MoveNext.
                if (!moved)
                {
                    routine.Stack.Pop(); _iterators.Remove(iterator); DisposeIterator(routine, iterator);
                    continue;
                }
                var yielded = iterator.Current;
                if (yielded is IEnumerator nested)
                {
                    if (routine.Stack.Count >= 64 || _iterators.ContainsKey(nested))
                        throw new InvalidOperationException("Nested IEnumerator cycle, shared ownership or depth limit.");
                    routine.Stack.Push(nested); _iterators.Add(nested, routine); continue;
                }
                if (yielded is Coroutine child)
                {
                    if (!ReferenceEquals(child.Scope, _scope) || ReferenceEquals(child, routine.Handle))
                        throw new InvalidOperationException("Cannot await a foreign-session coroutine or self.");
                    // Refuse a cycle in chains of coroutine handles rather than leaking a permanent wait.
                    for (var next = child; next.IsRunning;)
                    {
                        var dependency = _routines.Find(r => ReferenceEquals(r.Handle, next));
                        if (dependency?.Wait is not Coroutine following) break;
                        if (ReferenceEquals(following, routine.Handle)) throw new InvalidOperationException("Coroutine wait cycle.");
                        next = following;
                    }
                    if (!child.IsRunning) continue;
                }
                if (yielded is not null && yielded is not WaitForSeconds && yielded is not WaitForSecondsRealtime &&
                    yielded is not WaitUntil && yielded is not WaitWhile && yielded is not Coroutine && yielded is not WaitForFixedUpdate)
                    throw new InvalidOperationException("Unsupported coroutine yield: " + yielded.GetType().FullName);
                routine.Wait = yielded; routine.ResumeFrame = _frame + 1;
                routine.ResumeFixedStep = _fixedStep + 1;
                routine.Deadline = yielded switch {
                    WaitForSeconds wait => _simulationTime + wait.Seconds,
                    WaitForSecondsRealtime wait => Realtime + wait.Seconds,
                    _ => 0
                };
                if (!mayRun(routine.Owner)) Cancel(routine);
                break;
            }
        }
        catch (OperationCanceledException) { Cancel(routine); }
        catch (Exception error) { Fail(routine, "Coroutine.MoveNext", error); }
        finally
        {
            routine.Executing = false; --_executionDepth;
            if (!routine.Handle.IsRunning) Cleanup(routine);
        }
    }
    private void Cancel(Routine routine)
    {
        if (!routine.Handle.IsRunning) return;
        routine.Handle.Status = CoroutineStatus.Cancelled;
        if (!routine.Executing) Cleanup(routine);
    }
    private void Fail(Routine routine, string phase, Exception error)
    {
        routine.Handle.Status = CoroutineStatus.Failed; routine.Handle.Failure = error.ToString();
        report(routine.Handle, phase, error);
        if (!routine.Executing) Cleanup(routine);
    }
    private void Cleanup(Routine routine)
    {
        routine.Wait = null;
        while (routine.Stack.TryPop(out var iterator))
        {
            _iterators.Remove(iterator); DisposeIterator(routine, iterator);
        }
    }
    private void DisposeIterator(Routine routine, IEnumerator iterator)
    {
        if (iterator is not IDisposable disposable) return;
        ++_cleanupDepth;
        try { disposable.Dispose(); }
        catch (Exception error)
        {
            routine.Handle.Status = CoroutineStatus.Failed; routine.Handle.Failure = error.ToString();
            report(routine.Handle, "Coroutine.Dispose", error);
        }
        finally { --_cleanupDepth; }
    }
    private void Compact()
    {
        // Reentrant user code may stop/start while the list is being indexed.
        if (!_ticking && _executionDepth == 0 && _cleanupDepth == 0)
            _routines.RemoveAll(r => !r.Handle.IsRunning && !r.Executing);
    }
    private void CheckThread()
    {
        if (Environment.CurrentManagedThreadId != _thread)
            throw new InvalidOperationException("Coroutine scheduling belongs to the script dispatch thread.");
    }
}
