namespace Astra;

/// <summary>Gameplay math in radians, with explicit time and velocity state.
/// Unity 6000.0 Mathf is the capability reference; Astra angles are radians.</summary>
public static class Mathf
{
    public const float Pi = MathF.PI;
    public const float Tau = MathF.Tau;
    public const float Deg2Rad = Pi / 180;
    public const float Rad2Deg = 180 / Pi;
    public static float Clamp(float value, float minimum, float maximum) => Math.Clamp(value, minimum, maximum);
    public static float Clamp01(float value) => Math.Clamp(value, 0, 1);
    public static float Lerp(float from, float to, float t) => LerpUnclamped(from, to, Clamp01(t));
    public static float LerpUnclamped(float from, float to, float t) => (float)(from + ((double)to - from) * t);
    public static float InverseLerp(float from, float to, float value) => from == to ? 0 : Clamp01((float)(((double)value - from) / ((double)to - from)));
    public static float SmoothStep(float from, float to, float t)
    {
        t = Clamp01(t); return LerpUnclamped(from, to, t * t * (3 - 2 * t));
    }
    public static float MoveTowards(float current, float target, float maximumDelta)
    {
        NonNegativeFinite(maximumDelta, nameof(maximumDelta));
        var difference = (double)target - current;
        return Math.Abs(difference) <= maximumDelta ? target : (float)(current + Math.CopySign(maximumDelta, difference));
    }
    public static float Repeat(float value, float length)
    {
        if (!float.IsFinite(length) || length <= 0 || !float.IsFinite(value)) throw new ArgumentOutOfRangeException(nameof(length));
        var result = (double)value % length;
        if (result < 0) result += length;
        return Math.Clamp((float)result, 0, MathF.BitDecrement(length));
    }
    public static float PingPong(float value, float length)
    {
        if (!float.IsFinite(length) || length <= 0 || length > float.MaxValue / 2) throw new ArgumentOutOfRangeException(nameof(length));
        return length - MathF.Abs(Repeat(value, 2 * length) - length);
    }
    /// <summary>Signed shortest displacement in [-pi,pi], angles in radians.</summary>
    public static float DeltaAngle(float current, float target)
    {
        if (!float.IsFinite(current) || !float.IsFinite(target)) throw new ArgumentOutOfRangeException(nameof(target));
        var delta = ((double)target - current) % Tau;
        if (delta < -Pi) delta += Tau;
        if (delta > Pi) delta -= Tau;
        return (float)delta;
    }
    public static float LerpAngle(float from, float to, float t) => from + DeltaAngle(from, to) * Clamp01(t);
    public static float MoveTowardsAngle(float current, float target, float maximumDelta)
        => MoveTowards(current, current + DeltaAngle(current, target), maximumDelta);
    /// <summary>Frame-rate independent half-life damping. Zero half-life snaps to target.</summary>
    public static float Damp(float current, float target, float halfLifeSeconds, float deltaSeconds)
    {
        NonNegativeFinite(halfLifeSeconds, nameof(halfLifeSeconds)); NonNegativeFinite(deltaSeconds, nameof(deltaSeconds));
        if (deltaSeconds == 0) return current;
        if (halfLifeSeconds == 0) return target;
        return LerpUnclamped(target, current, MathF.Exp(-0.6931471805599453f * deltaSeconds / halfLifeSeconds));
    }
    /// <summary>Critically damped spring. Velocity must be retained by the caller; no hidden global time.
    /// Zero delta leaves current value and velocity intact.</summary>
    public static float SmoothDamp(float current, float target, ref float velocity, float smoothSeconds,
        float deltaSeconds, float maximumSpeed = float.PositiveInfinity)
    {
        NonNegativeFinite(smoothSeconds, nameof(smoothSeconds)); NonNegativeFinite(deltaSeconds, nameof(deltaSeconds));
        if (!float.IsFinite(current) || !float.IsFinite(target) || !float.IsFinite(velocity) ||
            float.IsNaN(maximumSpeed) || maximumSpeed < 0) throw new ArgumentOutOfRangeException(nameof(maximumSpeed));
        if (deltaSeconds == 0) return current;
        smoothSeconds = MathF.Max(0.0001f, smoothSeconds);
        var omega = 2.0 / smoothSeconds; var x = omega * deltaSeconds;
        var decay = 1.0 / (1.0 + x + 0.48 * x * x + 0.235 * x * x * x);
        var change = (double)current - target; var originalTarget = (double)target; var maxChange = (double)maximumSpeed * smoothSeconds;
        change = Math.Clamp(change, -maxChange, maxChange); var limitedTarget = current - change;
        var temporary = (velocity + omega * change) * deltaSeconds;
        var nextVelocity = (velocity - omega * temporary) * decay;
        var output = limitedTarget + (change + temporary) * decay;
        if ((originalTarget - current > 0) == (output > originalTarget)) { output = originalTarget; nextVelocity = 0; }
        // Float velocity cannot represent an arbitrary steep finite spring; reject atomically.
        if (Math.Abs(nextVelocity) > float.MaxValue || Math.Abs(output) > float.MaxValue)
            throw new ArgumentOutOfRangeException(nameof(smoothSeconds), "Spring result exceeds float range.");
        velocity = (float)nextVelocity;
        return (float)output;
    }
    public static float SmoothDampAngle(float current, float target, ref float velocity, float smoothSeconds,
        float deltaSeconds, float maximumSpeed = float.PositiveInfinity)
        => SmoothDamp(current, current + DeltaAngle(current, target), ref velocity, smoothSeconds, deltaSeconds, maximumSpeed);
    private static void NonNegativeFinite(float value, string name)
    {
        if (!float.IsFinite(value) || value < 0) throw new ArgumentOutOfRangeException(name);
    }
}
