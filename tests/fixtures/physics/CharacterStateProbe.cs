using System;
using Astra;
[ComponentId("acceptance.character.state")]
public sealed class CharacterStateProbe : Behavior {
 private int phase;private double deadline;
 public override void Start(){var state=Object.ReadCharacterState();if(state.HasMeasuredStep)throw new Exception("Initial sample invented");deadline=Time.TimeSinceStart+8;Scene.Log(ObjectId,"CHARACTER STATE READY");}
 public override void Update(float delta){
  var state=Object.ReadCharacterState();if(phase==3)return;
  if(Time.TimeSinceStart>deadline)throw new Exception("Character state acceptance timed out");
  if(phase==0&&state.IsGrounded&&state.HasMeasuredStep){if(state.GroundNormal.Y<.9f||!Object.TryJumpCharacter())throw new Exception("Support/jump mismatch");phase=1;}
  else if(phase==1&&!state.IsGrounded&&state.Velocity.Y>0)phase=2;
  else if(phase==2&&state.IsGrounded){phase=3;Scene.Log(ObjectId,"CHARACTER STATE PASS: initial, support normal, measured upward movement and landing");}
 }
}
