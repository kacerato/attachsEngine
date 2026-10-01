namespace Astra;

public enum AudioVoiceState { Stopped, Playing, Paused, MissingClip, MissingListener, InvalidBus, Limit, DeviceError, InvalidPose }
/// <summary>Observed backend cursor in seconds. OutputRunning describes the device gate, never audibility.</summary>
public readonly record struct AudioVoiceSnapshot(AudioVoiceState State,double Cursor,bool OutputRunning);
public interface IAudioVoiceAccess
{
    bool QueryAudioVoice(ulong id,uint world,uint generation,ulong instance,out AudioVoiceSnapshot snapshot);
}
/// <summary>Controls an authored source and reads its observed native voice. Seeking is not supported.</summary>
public readonly struct AudioVoice
{
    private readonly Component component;
    public AudioVoice(Component component)
    {
        if(component.TypeId!="astra.audio.source") throw new ArgumentException("Component must be AudioSource.",nameof(component));
        this.component=component;
    }
    public AudioVoiceSnapshot Snapshot
    {
        get
        {
            if(component.Object is null) throw new InvalidOperationException("Uninitialized audio voice access.");
            if(component.Scene is not IAudioVoiceAccess access) throw new NotSupportedException("Host does not provide observed audio voices.");
            if(!access.QueryAudioVoice(component.Object.ObjectId,component.Object.World,component.Object.Generation,component.InstanceId,out var snapshot))
                throw new WorldException(component.Scene.LastStatus,"consultar voz de áudio");
            return snapshot;
        }
    }
    public void Play()=>component.SetEnum("playback",1);
    public void Pause()=>component.SetEnum("playback",2);
    public void Stop()=>component.SetEnum("playback",0);
}
