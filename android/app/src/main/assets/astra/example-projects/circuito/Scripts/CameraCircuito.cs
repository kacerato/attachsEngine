using Astra;
using System.Numerics;

[ComponentId("project.CameraCircuito")]
public sealed class CameraCircuito : Behavior
{
    private GameObject? _piloto;
    public override void Start() => _piloto = Object.Parent?.Find("Corredor");
    public override void Update(float dt)
    {
        if (_piloto is not { IsAlive: true }) return;
        var pos = _piloto.Position;
        Object.Position = new Vector3(pos.X, 13, pos.Z - 12);
    }
}
