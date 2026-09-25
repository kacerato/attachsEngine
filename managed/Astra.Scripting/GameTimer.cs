namespace Astra;

/// <summary>Configuração de uma instância de Timer no mundo de Play.
/// A contagem começa no Play, pausa com o objeto ou o Play, e não modifica a cena autorada.
/// Para diferenciar vários timers no mesmo objeto, compare <see cref="InstanceId"/>
/// com o ID recebido em <see cref="Behavior.TimerElapsed"/>.</summary>
public readonly struct GameTimer(Component component)
{
    public ulong InstanceId => component.InstanceId;
    public float IntervalSeconds
    {
        get => component.GetFloat("interval_seconds");
        set => component.SetFloat("interval_seconds", value);
    }
    public bool Repeat
    {
        get => component.GetBool("repeat");
        set => component.SetBool("repeat", value);
    }
    public bool Enabled
    {
        get => component.GetBool("enabled");
        set => component.SetBool("enabled", value);
    }
}
