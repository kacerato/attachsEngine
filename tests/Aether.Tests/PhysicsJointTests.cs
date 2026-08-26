using Aether.Physics;

namespace Aether.Tests;

/// <summary>
/// Testes do item 4.1.3: juntas e motores (Point, Hinge, Slider, Distance) sobre
/// PhysicsWorld.CreateJoint/DestroyJoint/SetJointMotor/GetJointPosition. Mesma disciplina de
/// PhysicsTests.cs — números vêm de dinâmica real do Jolt via P/Invoke, com tolerância larga o
/// suficiente para o integrador mas estreita o bastante para pegar erro de eixo/sinal/unidade.
/// Pulados (retorno cedo, não falha) se a biblioteca nativa estiver ausente — mesmo padrão de
/// PhysicsTests.NativeLibraryAvailable.
/// </summary>
public static class PhysicsJointTests
{
    private static bool NativeLibraryAvailable() => NativeInterop.PhysicsLibraryAvailable();

    private static PhysicsBodyHandle MakeSphere(PhysicsWorld physics, float3 position, NativeMotionType motion) =>
        physics.CreateBody(PhysicsShape.Sphere(0.5f), position, quaternion.Identity, motion);

    [Test] public static void JuntaPonto_PrendeCorpoDinamicoAAncoraEstatica()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);

        var anchor = MakeSphere(physics, new float3(0f, 10f, 0f), NativeMotionType.Static);
        var bob = MakeSphere(physics, new float3(0f, 8f, 0f), NativeMotionType.Dynamic);

        var desc = new JointDesc
        {
            Kind = JointKind.Point,
            Point1 = new float3(0f, 8f, 0f),
            Point2 = new float3(0f, 8f, 0f),
            Motor = JointMotor.Off,
        };
        var joint = physics.CreateJoint(anchor, bob, in desc);
        Assert.True(joint.IsValid, "criação da junta ponto deveria ter sucesso");

        for (int i = 0; i < 120; i++) physics.Step(1f / 60f);

        physics.GetTransform(bob, out var position, out _);
        Assert.Close(8f, position.Y, eps: 0.05f, what: "corpo preso por junta ponto não pode cair sob gravidade");
    }

    [Test] public static void JuntaDobradica_ComLimites_AnguloNuncaUltrapassaOLimite()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);

        var anchor = MakeSphere(physics, new float3(0f, 10f, 0f), NativeMotionType.Static);
        var arm = MakeSphere(physics, new float3(1f, 10f, 0f), NativeMotionType.Dynamic);

        var desc = new JointDesc
        {
            Kind = JointKind.Hinge,
            Point1 = new float3(0f, 10f, 0f),
            Point2 = new float3(0f, 10f, 0f),
            Axis1 = new float3(0f, 0f, 1f),
            Axis2 = new float3(0f, 0f, 1f),
            LimitsMin = -1.4f,
            LimitsMax = 1.4f,
            Motor = JointMotor.Off,
        };
        var joint = physics.CreateJoint(anchor, arm, in desc);
        Assert.True(joint.IsValid, "criação da junta dobradiça deveria ter sucesso");

        float maxAbsAngleSeen = 0f;
        for (int i = 0; i < 300; i++)
        {
            physics.Step(1f / 60f);
            maxAbsAngleSeen = MathF.Max(maxAbsAngleSeen, MathF.Abs(physics.GetJointPosition(joint)));
        }

        Assert.True(maxAbsAngleSeen <= 1.4f + 0.1f, "ângulo não pode ultrapassar o limite configurado (com folga do solver)");
        Assert.True(maxAbsAngleSeen > 0.3f, "o pêndulo deveria ter caído de fato, não ficado parado perto de zero");
    }

    [Test] public static void MotorDeDobradica_ComVelocidadeAlvo_GiraOBracoNaVelocidadeCorreta()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16); // sem gravidade: isola o efeito do motor

        var anchor = MakeSphere(physics, float3.Zero, NativeMotionType.Static);
        var arm = MakeSphere(physics, new float3(1f, 0f, 0f), NativeMotionType.Dynamic);

        var desc = new JointDesc
        {
            Kind = JointKind.Hinge,
            Axis1 = new float3(0f, 0f, 1f),
            Axis2 = new float3(0f, 0f, 1f),
            // [-pi,pi] exatos = "sem limite" para Hinge no Jolt (ver aviso em JointDesc) — não é
            // um limite muito largo, é o valor específico que desliga a checagem de limite.
            LimitsMin = -MathF.PI,
            LimitsMax = MathF.PI,
            Motor = JointMotor.Velocity(targetVelocity: 1f, maxForceOrTorque: 100000f),
        };
        var joint = physics.CreateJoint(anchor, arm, in desc);
        Assert.True(joint.IsValid, "criação da junta motorizada deveria ter sucesso");

        const float dt = 1f / 60f;
        for (int i = 0; i < 5; i++) physics.Step(dt); // ignora a rampa inicial de aceleração do motor
        float angleAtWarmupEnd = physics.GetJointPosition(joint);

        const int measuredSteps = 25;
        for (int i = 0; i < measuredSteps; i++) physics.Step(dt);
        float angleAfter = physics.GetJointPosition(joint);

        float measuredVelocity = (angleAfter - angleAtWarmupEnd) / (dt * measuredSteps);
        Assert.Close(1f, measuredVelocity, eps: 0.1f, what: "motor de velocidade deveria manter ~1 rad/s depois da rampa inicial");
    }

    [Test] public static void MotorDeSlider_ComPosicaoAlvo_ConvergeParaOAlvo()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(float3.Zero, 16);

        var anchor = MakeSphere(physics, float3.Zero, NativeMotionType.Static);
        var piston = MakeSphere(physics, float3.Zero, NativeMotionType.Dynamic);

        var desc = new JointDesc
        {
            Kind = JointKind.Slider,
            Axis1 = new float3(1f, 0f, 0f),
            Axis2 = new float3(1f, 0f, 0f),
            LimitsMin = -5f,
            LimitsMax = 5f,
            // maxForceOrTorque alto o bastante para a mola nunca saturar antes do alvo — força
            // baixa demais converge para um equilíbrio bem antes do alvo, não é bug de sinal
            // (mesma pegadinha documentada em tests/native/test_joint_bridge.cpp).
            Motor = JointMotor.Position(targetPosition: 2f, maxForceOrTorque: 1000000f),
        };
        var joint = physics.CreateJoint(anchor, piston, in desc);
        Assert.True(joint.IsValid, "criação da junta slider motorizada deveria ter sucesso");

        for (int i = 0; i < 180; i++) physics.Step(1f / 60f); // 3s, tempo generoso para convergir

        Assert.Close(2f, physics.GetJointPosition(joint), eps: 0.1f, what: "motor de posição deveria convergir para o alvo");
    }

    [Test] public static void JuntaDistancia_LimitaAfastamentoEntreDoisCorpos()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);

        var anchor = MakeSphere(physics, new float3(0f, 10f, 0f), NativeMotionType.Static);
        var bob = MakeSphere(physics, new float3(0f, 8f, 0f), NativeMotionType.Dynamic);

        var desc = new JointDesc
        {
            Kind = JointKind.Distance,
            Point1 = new float3(0f, 10f, 0f),
            Point2 = new float3(0f, 8f, 0f),
            LimitsMin = 0f,
            LimitsMax = 3f,
            Motor = JointMotor.Off,
        };
        var joint = physics.CreateJoint(anchor, bob, in desc);
        Assert.True(joint.IsValid, "criação da junta distância deveria ter sucesso");

        for (int i = 0; i < 300; i++) physics.Step(1f / 60f); // 5s, tempo de sobra para esticar e estabilizar

        physics.GetTransform(bob, out var position, out _);
        float distanceFromAnchor = 10f - position.Y;
        Assert.True(distanceFromAnchor <= 3f + 0.05f, "corpo não pode se afastar além do limite máximo");
        Assert.True(distanceFromAnchor >= 3f - 0.3f, "sob gravidade sustentada, deveria terminar esticada perto do limite");
    }

    [Test] public static void DestroyJoint_LiberaOCorpoQueVoltaACairLivremente()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);

        var anchor = MakeSphere(physics, new float3(0f, 10f, 0f), NativeMotionType.Static);
        var bob = MakeSphere(physics, new float3(0f, 8f, 0f), NativeMotionType.Dynamic);

        var desc = new JointDesc
        {
            Kind = JointKind.Point,
            Point1 = new float3(0f, 8f, 0f),
            Point2 = new float3(0f, 8f, 0f),
            Motor = JointMotor.Off,
        };
        var joint = physics.CreateJoint(anchor, bob, in desc);

        for (int i = 0; i < 60; i++) physics.Step(1f / 60f);
        physics.GetTransform(bob, out var heldPosition, out _);
        Assert.Close(8f, heldPosition.Y, eps: 0.05f, what: "corpo deveria estar preso antes da destruição da junta");

        physics.DestroyJoint(joint);

        for (int i = 0; i < 60; i++) physics.Step(1f / 60f); // 1s de queda livre após soltar
        physics.GetTransform(bob, out var freePosition, out _);
        Assert.True(freePosition.Y < heldPosition.Y - 1f, "corpo deveria ter caído livremente depois da destruição da junta");
    }

    [Test] public static void CreateJoint_ComHandleDeCorpoInvalido_DevolveHandleInvalidoSemLancar()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        var body = MakeSphere(physics, float3.Zero, NativeMotionType.Dynamic);

        var desc = new JointDesc { Kind = JointKind.Point, Motor = JointMotor.Off };
        var joint = physics.CreateJoint(PhysicsBodyHandle.Invalid, body, in desc);
        Assert.False(joint.IsValid, "handle de corpo inválido não pode criar junta");
    }

    [Test] public static void DestroyJoint_ChamadoDuasVezes_NaoLancaEHandleReciclado_TemGeracaoDiferente()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        var a = MakeSphere(physics, float3.Zero, NativeMotionType.Static);
        var b = MakeSphere(physics, new float3(1f, 0f, 0f), NativeMotionType.Dynamic);

        var desc = new JointDesc { Kind = JointKind.Point, Point1 = new float3(0.5f, 0f, 0f), Point2 = new float3(0.5f, 0f, 0f), Motor = JointMotor.Off };
        var first = physics.CreateJoint(a, b, in desc);
        Assert.True(first.IsValid, "primeira junta deveria ser criada");

        physics.DestroyJoint(first);
        physics.DestroyJoint(first); // chamar de novo não pode lançar

        var c = MakeSphere(physics, new float3(2f, 0f, 0f), NativeMotionType.Dynamic);
        var second = physics.CreateJoint(a, c, in desc);
        Assert.True(second.IsValid, "segunda junta (reciclando o slot) deveria ser criada");
        Assert.NotEqual(first, second, "handle reciclado deveria ter geração diferente do handle antigo");
    }
}
