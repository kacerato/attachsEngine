namespace Astra;

public enum AudioVoiceState { Stopped, Playing, Paused, MissingClip, MissingListener, InvalidBus, Limit, DeviceError, InvalidPose }
public enum AudioVoiceCommand : uint { Play, Pause, Stop, Seek, Resume }
/// <summary>Backend cursor in seconds, including pending seeks. OutputRunning describes the device gate, never audibility.</summary>
public readonly record struct AudioVoiceSnapshot(AudioVoiceState State,double Cursor,bool OutputRunning);
public interface IAudioVoiceAccess
{
    bool QueryAudioVoice(ulong id,uint world,uint generation,ulong instance,out AudioVoiceSnapshot snapshot);
    bool CommandAudioVoice(ulong id,uint world,uint generation,ulong instance,AudioVoiceCommand command,double seconds)
        => throw new NotSupportedException("Host does not provide audio transport commands.");
}
/// <summary>Controls the actual native voice. Play restarts; Resume preserves a paused cursor.
/// Seek is applied by the mixer; its pending target may already appear in Snapshot.</summary>
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
    private void Command(AudioVoiceCommand command,double seconds=0)
    {
        if(component.Object is null) throw new InvalidOperationException("Uninitialized audio voice access.");
        if(component.Scene is not IAudioVoiceAccess access) throw new NotSupportedException("Host does not provide audio transport.");
        if(!access.CommandAudioVoice(component.Object.ObjectId,component.Object.World,component.Object.Generation,component.InstanceId,command,seconds))
            throw new WorldException(component.Scene.LastStatus,"comando de áudio");
    }
    public void Play()=>Command(AudioVoiceCommand.Play);
    public void Pause()=>Command(AudioVoiceCommand.Pause);
    public void Resume()=>Command(AudioVoiceCommand.Resume);
    public void Stop()=>Command(AudioVoiceCommand.Stop);
    public void Seek(double seconds)
    {
        if(!double.IsFinite(seconds)||seconds<0)throw new ArgumentOutOfRangeException(nameof(seconds));
        Command(AudioVoiceCommand.Seek,seconds);
    }
}
