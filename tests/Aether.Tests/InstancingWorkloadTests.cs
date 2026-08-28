using Aether.Core.Diagnostics;

namespace Aether.Tests;

public static class InstancingWorkloadTests
{
    [Test]
    public static void CachedLayout_MatchesOriginalForEveryInstanceAndTime()
    {
        foreach (int count in new[] { 2, 17, 4999, 5000 })
        {
            var workload = new InstancingWorkload(count);
            float[] actual = new float[count * 5];
            foreach (float time in new[] { 0f, 0.016f, 37f, 1800f })
            {
                workload.Fill(actual, time);
                for (int i = 0; i < count; ++i)
                {
                    int grid = (int)MathF.Ceiling(MathF.Sqrt(count));
                    float phase = i * 0.017f;
                    Assert.Equal((i % grid / (float)(grid - 1) - 0.5f) * 1.8f + MathF.Cos(time * 2f + phase) * 0.01f, actual[i * 5]);
                    Assert.Equal((i / grid / (float)(grid - 1) - 0.5f) * 1.8f + MathF.Sin(time * 2f + phase) * 0.01f, actual[i * 5 + 1]);
                    Assert.Equal(0.5f + 0.5f * MathF.Sin(phase), actual[i * 5 + 2]);
                    Assert.Equal(0.5f + 0.5f * MathF.Sin(phase + 2.094f), actual[i * 5 + 3]);
                    Assert.Equal(0.5f + 0.5f * MathF.Sin(phase + 4.188f), actual[i * 5 + 4]);
                }
            }
        }
    }

    [Test]
    public static void Fill_AllocatesNothingInSteadyState()
    {
        var workload = new InstancingWorkload(5000);
        float[] buffer = new float[25000];
        Assert.NoAlloc(() => { for (int i = 0; i < 120; ++i) workload.Fill(buffer, i / 60f); });
    }

    [Test]
    public static void OneInstance_IsFiniteAndCentered()
    {
        var workload = new InstancingWorkload(1);
        float[] buffer = new float[5];
        workload.Fill(buffer, 0);
        Assert.Equal(0.01f, buffer[0]); Assert.Equal(0f, buffer[1]);
    }

    [Test]
    public static void Fill_ValidatesSizeAndPreservesBufferTail()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new InstancingWorkload(0));
        Assert.Throws<OverflowException>(() => new InstancingWorkload(int.MaxValue));
        var workload = new InstancingWorkload(1);
        Assert.Throws<ArgumentException>(() => workload.Fill(new float[4], 0));
        float[] buffer = new float[6]; buffer[5] = 123;
        workload.Fill(buffer, 1);
        Assert.Equal(123f, buffer[5]);
    }

    [Test]
    public static unsafe void NativeEntryPoint_PreservesAbiAndCountCompatibility()
    {
        delegate* unmanaged<float*, int, float, void> fill = &Aether.Interop.NativeEntryPoints.FillInstanceBuffer;
        foreach (int count in new[] { 1, 17, 5000 })
        {
            var workload = new InstancingWorkload(count);
            float[] expected = new float[count * 5];
            workload.Fill(expected, 17f);
            float[] actual = new float[count * 5 + 1];
            actual[^1] = 123f;
            fixed (float* buffer = actual) fill(buffer, count, 17f);
            for (int i = 0; i < expected.Length; ++i) Assert.Equal(expected[i], actual[i]);
            Assert.Equal(123f, actual[^1]);
        }
    }

    [Test]
    public static unsafe void NativeEntryPoint_RejectsEmptyInputsWithoutWrites()
    {
        delegate* unmanaged<float*, int, float, void> fill = &Aether.Interop.NativeEntryPoints.FillInstanceBuffer;
        float sentinel = 123f;
        fill(null, 5000, 0);
        fill(&sentinel, 0, 0);
        fill(&sentinel, -1, 0);
        Assert.Equal(123f, sentinel);
    }
}
