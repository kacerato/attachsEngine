using System.Runtime.InteropServices;
using Astra.Runtime;
namespace Aether.Tests;
public static class PhysicsFieldsAbiTests
{
    [Test] public static void FieldSampleAndAppendedCallbackPreserveAbi()
    {
        Assert.Equal(64,Marshal.SizeOf<NativeBehaviorRuntime.NativeFieldState>());
        Assert.Equal(12L,Marshal.OffsetOf<NativeBehaviorRuntime.NativeFieldState>("Acceleration").ToInt64());
        Assert.Equal(24L,Marshal.OffsetOf<NativeBehaviorRuntime.NativeFieldState>("WindVelocity").ToInt64());
        Assert.Equal(52L,Marshal.OffsetOf<NativeBehaviorRuntime.NativeFieldState>("AffectedBodies").ToInt64());
        Assert.Equal(60L,Marshal.OffsetOf<NativeBehaviorRuntime.NativeFieldState>("Reserved").ToInt64());
        var body=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("BodyCommand").ToInt64();
        var field=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("FieldQuery").ToInt64();
        Assert.Equal(body+IntPtr.Size,field);
        var layer=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("ObjectLayer").ToInt64();
        Assert.Equal(field+IntPtr.Size,layer);
        var input=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("InputActionCommand").ToInt64();
        Assert.Equal(layer+IntPtr.Size,input);
        Assert.Equal(input+IntPtr.Size,(long)Marshal.SizeOf<NativeBehaviorRuntime.SceneAccess>());
    }
}
