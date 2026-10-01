using System.Numerics;
namespace Astra;
public enum CharacterGroundState : uint {OnGround,OnSteepGround,NotSupported,InAir}
/// <summary>Velocity is displacement resolved during the last fixed step; MotorVelocity is the Jolt motor input. Position is the capsule foot in world space. Runtime only.</summary>
public readonly record struct CharacterRuntimeState(CharacterGroundState GroundState,bool HasMeasuredStep,Vector3 Position,Vector3 Velocity,Vector3 MotorVelocity,Vector3 GroundVelocity,Vector3 GroundNormal) {public bool IsGrounded=>GroundState==CharacterGroundState.OnGround;}
