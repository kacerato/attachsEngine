using Astra;
using Astra.Components;

/// <summary>
/// G7 · exemplo 1 — ciclo de dia acelerado. Fica no objeto do Sol (Luz
/// direcional + Hora do dia). Velocidade 720 = um dia inteiro em 2 minutos.
/// Equivale na Unity a girar o sol pelo próprio script do ciclo de dia.
/// </summary>
[ComponentId("g7.dia_acelerado")]
public sealed class DiaAcelerado : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 720;
    [PropertyId("hora_inicial")] public float HoraInicial = 16;

    public override void Start()
    {
        var tempo = Object.GetComponent<TimeOfDay>()!.Value;
        tempo.Hour = HoraInicial;          // TimeOfDay.Current.Hour = 16
        tempo.Speed = Velocidade;          // 1 min real = 12 h de jogo
        tempo.HourChanged(this, hora => Scene.Log(ObjectId, $"Hora do dia: {hora:00}:00"));
        tempo.Sunset(this, () => Scene.Log(ObjectId, "Pôr do sol"));
        tempo.Sunrise(this, () => Scene.Log(ObjectId, "Nascer do sol"));
    }
}
