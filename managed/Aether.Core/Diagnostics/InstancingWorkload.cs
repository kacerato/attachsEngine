namespace Aether.Core.Diagnostics;

/// <summary>
/// Deterministic M0 workload, not a gameplay component or renderer resource.
/// Immutable layout/color data is prepared once. The render owner fills its own
/// buffer without allocations; animation and the five-float ABI remain unchanged.
/// </summary>
public sealed class InstancingWorkload
{
    public const int FloatsPerInstance = 5;
    private readonly float[] _layout;
    public int InstanceCount { get; }

    public InstancingWorkload(int instanceCount)
    {
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(instanceCount);
        InstanceCount = instanceCount;
        _layout = new float[checked(instanceCount * FloatsPerInstance)];
        int gridSize = (int)MathF.Ceiling(MathF.Sqrt(instanceCount));
        for (int i = 0; i < instanceCount; ++i)
        {
            int offset = i * FloatsPerInstance;
            float phase = i * 0.017f;
            _layout[offset] = gridSize == 1 ? 0 : (i % gridSize / (float)(gridSize - 1) - 0.5f) * 1.8f;
            _layout[offset + 1] = gridSize == 1 ? 0 : (i / gridSize / (float)(gridSize - 1) - 0.5f) * 1.8f;
            _layout[offset + 2] = 0.5f + 0.5f * MathF.Sin(phase);
            _layout[offset + 3] = 0.5f + 0.5f * MathF.Sin(phase + 2.094f);
            _layout[offset + 4] = 0.5f + 0.5f * MathF.Sin(phase + 4.188f);
        }
    }

    public void Fill(Span<float> destination, float timeSeconds)
    {
        if (destination.Length < _layout.Length) throw new ArgumentException("Instance buffer too small.", nameof(destination));
        for (int i = 0; i < InstanceCount; ++i)
        {
            int offset = i * FloatsPerInstance;
            float angle = timeSeconds * 2f + i * 0.017f;
            destination[offset] = _layout[offset] + MathF.Cos(angle) * 0.01f;
            destination[offset + 1] = _layout[offset + 1] + MathF.Sin(angle) * 0.01f;
            destination[offset + 2] = _layout[offset + 2];
            destination[offset + 3] = _layout[offset + 3];
            destination[offset + 4] = _layout[offset + 4];
        }
    }
}
