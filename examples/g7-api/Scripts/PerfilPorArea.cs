using Astra;
using Astra.Rendering;

/// <summary>
/// G7 · exemplo 2 — trocar o perfil do Volume ao entrar numa área. Fica no
/// sensor da área (Corpo físico sensor + Colisor). Quem entra troca o
/// Volume.sharedProfile do Volume global; quem sai devolve o perfil anterior.
/// </summary>
[ComponentId("g7.perfil_por_area")]
public sealed class PerfilPorArea : Behavior
{
    [PropertyId("volume")] public ObjectReference VolumeGlobal;
    [PropertyId("perfil_da_area")] public AssetReference PerfilDaArea;

    private Volume? volume;
    private AssetGuid perfilDeFora;

    public override void Start()
    {
        volume = Volume.Get(Resolve(VolumeGlobal)!);
        perfilDeFora = volume!.SharedProfile;
    }

    public override void TriggerEnter(ObjectReference other)
    {
        volume!.SharedProfile = AssetGuid.Parse(PerfilDaArea.AssetId);
        Scene.Log(ObjectId, "Perfil da área ativo");
    }

    public override void TriggerExit(ObjectReference other) => volume!.SharedProfile = perfilDeFora;
}
