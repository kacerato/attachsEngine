using Astra;
using Astra.Components;

/// <summary>
/// G7 · exemplo 5 — assar uma sonda de reflexo por script. Fica na Sonda de
/// reflexo (Tempo real, Via script). A cada intervalo troca a cor do painel ao
/// lado da esfera cromada e chama ReflectionProbe.RenderProbe(): o reflexo só
/// muda quando o script pede, como na Unity.
/// </summary>
[ComponentId("g7.sonda_por_script")]
public sealed class SondaPorScript : Behavior
{
    [PropertyId("painel")] public ObjectReference Painel;
    [PropertyId("intervalo")] public float Intervalo = 2;

    private static readonly Color[] Cores = [new(1, .1f, .05f), new(.05f, .8f, .2f), new(.1f, .3f, 1)];
    private float relogio;
    private int cor;

    public override void Start()
    {
        var sonda = Object.GetComponent<ReflectionProbe>()!.Value;
        sonda.Mode = ReflectionProbeMode.Realtime;
        sonda.RefreshModeUnity = ReflectionProbeRefreshMode.ViaScripting;
        Pintar();
    }

    public override void Update(float deltaTime)
    {
        relogio += deltaTime;
        if (relogio < Intervalo) return;
        relogio = 0;
        cor = (cor + 1) % Cores.Length;
        Pintar();
    }

    private void Pintar()
    {
        var painel = Resolve(Painel)!.GetComponent<MeshRenderer>()!.Value;
        painel.Material.SetColor("_BaseColor", Cores[cor]);
        painel.Material.SetColor("_EmissionColor", Cores[cor] * 3);
        Object.GetComponent<ReflectionProbe>()!.Value.RenderProbe();
        Scene.Log(ObjectId, "Sonda recapturada");
    }
}
