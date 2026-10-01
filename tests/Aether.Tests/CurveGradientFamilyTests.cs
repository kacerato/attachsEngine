using Astra;

namespace Aether.Tests;

public static class CurveGradientFamilyTests
{
    [Test]
    public static void Curve_RuntimeEditsModesWrapExtremesAndInvalidDataHaveRealEffects()
    {
        var curve = AnimationCurve.Linear(0, 0, 1, 2);
        Assert.Close(.5f, curve.Evaluate(.25f));
        curve.Keys[1].Value = 4;
        Assert.Close(1, curve.Evaluate(.25f), what: "Linear follows the current adjacent value, not cached tangents");
        curve.Keys[0].RightMode = curve.Keys[1].LeftMode = TangentMode.Auto;
        Assert.Close(.625f, curve.Evaluate(.25f), what: "Auto endpoints flatten after script mutation");
        curve.Keys[0].RightMode = TangentMode.Constant;
        Assert.Close(0, curve.Evaluate(.9f)); Assert.Close(4, curve.Evaluate(1));
        curve.Keys[0].RightMode = curve.Keys[1].LeftMode = TangentMode.Linear;
        curve.PreWrapMode = CurveWrapMode.Loop; curve.PostWrapMode = CurveWrapMode.PingPong;
        Assert.Close(3, curve.Evaluate(-.25f)); Assert.Close(3, curve.Evaluate(1.25f));
        var peak = new AnimationCurve { Keys = [new(0, 0), new(.5f, 1), new(1, 0)] };
        for (int i = 0; i < peak.Length; ++i) peak.Keys[i].LeftMode = peak.Keys[i].RightMode = TangentMode.ClampedAuto;
        for (int i = 0; i <= 100; ++i) Assert.True(peak.Evaluate(i / 100f) <= 1.00001f);
        peak.Keys[1].Value = -1;
        Assert.True(peak.Evaluate(.4f) >= -1, "ClampedAuto reevaluates the changed extremum");
        var huge = AnimationCurve.Linear(-float.MaxValue, -float.MaxValue, float.MaxValue, float.MaxValue);
        Assert.Close(0, huge.Evaluate(0)); huge.PostWrapMode = CurveWrapMode.PingPong;
        Assert.True(float.IsFinite(huge.Evaluate(float.MaxValue)));
        var free = new AnimationCurve { Keys = [new(0, 0, 0, 4), new(1, 0, 0, 0)] };
        Assert.Close(0, free.Evaluate(.5f), what: "unbroken Free aligns outgoing to incoming");
        free.Keys[0].Broken = true; Assert.Close(.5f, free.Evaluate(.5f), what: "broken Free consumes independent outgoing tangent");
        Assert.NoAlloc(() => { for (int i = 0; i < 1000; ++i) _ = curve.Evaluate(i * .001f); }, "per-frame sampling");
        Assert.Throws<ArgumentOutOfRangeException>(() => curve.Evaluate(float.NaN));
        curve.Keys[1].Time = 0; Assert.Throws<InvalidOperationException>(() => curve.Evaluate(.5f));
        Assert.Throws<ArgumentException>(() => AnimationCurve.Linear(1, 0, 1, 1));
    }

    [Test]
    public static void Gradient_MutableUnsortedStopsModesLinearHdrAndAlphaSampleWithoutAllocations()
    {
        var gradient = new Gradient
        {
            ColorKeys = [new(new Color(0, 1, 0), 1), new(new Color(1, 0, 0), 0)],
            AlphaKeys = [new(0, 1), new(1, 0)]
        };
        var mid = gradient.Evaluate(.5f); Assert.Close(.5f, mid.R); Assert.Close(.5f, mid.G); Assert.Close(.5f, mid.A);
        gradient.ColorKeys[1].Color.R = 4;
        Assert.Close(2, gradient.Evaluate(.5f).R, what: "linear HDR channels survive script edits");
        gradient.Mode = GradientMode.Fixed;
        mid = gradient.Evaluate(.5f); Assert.Close(0, mid.R); Assert.Close(1, mid.G); Assert.Close(0, mid.A);
        gradient.ColorKeys = [new(Color.Black, 0), new(Color.White, 1)];
        gradient.Mode = GradientMode.PerceptualBlend;
        Assert.Close(.125f, gradient.Evaluate(.5f).R, .001f); Assert.Close(.5f, gradient.Evaluate(.5f).A);
        Assert.NoAlloc(() => { for (int i = 0; i < 1000; ++i) _ = gradient.Evaluate(i * .001f); }, "no sorted-array allocation per sample");
        Assert.Throws<ArgumentOutOfRangeException>(() => gradient.Evaluate(float.PositiveInfinity));
        gradient.AlphaKeys[0].Alpha = -1; Assert.Throws<InvalidOperationException>(() => gradient.Evaluate(.5f));
    }
}
