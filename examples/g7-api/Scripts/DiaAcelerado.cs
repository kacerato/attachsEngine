using Astra;
using Astra.Components;

/// <summary>
/// G7 · exemplo 1 — ciclo de dia acelerado. Fica no objeto do Sol (Luz
/// direcional + Hora do dia). Velocidade 720 = um dia inteiro em 2 minutos.
/// A sonda de reflexo da cena (Tempo real, Via script) é recapturada a cada
/// 15 min de jogo com ReflectionProbe.RenderProbe(), como no ciclo de dia da
/// Unity: sem isso o chão refletiria o céu da tarde a noite inteira.
/// </summary>
[ComponentId("g7.dia_acelerado")]
public sealed class DiaAcelerado : Behavior
{
    [PropertyId("velocidade")] public float Velocidade = 720;
    [PropertyId("hora_inicial")] public float HoraInicial = 16;
    [PropertyId("sonda")] public ObjectReference Sonda;

    private TimeOfDay tempo;
    private int quartoDeHora = -1;

    public override void Start()
    {
        tempo = Object.GetComponent<TimeOfDay>()!.Value;
        tempo.Hour = HoraInicial;          // TimeOfDay.Current.Hour = 16
        tempo.Speed = Velocidade;          // 1 min real = 12 h de jogo
        tempo.HourChanged(this, hora => Scene.Log(ObjectId, $"Hora do dia: {hora:00}:00"));
        tempo.Sunset(this, () => Scene.Log(ObjectId, "Pôr do sol"));
        tempo.Sunrise(this, () => Scene.Log(ObjectId, "Nascer do sol"));
    }

    public override void Update(float deltaTime)
    {
        var quarto = (int)(tempo.Hour * 4);
        if (quarto == quartoDeHora) return;
        quartoDeHora = quarto;
        if (Resolve(Sonda)?.GetComponent<ReflectionProbe>() is { } sonda) sonda.RenderProbe();
    }
}
