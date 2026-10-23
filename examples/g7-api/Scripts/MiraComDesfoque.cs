using Astra;
using Astra.Components;
using Astra.Rendering;

/// <summary>
/// G7 · exemplo 3 — ligar o desfoque de profundidade ao mirar. Fica na
/// câmera. Segurar o botão "Mirar" fecha o campo de visão e liga o Depth of
/// Field do Volume de mira, focado no que está no centro da tela.
/// </summary>
[ComponentId("g7.mira_com_desfoque")]
public sealed class MiraComDesfoque : Behavior
{
    [PropertyId("volume_de_mira")] public ObjectReference VolumeDeMira;
    [PropertyId("campo_normal")] public float CampoNormal = 60;
    [PropertyId("campo_mirando")] public float CampoMirando = 30;
    [PropertyId("velocidade")] public float Velocidade = 6;

    private Volume? volume;
    private DepthOfField? desfoque;

    public override void Start()
    {
        volume = Volume.Get(Resolve(VolumeDeMira)!);
        // Volume.profile: a cópia própria, como na Unity.
        var perfil = volume!.Profile;
        desfoque = perfil.TryGet<DepthOfField>(out var existente) ? existente : perfil.Add<DepthOfField>(overrides: true);
        desfoque!.Mode.Override(DepthOfFieldMode.Bokeh);
        volume.Weight = 0;
    }

    public override void Update(float deltaTime)
    {
        var mirando = Input.Pressed("Mirar");
        var camera = Object.GetComponent<Camera>()!.Value;
        var alvo = mirando ? 1f : 0f;
        volume!.Weight = Mathf.MoveTowards(volume.Weight, alvo, Velocidade * deltaTime);
        camera.FieldOfView = Mathf.Lerp(CampoNormal, CampoMirando, volume.Weight);
        // Foco no que está à frente da câmera (raio de 50 m).
        var pose = Object.WorldTransform;
        var frente = System.Numerics.Vector3.Transform(System.Numerics.Vector3.UnitZ, pose.Rotation);
        if (mirando && Physics.RayCast(pose.Position, frente * 50) is { } acerto)
            desfoque!.FocusDistance.Override(acerto.Distance);
    }
}
