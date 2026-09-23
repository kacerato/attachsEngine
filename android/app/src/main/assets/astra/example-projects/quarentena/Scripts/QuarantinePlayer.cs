using Astra;
using System;
using System.Numerics;

[ComponentId("project.QuarantinePlayer")]
public sealed class QuarantinePlayer : Behavior
{
    [PropertyId("oxigenioInicial")] public float OxigenioInicial = 180;
    [PropertyId("alcanceInteracao")] public float AlcanceInteracao = 3.8f;
    [PropertyId("distanciaSegurar")] public float DistanciaSegurar = 2.15f;
    private GameObject? _camera, _world, _emergency, _work, _door, _drone, _held;
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
        _drone = _world?.Find("MIRA · drone de manutenção");
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
        var target = AimTarget(_held);
        if (_held is { IsAlive: true } carried)
        {
            if (target?.Name == "Painel do gerador" && carried.Name.StartsWith("Fusível ", StringComparison.Ordinal))
            {
                carried.Destroy();
                _held = null;
                _fuses++;
                Scene.Log(ObjectId, "FUSÍVEL INSERIDO " + _fuses + "/3 · O2 " + MathF.Ceiling(_oxygen) + " s");
            }
            else Drop("OBJETO SOLTO");
            return;
        }
        if (target is null) { Scene.Log(ObjectId, "Mire em um fusível ou no painel"); return; }
        if (target.Name.StartsWith("Fusível ", StringComparison.Ordinal))
        {
            _held = target;
            Scene.Log(ObjectId, "SEGURANDO " + target.Name + " · leve ao painel e interaja");
        }
        else if (target.Name == "Painel do gerador")
        {
            if (_fuses < 3) Scene.Log(ObjectId, "Gerador exige 3 fusíveis; faltam " + (3 - _fuses));
            else if (!_power)
            {
                _power = true;
                _emergency?.SetActive(false);
                _work?.SetActive(true);
                FindBehavior<MaintenanceDrone>(_drone)?.PowerRestored();
                Scene.Log(ObjectId, "ENERGIA RESTAURADA · porta abrindo · alcance a saída");
            }
        }
    }

    public override void FixedUpdate(float dt)
    {
        if (_held is not { IsAlive: true } carried || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var delta = pose.Position + forward * DistanciaSegurar - carried.WorldTransform.Position;
        if (delta.LengthSquared() > 42) { Drop("OBJETO PERDIDO"); return; }
        var velocity = delta * 13 - Scene.GetBodyVelocity(carried.ObjectId) * 2.7f;
        if (velocity.LengthSquared() > 144) velocity = Vector3.Normalize(velocity) * 12;
        if (!Scene.SetBodyVelocity(carried.ObjectId, velocity)) Drop("NÃO FOI POSSÍVEL SEGURAR");
    }

    private GameObject? AimTarget(GameObject? ignored)
    {
        if (_camera is not { IsAlive: true }) return null;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hits = Physics.RayCastAll(pose.Position, forward * AlcanceInteracao, out _,
                                      QueryFilter.Default.Ignoring(Object), 12);
        foreach (var hit in hits)
            if (ignored is null || hit.Object.ObjectId != ignored.ObjectId) return hit.Object;
        return null;
    }

    private void Drop(string message)
    {
        if (_held is { IsAlive: true }) Scene.SetBodyVelocity(_held.ObjectId, Vector3.Zero);
        _held = null;
        Scene.Log(ObjectId, message);
    }
}
