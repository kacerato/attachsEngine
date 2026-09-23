using Astra;
using System;
using System.Numerics;

[ComponentId("project.RescuePlayer")]
public sealed class RescuePlayer : Behavior
{
    [PropertyId("oxigenioInicial")] public float OxigenioInicial = 240;
    [PropertyId("alcanceInteracao")] public float AlcanceInteracao = 4.1f;
    [PropertyId("distanciaSegurar")] public float DistanciaSegurar = 2.2f;
    private GameObject? _camera, _world, _held;
    private float _oxygen;
    private int _pumps, _survivors;
    private bool _finished;

    public override void Start()
    {
        _world = Object.Parent;
        _camera = Object.Find("Câmera dos olhos");
        _oxygen = OxigenioInicial;
        for (var index = 1; index <= 2; index++)
            _world?.Find("Bomba " + index)?.Find("Indicador verde")?.SetActive(false);
        Scene.Log(ObjectId, "RESGATE · 2 bombas · 3 sobreviventes · elevador | O2 240 s");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _oxygen -= dt * (_pumps == 2 ? .55f : 1);
        if (_oxygen <= 0)
        {
            _finished = true;
            Scene.Log(ObjectId, "SEM OXIGÊNIO · reinicie Play");
            return;
        }
        if (_pumps == 2 && _survivors == 3 && Object.Position.Z > 26)
        {
            _finished = true;
            Scene.Log(ObjectId, "RESGATE CONCLUÍDO · equipe evacuada · O2 " + MathF.Ceiling(_oxygen) + " s");
            return;
        }
        if (!Input.JustPressed("Interagir") || _camera is not { IsAlive: true }) return;
        var target = AimTarget(_held);
        if (_held is { IsAlive: true } carried)
        {
            var survivor = FindBehavior<SurvivorAgent>(target);
            if (carried.Name.StartsWith("Kit médico real ", StringComparison.Ordinal) && survivor?.Heal() == true)
            {
                carried.Destroy();
                _held = null;
                _survivors++;
                Scene.Log(ObjectId, "SOBREVIVENTE ESTABILIZADO " + _survivors + "/3 · ele seguirá a equipe");
            }
            else
            {
                var pose = _camera.WorldTransform;
                var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
                Scene.SetBodyVelocity(carried.ObjectId, forward * 6 + Vector3.UnitY * 1.4f);
                _held = null;
                Scene.Log(ObjectId, "OBJETO ARREMESSADO · use kits nos sobreviventes");
            }
            return;
        }
        if (target is null) { Scene.Log(ObjectId, "Mire em bomba, kit, escombro ou sobrevivente"); return; }
        if (target.Name.StartsWith("Bomba ", StringComparison.Ordinal))
        {
            var green = target.Find("Indicador verde");
            if (green is { ActiveInHierarchy: true }) return;
            target.Find("Indicador vermelho")?.SetActive(false);
            green?.SetActive(true);
            _pumps++;
            Scene.Log(ObjectId, "BOMBAS " + _pumps + "/2 · drenagem ativada");
        }
        else if (target.Name.StartsWith("Kit médico real ", StringComparison.Ordinal) ||
                 target.Name.StartsWith("Bloco móvel ", StringComparison.Ordinal))
        {
            _held = target;
            Scene.Log(ObjectId, "SEGURANDO " + target.Name + " · interaja para usar ou arremessar");
        }
        else if (FindBehavior<SurvivorAgent>(target) is { IsHealed: false })
        {
            Scene.Log(ObjectId, "O sobrevivente precisa de um kit médico");
        }
    }

    public override void FixedUpdate(float dt)
    {
        if (_held is not { IsAlive: true } carried || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var delta = pose.Position + forward * DistanciaSegurar - carried.WorldTransform.Position;
        if (delta.LengthSquared() > 42) { _held = null; return; }
        var velocity = delta * 12.5f - Scene.GetBodyVelocity(carried.ObjectId) * 2.6f;
        if (velocity.LengthSquared() > 144) velocity = Vector3.Normalize(velocity) * 12;
        if (!Scene.SetBodyVelocity(carried.ObjectId, velocity)) _held = null;
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
}
