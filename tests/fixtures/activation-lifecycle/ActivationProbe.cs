using System;
using Astra;

// Cena de aceite no aparelho: Driver na raiz; Parent inativo; Child ativo
// dentro de Parent. Os dois scripts são compilados pelo editor do aparelho.
[ComponentId("acceptance.activation.counter")]
public sealed class ActivationCounter : Behavior
{
    [HideInInspector] public int Awakes, Starts, Enables, Disables, Updates;
    public override void Awake() { ++Awakes; Scene.Log(ObjectId, "O1A Awake"); }
    public override void Start() { ++Starts; Scene.Log(ObjectId, "O1A Start"); }
    public override void Enable() { ++Enables; Scene.Log(ObjectId, "O1A Enable"); }
    public override void Disable() { ++Disables; Scene.Log(ObjectId, "O1A Disable"); }
    public override void Update(float deltaTime) => ++Updates;
}

[ComponentId("acceptance.activation.driver")]
public sealed class ActivationDriver : Behavior
{
    private GameObject _parent = null!, _child = null!;
    private ActivationCounter _counter = null!;
    private float _time;
    private int _phase, _pausedUpdates;

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException("O1A FAIL: " + message);
    }

    public override void Start()
    {
        _parent = Object.Parent!.Find("Parent")!;
        _child = _parent.Find("Child")!;
        _counter = FindBehavior<ActivationCounter>(_child)!;
        Require(_child.ActiveSelf && !_child.ActiveInHierarchy && !_parent.ActiveSelf, "estado inicial");
        Require(_counter is not null && _counter.Awakes == 0, "Awake deve aguardar ativação");
        Scene.Log(ObjectId, "O1A local=True herdado=False; Awake adiado");
    }

    public override void Update(float deltaTime)
    {
        _time += deltaTime;
        if (_time < _phase + 1) return;
        switch (++_phase)
        {
            case 1:
                _parent.SetActive(true);
                Require(_child.ActiveSelf && _child.ActiveInHierarchy, "ativação do pai");
                break;
            case 2:
                _pausedUpdates = _counter.Updates;
                _parent.SetActive(false);
                Require(_child.ActiveSelf && !_child.ActiveInHierarchy, "desativação herdada");
                break;
            case 3:
                Require(_counter.Updates == _pausedUpdates && _counter.Disables == 1, "nenhum Update inativo");
                _parent.SetActive(true);
                break;
            case 4:
                _child.SetActive(false);
                break;
            case 5:
                _parent.SetActive(false); _parent.SetActive(true);
                Require(!_child.ActiveSelf && !_child.ActiveInHierarchy, "pai não reativa filho desligado");
                break;
            case 6:
                _child.SetActive(true);
                break;
            case 7:
                Require(ReferenceEquals(_counter, FindBehavior<ActivationCounter>(_child)), "instância preservada");
                Require(_counter.Awakes == 1 && _counter.Starts == 1 && _counter.Enables == 3 &&
                        _counter.Disables == 2 && _counter.Updates > 0, "contagem de lifecycle");
                Scene.Log(ObjectId, "O1A PASS: Awake=1 Start=1 Enable=3 Disable=2; mesma instancia");
                Enabled = false;
                break;
        }
    }
}
