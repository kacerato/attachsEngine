using Astra;
using System;
using System.Numerics;

[ComponentId("project.QuarantinePlayer")]
public sealed class QuarantinePlayer : Behavior
{
    [PropertyId("oxigenioInicial")] public float OxigenioInicial = 180;
    [PropertyId("alcanceInteracao")] public float AlcanceInteracao = 3.8f;
    private GameObject? _camera, _world, _emergency, _work, _door;
    private float _oxygen, _doorHeight;
    private int _fuses;
    private bool _power, _finished;

    public override void Start()
    {
        _world = Object.Parent;
        _camera = Object.Find("Câmera dos olhos");
        _emergency = _world?.Find("Luzes de emergência");
        _work = _world?.Find("Iluminação restaurada");
        _door = _world?.Find("Porta blindada");
        _work?.SetActive(false);
        _oxygen = OxigenioInicial;
        _doorHeight = 2;
        Scene.Log(ObjectId, "QUARENTENA · 3 fusíveis · gerador · saída | O2 180 s");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _oxygen -= dt;
        if (_oxygen <= 0)
        {
            _finished = true;
            Scene.Log(ObjectId, "SEM OXIGÊNIO · reinicie Play");
            return;
        }
        if (_power && _door is { IsAlive: true } && _doorHeight < 6.3f)
        {
            _doorHeight = MathF.Min(6.3f, _doorHeight + dt * 2.4f);
            Scene.MoveKinematic(_door.ObjectId, new Vector3(0, _doorHeight, 21), Quaternion.Identity);
        }
        if (_power && Object.Position.Z > 25)
        {
            _finished = true;
            Scene.Log(ObjectId, "EXTRAÇÃO CONCLUÍDA · oxigênio restante " + MathF.Ceiling(_oxygen) + " s");
            return;
        }
        if (!Input.JustPressed("Interagir") || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hit = Physics.RayCast(pose.Position, forward * AlcanceInteracao, QueryFilter.Default.Ignoring(Object));
        if (hit is null) { Scene.Log(ObjectId, "Interação: mire em um fusível ou no gerador"); return; }
        var target = hit.Value.Object;
        if (target.Name.StartsWith("Fusível ", StringComparison.Ordinal))
        {
            target.Destroy();
            _fuses++;
            Scene.Log(ObjectId, "FUSÍVEIS " + _fuses + "/3 · O2 " + MathF.Ceiling(_oxygen) + " s");
        }
        else if (target.Name == "Painel do gerador")
        {
            if (_fuses < 3) Scene.Log(ObjectId, "Gerador exige 3 fusíveis; faltam " + (3 - _fuses));
            else if (!_power)
            {
                _power = true;
                _emergency?.SetActive(false);
                _work?.SetActive(true);
                Scene.Log(ObjectId, "ENERGIA RESTAURADA · porta abrindo · alcance a saída");
            }
        }
    }
}
