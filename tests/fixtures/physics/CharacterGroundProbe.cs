using System;
using System.Numerics;
using Astra;
using Astra.Components;
[ComponentId("acceptance.character.ground")]
public sealed class CharacterGroundProbe : Behavior
{
    private Character character;private double start;private bool climbing,done;
    public override void Start()
    {
        foreach(var c in Object.Components())if(c.TypeId==Character.TypeId)character=new(c);
        if(character.InstanceId==0)throw new Exception("Character missing");
        climbing=character.StepHeight>0;character.Gravity=24;character.FloorSnapLength=.5f;character.Speed=2;
        start=Time.TimeSinceStart;Scene.Log(ObjectId,"CHARACTER GROUND READY: step="+character.StepHeight);
    }
    public override void Update(float delta)
    {
        if(done)return;
        var elapsed=Time.TimeSinceStart-start;
        if(elapsed>2&&elapsed<5)Object.MoveCharacter(new Vector2(1,0),0);
    }
    public override void LateUpdate(float delta)
    {
        if(done)return;
        if(Time.TimeSinceStart-start<=5)return;
        var x=Transform.Position.X;
        if(climbing?x<=4.5f:x>=1.5f)throw new Exception("Stair traversal mismatch: step="+character.StepHeight+" x="+x);
        done=true;Scene.Log(ObjectId,"CHARACTER GROUND PASS: "+(climbing?"climbed":"blocked")+" x="+x);
    }
}
