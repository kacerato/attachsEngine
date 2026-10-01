using System.Numerics;

namespace Astra;

/// <summary>Deterministic, per-owner random stream. State is explicitly capturable for replay/save.
/// PCG-XSH-RR 32; Godot 4.5 RandomNumberGenerator is the capability reference.
/// Algorithm/version belongs to Astra and is not sequence-compatible with Godot or Unity.</summary>
public sealed class RandomStream
{
    public const uint AlgorithmVersion = 1;
    public readonly record struct Snapshot(uint Version, ulong State, ulong Increment);
    private ulong _state;
    private ulong _increment;

    public RandomStream(ulong seed, ulong stream = 0)
    {
        if (stream > (ulong.MaxValue >> 1)) throw new ArgumentOutOfRangeException(nameof(stream), "PCG stream has 63 bits.");
        _increment = (stream << 1) | 1; NextUInt(); _state = unchecked(_state + seed); NextUInt();
    }
    public Snapshot Capture() => new(AlgorithmVersion, _state, _increment);
    public void Restore(Snapshot snapshot)
    {
        if (snapshot.Version != AlgorithmVersion || (snapshot.Increment & 1) == 0)
            throw new ArgumentException("Unsupported random state or invalid PCG stream.", nameof(snapshot));
        _state = snapshot.State; _increment = snapshot.Increment;
    }
    public uint NextUInt()
    {
        var old = _state; _state = unchecked(old * 6364136223846793005UL + _increment);
        var value = (uint)(((old >> 18) ^ old) >> 27); var rotation = (int)(old >> 59);
        return (value >> rotation) | (value << ((-rotation) & 31));
    }
    /// <summary>Uniform [0,1), using 24 random bits exactly representable by float.</summary>
    public float Value => (NextUInt() >> 8) * (1f / 16777216f);
    /// <summary>Uniform integer range [minimumInclusive, maximumExclusive), without modulo bias.</summary>
    public int Range(int minimumInclusive, int maximumExclusive)
    {
        if (maximumExclusive <= minimumInclusive) throw new ArgumentOutOfRangeException(nameof(maximumExclusive));
        var bound = (uint)((long)maximumExclusive - minimumInclusive);
        var threshold = unchecked(0u - bound) % bound;
        uint sample; do { sample = NextUInt(); } while (sample < threshold);
        return (int)(minimumInclusive + (long)(sample % bound));
    }
    /// <summary>Finite float range [minimumInclusive, maximumExclusive). Equal endpoints return the endpoint.</summary>
    public float Range(float minimumInclusive, float maximumExclusive)
    {
        if (!float.IsFinite(minimumInclusive) || !float.IsFinite(maximumExclusive) || maximumExclusive < minimumInclusive)
            throw new ArgumentOutOfRangeException(nameof(maximumExclusive));
        if (minimumInclusive == maximumExclusive) return minimumInclusive;
        var value = (float)(minimumInclusive + ((double)maximumExclusive - minimumInclusive) * Value);
        return value < maximumExclusive ? value : MathF.BitDecrement(maximumExclusive);
    }
    /// <summary>Uniform direction on the unit sphere (no rejection loop).</summary>
    public Vector3 OnUnitSphere()
    {
        var z = Range(-1f, 1f); var azimuth = Value * MathF.Tau;
        var radius = MathF.Sqrt(MathF.Max(0, 1 - z * z));
        return new(radius * MathF.Cos(azimuth), radius * MathF.Sin(azimuth), z);
    }
    /// <summary>Uniform volume distribution, not a uniform radius.</summary>
    public Vector3 InsideUnitSphere() => OnUnitSphere() * MathF.Cbrt(Value);
    public Vector2 InsideUnitCircle()
    {
        var angle = Value * MathF.Tau; var radius = MathF.Sqrt(Value);
        return new(radius * MathF.Cos(angle), radius * MathF.Sin(angle));
    }
    /// <summary>Fisher-Yates shuffle, unbiased bounded indices.</summary>
    public void Shuffle<T>(Span<T> values)
    {
        for (var i = values.Length - 1; i > 0; --i)
        {
            var j = Range(0, i + 1); (values[i], values[j]) = (values[j], values[i]);
        }
    }
    /// <summary>Normal distribution using Box-Muller; zero deviation returns the mean.</summary>
    public float Gaussian(float mean = 0, float standardDeviation = 1)
    {
        if (!float.IsFinite(mean) || !float.IsFinite(standardDeviation) || standardDeviation < 0)
            throw new ArgumentOutOfRangeException(nameof(standardDeviation));
        if (standardDeviation == 0) return mean;
        var radius = MathF.Sqrt(-2 * MathF.Log(1 - Value));
        var sample = mean + (double)standardDeviation * radius * MathF.Cos(Value * MathF.Tau);
        if (Math.Abs(sample) > float.MaxValue) throw new ArgumentOutOfRangeException(nameof(standardDeviation), "Normal sample exceeds float range.");
        return (float)sample;
    }
}
