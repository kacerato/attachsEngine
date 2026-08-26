using System.Runtime.InteropServices;

namespace Aether.Physics;

/// <summary>
/// Enum blittable — mesmo layout de <c>AetherMotionType</c> em <c>native/physics/jolt_bridge.h</c>.
/// </summary>
public enum NativeMotionType : uint
{
    Static = 0,
    Kinematic = 1,
    Dynamic = 2,
}

/// <summary>Mesmo layout de <c>AetherShapeKind</c>.</summary>
public enum NativeShapeKind : uint
{
    Box = 0,
    Sphere = 1,
    /// <summary>Item 4.1.5 — forma padrão de character controller. Ver <see cref="NativeShapeDesc.CapsuleHalfHeight"/>.</summary>
    Capsule = 2,
}

/// <summary>
/// Mesmo layout de <c>AetherShapeDesc</c>: <c>boxHalfExtent</c>/<c>sphereRadius</c>/
/// <c>capsuleHalfHeight</c> ocupam campos separados (não uma union) porque o C ABI do lado
/// nativo também não usa union — mais simples e o desperdício de bytes não pesa (descritor é só
/// usado na criação do corpo/query, nunca no caminho quente de frame).
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct NativeShapeDesc
{
    public NativeShapeKind Kind;
    public float3 BoxHalfExtent;
    public float SphereRadius;       // usado quando Kind == Sphere ou Capsule (raio da cápsula)
    public float CapsuleHalfHeight;  // usado quando Kind == Capsule — altura do cilindro, sem as tampas
}

/// <summary>Mesmo layout de <c>AetherAllowedDOFs</c> (item 4.1.6, física 2D). AVISO DE
/// CONVENÇÃO: <see cref="All"/> == 0 aqui (não <c>0b111111</c> como <c>JPH::EAllowedDOFs::All</c>)
/// — um <see cref="NativeBodyDesc"/> <c>default</c>/zero-inicializado precisa continuar sendo
/// "corpo 3D normal, sem restrição", não "todos os eixos travados" (inválido no Jolt para
/// corpo Dynamic). A conversão para o Jolt do lado nativo inverte isso explicitamente — ver
/// comentário completo em jolt_bridge.h.</summary>
[Flags]
public enum NativeAllowedDOFs : uint
{
    All = 0,
    TranslationX = 1u << 0,
    TranslationY = 1u << 1,
    TranslationZ = 1u << 2,
    RotationX = 1u << 3,
    RotationY = 1u << 4,
    RotationZ = 1u << 5,
    /// <summary>Plano XY (mão-esquerda, Y-para-cima — ver CONVENCOES.md §5): trava
    /// profundidade (Z) e as rotações que tirariam o corpo do plano da tela, deixando livre
    /// translação em X/Y e giro em torno de Z. Caso de uso mais comum de física 2D
    /// (plataforma vista de lado/de frente).</summary>
    Plane2D = TranslationX | TranslationY | RotationZ,
}

/// <summary>Mesmo layout de <c>AetherBodyDesc</c>.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct NativeBodyDesc
{
    public NativeShapeDesc Shape;
    public float3 Position;
    public quaternion Rotation;
    public NativeMotionType MotionType;
    public float Friction;
    public float Restitution;
    public NativeAllowedDOFs AllowedDOFs;
}

/// <summary>Mesmo layout de <c>AetherJointKind</c> (item 4.1.3).</summary>
public enum NativeJointKind : uint
{
    Point = 0,
    Hinge = 1,
    Slider = 2,
    Distance = 3,
}

/// <summary>Referencial dos pontos/eixos de <see cref="NativeJointDescV2"/>.</summary>
public enum NativeJointSpace : uint
{
    World = 0,
    LocalToBody1 = 1,
    LocalToBody2 = 2,
}

/// <summary>Mesmo layout de <c>AetherMotorState</c>. Só Hinge e Slider usam motor de verdade
/// (Point/Distance não têm <c>JPH::MotorSettings</c> — ver comentário em jolt_bridge.h).</summary>
public enum NativeMotorState : uint
{
    Off = 0,
    Velocity = 1,
    Position = 2,
    PositionAndVelocity = 3,
}

