using Astra;
using Astra.Components;

/// <summary>
/// G7 · exemplo 6 — trocar o material de um objeto. Fica no objeto com Mesh
/// Renderer. A cada intervalo alterna Renderer.sharedMaterial entre um
/// material Lit (ouro) e um Unlit (neon); no Lit ainda ajusta a suavidade pelo
/// nome da propriedade da URP (_Smoothness), como Renderer.material.SetFloat.
/// </summary>
[ComponentId("g7.troca_de_material")]
public sealed class TrocaDeMaterial : Behavior
{
    [PropertyId("material_a")] public AssetReference MaterialA;
    [PropertyId("material_b")] public AssetReference MaterialB;
    [PropertyId("intervalo")] public float Intervalo = 2;

    private float relogio;
    private bool usandoB;

    public override void Update(float deltaTime)
    {
        relogio += deltaTime;
        if (relogio < Intervalo) return;
        relogio = 0;
        usandoB = !usandoB;
        var malha = Object.GetComponent<MeshRenderer>()!.Value;
        malha.SharedMaterial = AssetGuid.Parse((usandoB ? MaterialB : MaterialA).AssetId);
        if (!usandoB) malha.Material.SetFloat("_Smoothness", .9f);
        Scene.Log(ObjectId, usandoB ? "Material B (Unlit)" : "Material A (Lit)");
    }
}
