using Astra;

[ComponentId("acceptance.services.fase2")]
public sealed class Fase2Probe : Behavior
{
    public override void Start() =>
        Scene.Log(ObjectId, "SERVICES SCENE " + (Scenes.Active == "Fase2" ? "PASS" : "FAIL") + " active=" + Scenes.Active);
}
