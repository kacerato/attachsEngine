using System;
using System.Numerics;
using Astra;
using Astra.Components;

[ComponentId("acceptance.primitives")]
public sealed class PrimitiveProbe : Behavior
{
    private readonly GameObject[] shapes = new GameObject[6];
    private float elapsed;
    private bool verified;
    public override void Start()
    {
        var root = Object.Parent!;
        for (var i = 0; i < shapes.Length; ++i)
        {
            var shape = root.CreatePrimitive((PrimitiveType)i);
            shapes[i] = shape;
            shape.Name = "API " + (PrimitiveType)i;
            shape.LocalTransform = new(new Vector3((i - 2.5f) * 2.5f, 1, 0),
                i == 5 ? Quaternion.CreateFromAxisAngle(Vector3.UnitY, MathF.PI) : Quaternion.Identity, Vector3.One);
            var material = shape.GetComponent<MeshRenderer>()!.Value.Component.Material();
            material.BaseColor = new Vector3(i % 3 == 0 ? .8f : .18f, i % 3 == 1 ? .8f : .18f, i % 3 == 2 ? .8f : .18f);
            material.Roughness = .35f;
            if (shape.GetComponent<PhysicsBody>() is null || shape.GetComponent<Collider>() is null)
                throw new Exception("PRIMITIVE FAIL: missing collision recipe");
        }
        // Plane stays at the back so its 10 m footprint does not hide the other shapes.
        shapes[4].LocalTransform = new(new Vector3(0, -1, -8), Quaternion.Identity, Vector3.One);
        try { root.CreatePrimitive((PrimitiveType)99); throw new Exception("invalid primitive accepted"); }
        catch (ArgumentOutOfRangeException) { }
        Scene.Log(ObjectId, "PRIMITIVE READY: six meshes, PBR materials and collision recipes");
    }
    public override void Update(float dt)
    {
        elapsed += dt;
        if (verified || elapsed < .4f) return;
        for (var i = 0; i < shapes.Length; ++i)
        {
            var position = shapes[i].WorldTransform.Position;
            var direction = i == 5 ? Vector3.UnitZ : -Vector3.UnitY;
            var hit = Physics.RayCast(position - direction * 3, direction * 6);
            if (hit is null || hit.Value.Object != shapes[i])
                throw new Exception("PRIMITIVE FAIL: real physics ray missed " + (PrimitiveType)i);
        }
        verified = true;
        Scene.Log(ObjectId, "PRIMITIVE PASS: all six rendered resources resolve and all six Jolt colliders answer rays");
    }
}
