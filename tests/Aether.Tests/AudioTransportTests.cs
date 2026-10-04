using System.Numerics;
using System.Runtime.InteropServices;
using Astra;
using Astra.Runtime;
namespace Aether.Tests;

public static class AudioTransportTests
{
    // SDK routing only; PCM/backend acceptance lives in test_script_audio.cpp.
    private sealed class Transport : ISceneAccess, IAudioVoiceAccess
    {
        public uint WorldId=>17;
        public uint GenerationOf(ulong id)=>3;
        public bool Exists(ulong id)=>id==1;
        public WorldStatus LastStatus=>WorldStatus.StaleHandle;
        public bool Accept=true;
        public readonly List<(AudioVoiceCommand,double)> Calls=[];
        public TransformValue GetTransform(ulong id)=>default;
        public bool SetTransform(ulong id,TransformValue value)=>false;
        public bool SetBodyVelocity(ulong id,Vector3 value)=>false;
        public bool MoveKinematic(ulong id,Vector3 value,Quaternion rotation)=>false;
        public void Log(ulong id,string message){}
        public ulong FindComponent(ulong id,string type,uint ordinal)=>7;
        public bool QueryAudioVoice(ulong id,uint world,uint generation,ulong instance,out AudioVoiceSnapshot snapshot)
        {snapshot=new(AudioVoiceState.Paused,.5,false);return Accept;}
        public bool CommandAudioVoice(ulong id,uint world,uint generation,ulong instance,AudioVoiceCommand command,double seconds)
        {
            Assert.Equal(1UL,id);Assert.Equal(17u,world);Assert.Equal(3u,generation);Assert.Equal(7UL,instance);
            Calls.Add((command,seconds));return Accept;
        }
    }
    [Test] public static void AudioTransport_RoutesCommandsIdentityErrorsAndAppendedAbi()
    {
        var scene=new Transport();
        var component=GameObject.Resolve(scene,1).GetComponent("astra.audio.source")!.Value;
        var voice=new AudioVoice(component);
        voice.Play();voice.Pause();voice.Resume();voice.Seek(.5);voice.Stop();
        Assert.Equal(5,scene.Calls.Count);
        Assert.Equal(AudioVoiceCommand.Play,scene.Calls[0].Item1);
        Assert.Equal(AudioVoiceCommand.Pause,scene.Calls[1].Item1);
        Assert.Equal(AudioVoiceCommand.Resume,scene.Calls[2].Item1);
        Assert.Equal((AudioVoiceCommand.Seek,.5),scene.Calls[3]);
        Assert.Equal(AudioVoiceCommand.Stop,scene.Calls[4].Item1);
        Assert.Throws<ArgumentOutOfRangeException>(()=>voice.Seek(double.NaN));
        Assert.Throws<ArgumentOutOfRangeException>(()=>voice.Seek(-1));
        Assert.Equal(5,scene.Calls.Count);
        scene.Accept=false;Assert.Throws<WorldException>(()=>voice.Play());
        Assert.Throws<InvalidOperationException>(()=>default(AudioVoice).Play());
        var previous=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("InputActionCommand").ToInt64();
        var command=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("AudioCommand").ToInt64();
        Assert.Equal(previous+IntPtr.Size,command);
        var gui=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("GuiCommand").ToInt64();
        Assert.Equal(command+IntPtr.Size,gui);
        var text=Marshal.OffsetOf<NativeBehaviorRuntime.SceneAccess>("GuiText").ToInt64();
        Assert.Equal(gui+2*IntPtr.Size,text);
        Assert.Equal(text+3*IntPtr.Size,(long)Marshal.SizeOf<NativeBehaviorRuntime.SceneAccess>());
    }
}
