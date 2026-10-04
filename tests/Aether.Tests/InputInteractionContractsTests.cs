using System.Numerics;
using System.Runtime.InteropServices;
using Astra;
namespace Aether.Tests;
public static class InputInteractionContractsTests
{
    // SDK transport recorder only. Native acceptance separately measures the
    // real input state machine, scene archive and touch-to-Play consumer.
    private sealed class Transport : ISceneAccess
    {
        public InputDeviceGroups Groups=InputDeviceGroups.All;
        public bool Enabled=true;
        public bool InputActionCommand(string action,uint op,ref InputActionState state)
        {
            var bytes=MemoryMarshal.AsBytes(MemoryMarshal.CreateSpan(ref state,1));
            Assert.Equal(32,bytes.Length);Assert.Equal(32u,MemoryMarshal.Read<uint>(bytes));
            if(op>=3) {
                if(op==4)Groups=state.DeviceGroups;
                state.DeviceGroups=Groups;return true;
            }
            if(action!="Hold")return false;
            if(op==1)Enabled=(MemoryMarshal.Read<uint>(bytes[4..])&1)!=0;
            if(op==2)Enabled=true;
            uint flags=Enabled?3u:2u;MemoryMarshal.Write(bytes[4..],in flags);
            state.Interaction=InputInteraction.Hold;state.DeviceGroups=InputDeviceGroups.Touch;
            state.Phase=Enabled?InputPhase.Started:InputPhase.Disabled;
            state.Duration=.5f;state.Elapsed=.25f;state.Progress=.5f;return true;
        }
        public bool Exists(ulong id)=>true;
        public TransformValue GetTransform(ulong id)=>default;
        public bool SetTransform(ulong id,TransformValue value)=>false;
        public bool SetBodyVelocity(ulong id,Vector3 value)=>false;
        public bool MoveKinematic(ulong id,Vector3 position,Quaternion rotation)=>false;
        public void Log(ulong id,string message){}
    }
    [Test]
    public static void InputInteractions_TypedTransportLayoutRuntimePoliciesAndFailures()
    {
        var scene=new Transport();var input=new InputAccess(scene);
        Assert.Equal(32,Marshal.SizeOf<InputActionState>());
        Assert.Equal(20,Marshal.OffsetOf<InputActionState>(nameof(InputActionState.Duration)).ToInt32());
        var previous=Marshal.OffsetOf<Astra.Runtime.NativeBehaviorRuntime.SceneAccess>("ObjectLayer").ToInt64();
        var appended=Marshal.OffsetOf<Astra.Runtime.NativeBehaviorRuntime.SceneAccess>("InputActionCommand").ToInt64();
        Assert.Equal(previous+IntPtr.Size,appended);
        Assert.Equal(appended+2*IntPtr.Size,(long)Marshal.SizeOf<Astra.Runtime.NativeBehaviorRuntime.SceneAccess>());
        var state=input.ActionState("Hold");Assert.True(state.Enabled&&state.AuthoredEnabled);
        Assert.Equal(InputInteraction.Hold,state.Interaction);Assert.Equal(InputPhase.Started,state.Phase);
        Assert.Close(.5f,state.Progress);Assert.Close(.25f,state.Elapsed);
        Assert.True(input.SetActionEnabled("Hold",false));state=input.ActionState("Hold");
        Assert.False(state.Enabled);Assert.True(state.AuthoredEnabled);Assert.Equal(InputPhase.Disabled,state.Phase);
        Assert.True(input.RestoreActionEnabled("Hold"));Assert.True(input.ActionState("Hold").Enabled);
        input.DeviceGroups=InputDeviceGroups.KeyboardMouse|InputDeviceGroups.Gamepad;
        Assert.Equal((InputDeviceGroups)6,input.DeviceGroups);
        Assert.Throws<ArgumentOutOfRangeException>(()=>input.DeviceGroups=(InputDeviceGroups)8);
        Assert.Equal((InputDeviceGroups)6,scene.Groups);
        Assert.Throws<ArgumentException>(()=>input.ActionState("Missing"));
        Assert.False(input.SetActionEnabled("Missing",true));
    }
}
