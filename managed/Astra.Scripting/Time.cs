namespace Astra;

public interface ITimeHost { TimeAccess Time { get; } }
public readonly record struct SimulationTimeState(ulong FrameCount, double SimulationTime,
    double UnscaledTime, float DeltaTime, float UnscaledDeltaTime, float Scale, float FrameScale);
/// <summary>Native session clock; changing scale must affect the actual simulation.</summary>
public interface ITimeSceneAccess
{
    SimulationTimeState ReadTime();
    WorldStatus SetTimeScale(float value);
}

/// <summary>Session-owned dispatch clock. Reads require the Play thread and an active session.</summary>
public sealed class TimeAccess
{
    private readonly int _thread = Environment.CurrentManagedThreadId;
    private readonly ITimeSceneAccess? _source;
    private bool _active, _fixed;
    private ulong _frame;
    private double _time, _fixedTime;
    private float _delta, _fixedDelta;
    private double _unscaledTime;
    private float _unscaledDelta, _frameScale=1;
    internal TimeAccess(ITimeSceneAccess? source=null) => _source=source;
    private void Check() {
        if (Environment.CurrentManagedThreadId != _thread) throw new InvalidOperationException("Time requires the Play dispatch thread.");
        if (!_active) throw new InvalidOperationException("This Time clock belongs to an ended Play session.");
    }
    public ulong FrameCount { get { Check(); return _frame; } }
    public double TimeSinceStart { get { Check(); return _fixed ? _fixedTime : _time; } }
    public float DeltaTime { get { Check(); return _fixed ? _fixedDelta : _delta; } }
    public double FixedTime { get { Check(); return _fixedTime; } }
    public float FixedDeltaTime { get { Check(); return _fixedDelta; } }
    public bool InFixedTimeStep { get { Check(); return _fixed; } }
    public double UnscaledTimeSinceStart { get { Check(); return _unscaledTime; } }
    public float UnscaledDeltaTime { get { Check(); return _fixed ? (_frameScale>0 ? _fixedDelta / _frameScale : 0) : _unscaledDelta; } }
    /// <summary>0 freezes simulation; 1 is normal; supported range is [0,4]. Takes effect next frame.</summary>
    public float Scale {
        get { Check(); return _source?.ReadTime().Scale ?? 1; }
        set {
            Check();
            if(!float.IsFinite(value) || value<0 || value>4) throw new ArgumentOutOfRangeException(nameof(value));
            if(_source is null) throw new NotSupportedException("This host does not provide native simulation time control.");
            var status=_source.SetTimeScale(value);
            if(status!=WorldStatus.Ok) throw new WorldException(status,"Time.Scale");
        }
    }
    internal void Activate() { _active = true; Check(); }
    internal TimeAccess Validate() { Check(); return this; }
    internal void End() { Check(); _active = false; }
    internal void BeginFrame(float delta) {
        Check(); _fixed = false; _delta = delta;
        if(_source is null) { _time += delta; ++_frame; _unscaledTime+=delta; _unscaledDelta=delta; _frameScale=1; }
        else {
            var state=_source.ReadTime();
            _frame=state.FrameCount;_time=state.SimulationTime;_unscaledTime=state.UnscaledTime;
            _unscaledDelta=state.UnscaledDeltaTime;_frameScale=state.FrameScale;
        }
    }
    internal void BeginFixed(float delta) { Check(); _fixed = true; _fixedDelta = delta; _fixedTime += delta; }
    internal void EndFixed() { Check(); _fixed = false; }
    internal void FramePhase() { Check(); _fixed = false; }
}
