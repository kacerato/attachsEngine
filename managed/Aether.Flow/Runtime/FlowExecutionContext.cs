using Aether.Physics;

namespace Aether.Flow.Runtime;

/// <summary>
/// Capacidades externas que um nó Flow pode consumir. O conjunto é declarado
/// na AST e comparado com os serviços injetados antes da execução; assim um
/// grafo de runtime não depende de singletons nem descobre serviços por acaso.
/// </summary>
[Flags]
public enum FlowCapability
{
    None = 0,
    World = 1 << 0,
    Physics = 1 << 1,
    Time = 1 << 2,
    Input = 1 << 3,
    Logging = 1 << 4,
}

public interface IFlowTimeSource
{
    double ElapsedSeconds { get; }
    float DeltaTime { get; }
}

public interface IFlowInputSource
{
    bool IsActionPressed(string action);
    float GetActionStrength(string action);
}

public enum FlowLogLevel
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
}

public interface IFlowLogSink
{
    void Write(FlowLogLevel level, string message, string nodeId);
}

/// <summary>
/// Fronteira explícita entre a execução do AetherFlow e o runtime da engine.
/// Uma instância pertence a uma execução/Behavior e pode ser substituída por
/// doubles em teste. Nenhum serviço é global e a ausência é um estado válido,
/// diagnosticado pela capability do nó que tentou usá-lo.
/// </summary>
public sealed class FlowExecutionContext
{
    public World? World { get; }
    public PhysicsWorld? PhysicsWorld { get; }
    public IFlowTimeSource? Time { get; }
    public IFlowInputSource? Input { get; }
    public IFlowLogSink? Logger { get; }

    public FlowCapability AvailableCapabilities { get; }

    public FlowExecutionContext(
        World? world = null,
        PhysicsWorld? physicsWorld = null,
        IFlowTimeSource? time = null,
        IFlowInputSource? input = null,
        IFlowLogSink? logger = null)
    {
        World = world;
        PhysicsWorld = physicsWorld;
        Time = time;
        Input = input;
        Logger = logger;

        FlowCapability available = FlowCapability.None;
        if (world is not null) available |= FlowCapability.World;
        if (physicsWorld is not null) available |= FlowCapability.Physics;
        if (time is not null) available |= FlowCapability.Time;
        if (input is not null) available |= FlowCapability.Input;
        if (logger is not null) available |= FlowCapability.Logging;
        AvailableCapabilities = available;
    }

    public bool Supports(FlowCapability required) =>
        (AvailableCapabilities & required) == required;

    public FlowCapability Missing(FlowCapability required) =>
        required & ~AvailableCapabilities;

    public void Require(FlowCapability required, string nodeId)
    {
        FlowCapability missing = Missing(required);
        if (missing != FlowCapability.None)
            throw new FlowCapabilityException(nodeId, missing);
    }
}

public sealed class FlowCapabilityException : InvalidOperationException
{
    public string NodeId { get; }
    public FlowCapability MissingCapabilities { get; }

    public FlowCapabilityException(string nodeId, FlowCapability missing)
        : base($"O bloco \"{nodeId}\" precisa de serviços que não foram fornecidos: {missing}.")
    {
        NodeId = nodeId;
        MissingCapabilities = missing;
    }
}
