using Astra;
using System;
using System.Numerics;

// Executable acceptance for the write-query-owners native project fixture.
// Uses the public SDK and the real Play world, including typed property reads.
[ComponentId("example.physics.compound-query-probe")]
public sealed class CompoundQueryProbe : Behavior
{
    private void Verify(RayHit hit, string name)
    {
        if (hit.BodyObject.ObjectId != ObjectId || hit.ColliderObject is not { IsAlive: true } owner ||
            owner.Name != name || hit.ColliderInstance != 1 || hit.Collider is not { } collider ||
            !collider.IsAlive || collider.Object.ObjectId != owner.ObjectId || !collider.Component.Enabled)
            throw new InvalidOperationException("Compound query identity mismatch: " + name);
        Scene.Log(ObjectId, $"QUERY_IDENTITY {name} body={hit.BodyObject.ObjectId} colliderObject={owner.ObjectId} local={hit.ColliderInstance} generation={owner.Generation}");
    }
    public override void Start()
    {
        Verify(Physics.RayCast(new(-1.5f,3,0),new(0,-6,0)) ?? throw new InvalidOperationException("Left ray missed"), "LeftBox");
        Verify(Physics.RayCast(new(1.5f,3,0),new(0,-6,0)) ?? throw new InvalidOperationException("Right ray missed"), "RightCylinder");
        var all=Physics.RayCastAll(new(-4,0,0),new(8,0,0),out var truncated);
        if (truncated || all.Count!=2) throw new InvalidOperationException("All ray did not return both parts");
        Verify(all[0],"LeftBox"); Verify(all[1],"RightCylinder");
        Verify(Physics.ShapeCast(ShapeQuery.Sphere(.2f),new(1.5f,3,0),new(0,-6,0)) ?? throw new InvalidOperationException("Sweep missed"),"RightCylinder");
        var overlaps=Physics.Overlap(ShapeQuery.Box(new(3,2,2)),Vector3.Zero,out truncated);
        if (truncated || overlaps.Count!=2 || overlaps[0].ColliderObject!.ObjectId==overlaps[1].ColliderObject!.ObjectId)
            throw new InvalidOperationException("Overlap lost child identity");
        foreach(var hit in overlaps) Verify(hit,hit.ColliderObject!.Name);
        Scene.Log(ObjectId,"QUERY_IDENTITY PASS rays=2 all=2 sweep=1 overlap=2 typedColliders=7");
    }
}
