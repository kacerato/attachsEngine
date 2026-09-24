namespace Astra;

/// <summary>Wrap Mode do clipe, na ordem do componente nativo `astra.animation`.</summary>
public enum AnimationWrapMode : uint { Once, Loop, PingPong, ClampForever }

/// <summary>
/// Reprodutor de um componente Animation (`astra.animation`) no mundo Play, no modelo do
/// Animation legado da Unity: Play, Stop, Rewind, IsPlaying e o estado do clipe
/// (tempo, velocidade, repetição). O clipe vem da fonte importada do objeto; os
/// canais animam os nós da mesma instância abaixo dele. Tocando e tempo são estado
/// de execução: o Play parte sempre do zero.
/// </summary>
public readonly struct AnimationPlayer(Component component)
{
    public Component Component => component;
    public bool IsPlaying => component.GetBool("playing");
    /// <summary>Toca do tempo atual; use <see cref="Rewind"/> para recomeçar.</summary>
    public void Play() => component.SetBool("playing", true);
    /// <summary>Para e volta ao início, como Animation.Stop da Unity. A pose atual fica.</summary>
    public void Stop() { component.SetBool("playing", false); component.SetFloat("time", 0); }
    public void Rewind() => component.SetFloat("time", 0);
    /// <summary>Tempo do clipe em segundos (AnimationState.time).</summary>
    public float Time { get => component.GetFloat("time"); set => component.SetFloat("time", value); }
    /// <summary>Multiplicador de tempo entre -10 e 10 (AnimationState.speed).</summary>
    public float Speed { get => component.GetFloat("speed"); set => component.SetFloat("speed", value); }
    /// <summary>Índice do clipe na fonte importada.</summary>
    public uint Clip
    {
        get => (uint)component.GetFloat("clip");
        set => component.SetFloat("clip", value);
    }
    public AnimationWrapMode WrapMode
    {
        get => (AnimationWrapMode)component.GetEnum("wrap_mode");
        set => component.SetEnum("wrap_mode", (uint)value);
    }
    public bool PlayAutomatically
    {
        get => component.GetBool("play_automatically");
        set => component.SetBool("play_automatically", value);
    }
}