/// <summary>Mesmo layout de <c>AetherJointMotorDesc</c>.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct NativeJointMotorDesc
{
    public NativeMotorState State;
    public float TargetVelocity;
    public float TargetPosition;
    public float MaxForceOrTorque;
    public float SpringFrequency;
    public float SpringDamping;
}

/// <summary>Mesmo layout de <c>AetherJointDesc</c>.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct NativeJointDesc
{
    public NativeJointKind Kind;
    public float3 Point1;
    public float3 Point2;
    public float3 Axis1;
    public float3 Axis2;
    public float LimitsMin;
    public float LimitsMax;
    public NativeJointMotorDesc Motor;
}

/// <summary>Mesmo layout de <c>AetherJointDescV2</c>. V1 permanece declarado acima para
/// compatibilidade de ABI e para testes de layout, mas código gerenciado novo usa somente V2.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct NativeJointDescV2
{
    public uint StructSize;
    public uint ApiVersion;
    public NativeJointKind Kind;
    public NativeJointSpace Space;
    public float3 Point1;
    public float3 Point2;
    public float3 Axis1;
    public float3 Axis2;
    public float LimitsMin;
    public float LimitsMax;
    public NativeJointMotorDesc Motor;
}

/// <summary>Mesmo layout de <c>AetherQueryLayerMask</c> (item 4.1.4) — combinável com OR bit a
/// bit, mesma disciplina de qualquer flags enum em .NET (<c>[Flags]</c>).</summary>
[Flags]
public enum NativeQueryLayerMask : uint
{
    None = 0,
    Static = 1u << 0,
    Dynamic = 1u << 1,
    All = Static | Dynamic,
}

/// <summary>Mesmo layout de <c>AetherShapeQueryHit</c>.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct NativeShapeQueryHit
{
    public uint Body;
    public float Fraction;
    public float3 ContactPointOnQuery;
    public float3 ContactPointOnHit;
    public float3 PenetrationAxis;
}

/// <summary>Mesmo layout de <c>AetherCharacterGroundState</c> (item 4.1.5).</summary>
public enum NativeCharacterGroundState : uint
{
    OnGround = 0,
    OnSteepGround = 1,
    NotSupported = 2,
    InAir = 3,
}

/// <summary>Mesmo layout de <c>AetherCharacterDesc</c>.</summary>
[StructLayout(LayoutKind.Sequential)]
public struct NativeCharacterDesc
{
    public float Radius;
    public float StandingHalfHeight;
    public float CrouchingHalfHeight;
    public float MaxSlopeAngle;
    public float Mass;
    public float MaxStrength;
}

internal enum NativePhysicsOverflowPolicy : uint
{
    BuildDefault = 0,
    Warning = 1,
    FailFast = 2,
}

