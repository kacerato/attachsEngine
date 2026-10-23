using Astra;
using Astra.Components;

/// <summary>
/// G7 · exemplo 4 — acender os postes ao anoitecer. Fica no Sol. No pôr do
/// sol liga a Luz de cada poste (Light.enabled) e acende a lâmpada
/// (_EmissionColor); no nascer apaga. O dia corre a 720× para a cena mostrar
/// a virada em poucos segundos.
/// </summary>
[ComponentId("g7.postes_ao_anoitecer")]
public sealed class PostesAoAnoitecer : Behavior
{
    [PropertyId("postes")] public ObjectReference Postes;
    [PropertyId("velocidade")] public float Velocidade = 720;
    [PropertyId("hora_inicial")] public float HoraInicial = 17.2f;

    public override void Start()
    {
        var tempo = Object.GetComponent<TimeOfDay>()!.Value;
        tempo.Hour = HoraInicial;
        tempo.Speed = Velocidade;
        tempo.Sunset(this, () => Acender(true));
        tempo.Sunrise(this, () => Acender(false));
        Acender(false);
    }

    private void Acender(bool aceso)
    {
        foreach (var poste in Resolve(Postes)!.Children())
        foreach (var parte in poste.Children())
        {
            if (parte.GetComponent<Light>() is { } luz) luz.Enabled = aceso;
            if (parte.GetComponent<MeshRenderer>() is { } lampada)
                lampada.Material.SetColor("_EmissionColor", aceso ? new Color(4f, 2.6f, 1.2f) : new Color(0, 0, 0));
        }
        Scene.Log(ObjectId, aceso ? "Postes acesos" : "Postes apagados");
    }
}
