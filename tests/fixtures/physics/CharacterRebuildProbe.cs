using System;
using System.Numerics;
using Astra;
using Astra.Components;
[ComponentId("acceptance.character.rebuild")]
public sealed class CharacterRebuildProbe : Behavior
{
    private PhysicsBody floor;private double start;private int phase;private Vector3 origin;
    public override void Start()
    {
        var support=Object.FindInWorld("Piso")??throw new Exception("Authored floor missing");
        floor=new(support.GetComponent(PhysicsBody.TypeId)??throw new Exception("Real body missing"));
        start=Time.TimeSinceStart;Scene.Log(ObjectId,"CHARACTER REBUILD READY: settle on real Jolt floor");
    }
    public override void Update(float delta)
    {
        if(phase>1||Time.TimeSinceStart-start<2)return;
        Object.MoveCharacter(new Vector2(1,0),0);
        if(phase==0){origin=Transform.Position;if(!Object.TryJumpCharacter())throw new Exception("Supported jump rejected");phase=1;}
    }
    public override void FixedUpdate(float delta)
    {
        if(phase!=1)return;
        floor.Friction=.7f;phase=2; // Native safe-point rebuild after this callback.
    }
    public override void LateUpdate(float delta)
    {
        if(phase!=2)return;var now=Transform.Position;
        if(now.X<=origin.X+.01f||now.Y<=origin.Y+.02f)throw new Exception("Rebuild erased accepted movement/jump");
        phase=3;Scene.Log(ObjectId,"CHARACTER REBUILD PASS: native contacts, move and queued jump survive body edit");
    }
}