[Flags]
internal enum NativePhysicsUpdateError : uint
{
    None = 0,
    ManifoldCacheFull = 1u << 0,
    BodyPairCacheFull = 1u << 1,
    ContactConstraintsFull = 1u << 2,
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativePhysicsWorldDescV2
{
    public uint StructSize;
    public uint ApiVersion;
    public float3 Gravity;
    public uint MaxBodies;
    public uint MaxBodyPairs;
    public uint MaxContactConstraints;
    public uint MaxBroadPhasePairs;
    public NativePhysicsOverflowPolicy OverflowPolicy;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativePhysicsStepStatsV2
{
    public uint StructSize;
    public uint ApiVersion;
    public ulong TotalSteps;
    public ulong OverflowSteps;
    public ulong ManifoldCacheFullCount;
    public ulong BodyPairCacheFullCount;
    public ulong ContactConstraintsFullCount;
    public NativePhysicsUpdateError LastErrorFlags;
    public uint Reserved;
}

/// <summary>
/// Bindings P/Invoke cruas sobre <c>native/physics/jolt_bridge.h</c> (itens 4.1.1 e 4.1.3, juntas
/// e motores). Espelha a
/// fronteira C ABI 1:1 — nenhuma lógica aqui, só a declaração de cada função exportada. Não use
/// esta classe diretamente fora de <see cref="Aether.Physics"/>; <see cref="PhysicsWorld"/> e
/// <see cref="PhysicsSyncSystem"/> são a API que o resto da engine deve chamar.
/// <para>
/// <c>[LibraryImport]</c> (source-generated, sem reflection) em vez de <c>[DllImport]</c>: é a
/// forma preferida desde .NET 7 e evita o custo de marshaling reflection-based no primeiro uso —
/// consistente com "zero dependência externa" e a meta de AOT/trimming já citada em
/// <c>ComponentMetadata.cs</c>. Nome da biblioteca nativa é "aether_physics" sem prefixo/extensão:
/// o runtime resolve para "aether_physics.dll" no Windows e "libaether_physics.so" no Linux —
/// mesma convenção de <c>OUTPUT_NAME</c>/<c>PREFIX</c> configurada em <c>native/CMakeLists.txt</c>
/// para o alvo <c>aether_physics_shared</c>.
/// </para>
/// <para>
/// Tipos blittable usados diretamente pela fronteira: <see cref="float3"/> e
/// <see cref="quaternion"/> têm o mesmo layout de <c>AetherVec3</c>/<c>AetherQuat</c> (12/16
/// bytes, campos float na mesma ordem) — reusados aqui em vez de duplicar um par de structs só
/// para P/Invoke (ver <c>docs/CONVENCOES.md</c> §2: "a fronteira só transporta tipos blittable").
/// </para>
/// </summary>
internal static partial class NativePhysics
{
    private const string LibraryName = "aether_physics";

    [LibraryImport(LibraryName)]
    internal static partial nint AetherPhysics_CreateWorld(float3 gravity, uint maxBodies);

    [LibraryImport(LibraryName)]
    internal static partial nint AetherPhysics_CreateWorldV2(in NativePhysicsWorldDescV2 desc);

    [LibraryImport(LibraryName)]
    internal static partial void AetherPhysics_DestroyWorld(nint world);

    [LibraryImport(LibraryName)]
    internal static partial uint AetherPhysics_CreateBody(nint world, in NativeBodyDesc desc);

    [LibraryImport(LibraryName)]
    internal static partial uint AetherPhysics_CreateBodyV2(nint world, in PhysicsBodyDescription desc);

    [LibraryImport(LibraryName)]
    internal static unsafe partial int AetherPhysics_CreateBodiesV2(nint world,
        PhysicsBodyDescription* descs, PhysicsBodyHandle* outHandles, int count);

    [LibraryImport(LibraryName)]
    internal static partial void AetherPhysics_DestroyBody(nint world, uint handle);

    [LibraryImport(LibraryName)]
    internal static unsafe partial void AetherPhysics_DestroyBodies(nint world,
        PhysicsBodyHandle* handles, int count);

    [LibraryImport(LibraryName)]
    internal static partial void AetherPhysics_Step(nint world, float deltaTime, int collisionSteps);

    [LibraryImport(LibraryName)]
    internal static partial NativePhysicsUpdateError AetherPhysics_StepV2(nint world, float deltaTime, int collisionSteps);

    [LibraryImport(LibraryName)]
    internal static unsafe partial int AetherPhysics_GetTriggerEvents(nint world,
        PhysicsTriggerEvent* outEvents, int maxResults);

    [LibraryImport(LibraryName)]
    internal static partial int AetherPhysics_GetStepStatsV2(nint world, ref NativePhysicsStepStatsV2 outStats);

    [LibraryImport(LibraryName)]
    internal static unsafe partial void AetherPhysics_GetTransform(nint world, uint handle, float3* outPosition, quaternion* outRotation);

    [LibraryImport(LibraryName)]
    internal static partial void AetherPhysics_SetLinearVelocity(nint world, uint handle, float3 velocity);

    [LibraryImport(LibraryName)]
    internal static partial float3 AetherPhysics_GetLinearVelocity(nint world, uint handle);

    [LibraryImport(LibraryName)]
    internal static partial int AetherPhysics_MoveKinematicV2(nint world, uint handle,
        float3 targetPosition, quaternion targetRotation, float deltaTime);

    [LibraryImport(LibraryName)]
    [return: MarshalAs(UnmanagedType.I4)]
    internal static partial int AetherPhysics_IsActive(nint world, uint handle);

    [LibraryImport(LibraryName)]
    internal static unsafe partial int AetherPhysics_RayCastClosest(nint world, float3 origin, float3 direction, uint* outBody, float* outHitFraction);

    // ---------------------------------------------------------------- juntas e motores (4.1.3)

    [LibraryImport(LibraryName)]
    internal static partial uint AetherPhysics_CreateJoint(nint world, uint body1, uint body2, in NativeJointDesc desc);

    [LibraryImport(LibraryName)]
    internal static partial uint AetherPhysics_CreateJointV2(nint world, uint body1, uint body2, in NativeJointDescV2 desc);

    [LibraryImport(LibraryName)]
    internal static partial void AetherPhysics_DestroyJoint(nint world, uint handle);

    [LibraryImport(LibraryName)]
    internal static partial void AetherPhysics_SetJointMotor(nint world, uint handle, in NativeJointMotorDesc motor);

    [LibraryImport(LibraryName)]
    internal static partial float AetherPhysics_GetJointPosition(nint world, uint handle);

    // ---------------------------------------------------------------- queries (4.1.4)

    [LibraryImport(LibraryName)]
    internal static unsafe partial int AetherPhysics_RayCastAll(nint world, float3 origin, float3 direction,
        NativeQueryLayerMask layerMask, uint ignoreBody, uint* outBodies, float* outFractions, int maxResults);

    [LibraryImport(LibraryName)]
    internal static unsafe partial int AetherPhysics_ShapeCastClosest(nint world, in NativeShapeDesc shape,
        float3 origin, quaternion rotation, float3 direction, NativeQueryLayerMask layerMask, uint ignoreBody,
        NativeShapeQueryHit* outHit);

    [LibraryImport(LibraryName)]
    internal static unsafe partial int AetherPhysics_OverlapShape(nint world, in NativeShapeDesc shape,
        float3 origin, quaternion rotation, NativeQueryLayerMask layerMask, uint ignoreBody,
        NativeShapeQueryHit* outHits, int maxResults);

    // ---------------------------------------------------------------- character controller (4.1.5)

    [LibraryImport(LibraryName)]
    internal static partial uint AetherPhysics_CreateCharacter(nint world, in NativeCharacterDesc desc, float3 position, quaternion rotation);

    [LibraryImport(LibraryName)]
    internal static partial void AetherPhysics_DestroyCharacter(nint world, uint handle);

    [LibraryImport(LibraryName)]
    internal static partial void AetherPhysics_SetCharacterVelocity(nint world, uint handle, float3 velocity);

    [LibraryImport(LibraryName)]
    internal static partial float3 AetherPhysics_GetCharacterVelocity(nint world, uint handle);

    [LibraryImport(LibraryName)]
    internal static partial void AetherPhysics_UpdateCharacter(nint world, uint handle, float deltaTime,
        float3 gravity, NativeQueryLayerMask layerMask, uint ignoreBody);

    [LibraryImport(LibraryName)]
    internal static unsafe partial void AetherPhysics_GetCharacterTransform(nint world, uint handle, float3* outPosition, quaternion* outRotation);

    [LibraryImport(LibraryName)]
    internal static partial NativeCharacterGroundState AetherPhysics_GetCharacterGroundState(nint world, uint handle);

    [LibraryImport(LibraryName)]
    internal static partial float3 AetherPhysics_GetCharacterGroundVelocity(nint world, uint handle);

    [LibraryImport(LibraryName)]
    internal static partial float3 AetherPhysics_GetCharacterGroundNormal(nint world, uint handle);

    [LibraryImport(LibraryName)]
    internal static partial int AetherPhysics_SetCharacterCrouching(nint world, uint handle, int crouching);
}
