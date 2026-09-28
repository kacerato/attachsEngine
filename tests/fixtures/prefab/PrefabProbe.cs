using System;
using System.Numerics;
using Astra;
using Astra.Components;

[ComponentId("acceptance.prefab.receiver")]
public sealed class PrefabReceiver : Behavior
{
    // The prefab authors 41; a different code default proves binding before Awake.
    [PropertyId("value")] public int Value = 7;
    public override void Awake()
    {
        if (Value != 41) throw new Exception("PREFAB FAIL: authored value was not bound before Awake");
        Scene.Log(ObjectId, "PREFAB AWAKE: authored value 41");
    }
}

[ComponentId("acceptance.prefab.driver")]
public sealed class PrefabProbe : Behavior
{
    [PropertyId("prefab")] public string Prefab = "";
    private GameObject? first, second;
    private float elapsed;
    private bool checkedPhysics, checkedDestruction;
    public override void Start()
    {
        var guid = AssetGuid.Parse(Prefab);
        first = Object.Parent!.InstantiatePrefab(guid);
        second = Object.Parent!.InstantiatePrefab(guid);
        first.Name = "Prefab API A"; second.Name = "Prefab API B";
        first.LocalTransform = new(new Vector3(-2, 1, 0), Quaternion.Identity, Vector3.One);
        second.LocalTransform = new(new Vector3(2, 1, 0), Quaternion.Identity, Vector3.One);
        if (first.GetBehavior<PrefabReceiver>() is null || second.GetBehavior<PrefabReceiver>() is null)
            throw new Exception("PREFAB FAIL: receiver not registered");
        first.GetBehavior<PrefabReceiver>()!.Value = 73;
        if (second.GetBehavior<PrefabReceiver>()!.Value != 41)
            throw new Exception("PREFAB FAIL: shared instance state");
        Scene.Log(ObjectId, "PREFAB READY: two independent hierarchies and scripts");
    }
    public override void Update(float dt)
    {
        elapsed += dt;
        if (!checkedPhysics && elapsed > .5f)
        {
            foreach (var copy in new[] { first!, second! })
            {
                var hit = Physics.RayCast(copy.WorldTransform.Position + new Vector3(0, 3, 0), new Vector3(0, -5, 0));
                if (copy.GetComponent<Collider>() is null || hit is null || hit.Value.Object.ObjectId != copy.ObjectId)
                    throw new Exception("PREFAB FAIL: instantiated physics did not reach Jolt");
            }
            first!.Destroy(); checkedPhysics = true;
        }
        if (checkedPhysics && !checkedDestruction && elapsed > 1)
        {
            if (first!.IsAlive || !second!.IsAlive || second.GetBehavior<PrefabReceiver>()!.Value != 41)
                throw new Exception("PREFAB FAIL: lifetime crossed instance boundary");
            checkedDestruction = true;
            Scene.Log(ObjectId, "PREFAB PASS: authored fields, isolated scripts, collision and destruction");
        }
    }
}
