using Astra;
using System;
using System.Numerics;

[ComponentId("project.MetroPlayer")]
public sealed class MetroPlayer : Behavior
{
    [PropertyId("tempoInicial")] public float TempoInicial = 300;
    [PropertyId("alcanceInteracao")] public float AlcanceInteracao = 3.7f;
    private GameObject? _camera, _world, _emergency, _restored, _gate, _flashlight;
    private float _time, _gateHeight, _flicker;
    private int _fuses;
    private bool _powered, _finished, _flashlightOn = true;

    public override void Start()
    {
        _world = Object.Parent;
        _camera = Object.Find("Câmera dos olhos");
        _emergency = _world?.Find("Luzes de emergência");
        _restored = _world?.Find("Luzes restauradas");
        _gate = _world?.Find("Barreira de segurança");
        _flashlight = _world?.Find("Lanterna real");
        _world?.Find("Indicador verde")?.SetActive(false);
        _restored?.SetActive(false);
        _time = TempoInicial;
        _gateHeight = 2.45f;
        Scene.Log(ObjectId, "LINHA FANTASMA · 3 fusíveis · energia · catraca · trem | 300 s");
    }

    public override void Update(float dt)
    {
        if (_finished) return;
        _time -= dt;
        if (_time <= 0)
        {
            _finished = true;
            Scene.Log(ObjectId, "O ÚLTIMO TREM PARTIU · reinicie Play");
            return;
        }
        if (!_powered && _emergency is { IsAlive: true })
        {
            _flicker += dt;
            if (_flicker > .18f)
            {
                _flicker = 0;
                _emergency.SetActive((int)(_time * 13) % 9 != 0);
            }
        }
        if (_powered && _gate is { IsAlive: true } && _gateHeight < 5.2f)
        {
            _gateHeight = MathF.Min(5.2f, _gateHeight + dt * 2.2f);
            Scene.MoveKinematic(_gate.ObjectId, new Vector3(0, _gateHeight, 16.2f), Quaternion.Identity);
        }
        if (_powered && Object.Position.Z > 17.1f)
        {
            _finished = true;
            Scene.Log(ObjectId, "EXTRAÇÃO CONCLUÍDA · tempo restante " + MathF.Ceiling(_time) + " s");
            return;
        }
        if (!Input.JustPressed("Interagir") || _camera is not { IsAlive: true }) return;
        var pose = _camera.WorldTransform;
        var forward = Vector3.Normalize(Vector3.Transform(Vector3.UnitZ, pose.Rotation));
        var hit = Physics.RayCast(pose.Position, forward * AlcanceInteracao, QueryFilter.Default.Ignoring(Object));
        if (hit is null)
        {
            _flashlightOn = !_flashlightOn;
            _flashlight?.SetActive(_flashlightOn);
            Scene.Log(ObjectId, _flashlightOn ? "LANTERNA LIGADA" : "LANTERNA DESLIGADA");
            return;
        }
        var target = hit.Value.Object;
        if (target.Name.StartsWith("Fusível ", StringComparison.Ordinal))
        {
            target.Destroy();
            _fuses++;
            Scene.Log(ObjectId, "FUSÍVEIS " + _fuses + "/3 · tempo " + MathF.Ceiling(_time) + " s");
        }
        else if (target.Name == "Painel de energia")
        {
            if (_fuses < 3)
                Scene.Log(ObjectId, "PAINEL BLOQUEADO · faltam " + (3 - _fuses) + " fusíveis");
            else if (!_powered)
            {
                _powered = true;
                _emergency?.SetActive(false);
                _restored?.SetActive(true);
                _world?.Find("Indicador vermelho")?.SetActive(false);
                _world?.Find("Indicador verde")?.SetActive(true);
                Scene.Log(ObjectId, "ENERGIA RESTAURADA · catraca abrindo · siga até o trem");
            }
        }
        else if (target.Name.StartsWith("Caixa móvel ", StringComparison.Ordinal))
        {
            Scene.AddImpulse(target.ObjectId, forward * 12 + Vector3.UnitY * 1.5f);
            Scene.Log(ObjectId, "OBSTÁCULO DESLOCADO · tempo " + MathF.Ceiling(_time) + " s");
        }
    }
}
