using System.Runtime.InteropServices;
using Astra.Runtime;
namespace Aether.Tests;
public static class Bulk50AbiTests
{
    [Test] public static void AppendedBodyCallbackAndSnapshotMatchNativeLayout()
    {
        // Protects a real ABI regression: pointer order/packing and reservations.
        Assert.Equal(48, Marshal.SizeOf<NativeBehaviorRuntime.NativeBodyState>());
        Assert.Equal(8L, Marshal.OffsetOf<NativeBehaviorRuntime.NativeBodyState>("Linear").ToInt64());
        Assert.Equal(20L, Marshal.OffsetOf<NativeBehaviorRuntime.NativeBodyState>("Angular").ToInt64());
        Assert.Equal(32L, Marshal.OffsetOf<NativeBehaviorRuntime.NativeBodyState>("CenterOfMass").ToInt64());
        Assert.Equal(44L, Marshal.OffsetOf<NativeBehaviorRuntime.NativeBodyState>("Reserved").ToInt64());
        var previous = Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("CharacterSnapshot").ToInt64();
        var current = Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("BodyCommand").ToInt64();
        Assert.Equal(previous + IntPtr.Size, current);
        var field = Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("FieldQuery").ToInt64();
        Assert.Equal(current + IntPtr.Size, field);
        var layer=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("ObjectLayer").ToInt64();
        Assert.Equal(field+IntPtr.Size,layer);
    }
}
