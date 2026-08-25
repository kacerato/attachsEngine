using Aether.Physics;

namespace Aether.Tests;

/// <summary>
/// Testes do item 4.1.5: character controller — mover, detecção de chão/rampa, degraus,
/// deslizar, plataforma móvel, agachar — sobre PhysicsWorld.CreateCharacter/UpdateCharacter/etc.
/// Escalar e nadar não são cobertos (sem suporte nativo no Jolt, ver docs/ESTADO.md). Mesma
/// disciplina de tolerância/pulo-se-lib-ausente dos demais arquivos de teste de física; a
/// geometria de cada cenário foi validada experimentalmente contra o Jolt real antes de virar
/// teste (ver comentários — mesma disciplina do arquivo C++ irmão, test_character_bridge.cpp).
/// </summary>
public static class PhysicsCharacterTests
{
    private static bool NativeLibraryAvailable()
    {
        try { using var w = new PhysicsWorld(float3.Zero, 16); return true; }
        catch (DllNotFoundException) { return false; }
    }

    private static PhysicsBodyHandle MakeBox(PhysicsWorld physics, float3 halfExtent, float3 position,
        quaternion rotation, NativeMotionType motion) =>
        physics.CreateBody(PhysicsShape.Box(halfExtent), position, rotation, motion);

    private static void StepFreefall(PhysicsWorld physics, PhysicsCharacterHandle character, int steps, float dt)
    {
        var gravity = new float3(0f, -9.81f, 0f);
        for (int i = 0; i < steps; i++)
        {
            var v = physics.GetCharacterVelocity(character);
            v.Y += gravity.Y * dt;
            physics.SetCharacterVelocity(character, v);
            physics.UpdateCharacter(character, dt, gravity);
            physics.Step(dt);
        }
    }

    [Test] public static void CriarEDestruirCharacter_SemDeixarEstadoPendurado()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        var character = physics.CreateCharacter(CharacterDesc.Default, new float3(0f, 5f, 0f), quaternion.Identity);
        Assert.True(character.IsValid, "criação do character deveria ter sucesso");
        physics.DestroyCharacter(character);
    }

    [Test] public static void Character_CaiSobGravidadeEAssentaSobrePisoEstatico()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        MakeBox(physics, new float3(10f, 0.5f, 10f), float3.Zero, quaternion.Identity, NativeMotionType.Static);

        var character = physics.CreateCharacter(CharacterDesc.Default, new float3(0f, 5f, 0f), quaternion.Identity);
        StepFreefall(physics, character, 300, 1f / 60f); // 5s

        physics.GetCharacterTransform(character, out var position, out _);
        Assert.Close(0.5f, position.Y, eps: 0.05f, what: "character deveria assentar sobre o topo do piso");
        Assert.Equal(CharacterGroundState.OnGround, physics.GetCharacterGroundState(character), "character assentado deveria reportar OnGround");
    }

    [Test] public static void Character_EmQuedaSemPiso_ReportaEstadoEmAr()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        var character = physics.CreateCharacter(CharacterDesc.Default, new float3(0f, 50f, 0f), quaternion.Identity);
        StepFreefall(physics, character, 30, 1f / 60f); // meio segundo, sem nenhum piso no mundo

        Assert.Equal(CharacterGroundState.InAir, physics.GetCharacterGroundState(character), "character em queda livre sem piso deveria reportar InAir");
        physics.GetCharacterTransform(character, out var position, out _);
        Assert.True(position.Y < 50f, "character em queda livre deveria ter descido");
    }

    [Test] public static void Character_AndaSobreRampaAndavel_SemFicarPreso()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        // Rampa de 20° (< maxSlopeAngle 45° do CharacterDesc.Default), rotação em Z.
        const float rampAngle = 20f * MathF.PI / 180f;
        var rampRotation = new quaternion(0f, 0f, -MathF.Sin(rampAngle * 0.5f), MathF.Cos(rampAngle * 0.5f));
        MakeBox(physics, new float3(10f, 0.3f, 10f), float3.Zero, rampRotation, NativeMotionType.Static);

        var character = physics.CreateCharacter(CharacterDesc.Default, new float3(-3f, 3f, 0f), quaternion.Identity);
        StepFreefall(physics, character, 120, 1f / 60f); // 2s: cair e assentar na rampa

        Assert.Equal(CharacterGroundState.OnGround, physics.GetCharacterGroundState(character),
            "character sobre rampa de 20° (< maxSlopeAngle 45°) deveria estar OnGround");
    }

    [Test] public static void Character_ContraRampaIngremeDemais_PrimeiroContatoNaoEhOnGround()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        // Rampa de 80° (>> maxSlopeAngle 45°). x=0.3 é onde o centro da face inclinada (que era
        // o topo da caixa) cai no mundo, calculado a partir da geometria de rotação — um x fora
        // dessa faixa faz o character cair no vazio ao lado da caixa rotacionada.
        const float rampAngle = 80f * MathF.PI / 180f;
        var rampRotation = new quaternion(0f, 0f, -MathF.Sin(rampAngle * 0.5f), MathF.Cos(rampAngle * 0.5f));
        MakeBox(physics, new float3(10f, 0.3f, 10f), float3.Zero, rampRotation, NativeMotionType.Static);

        var character = physics.CreateCharacter(CharacterDesc.Default, new float3(0.3f, 3f, 0f), quaternion.Identity);

        // Captura o PRIMEIRO frame em que deixa de estar InAir — depois disso pode escorregar e
        // voltar a InAir (comportamento esperado de rampa íngreme demais), então checar só o
        // estado final aceitaria até "caiu no vazio" como resultado válido.
        var gravity = new float3(0f, -9.81f, 0f);
        var firstContactState = CharacterGroundState.InAir;
        bool sawContact = false;
        const float dt = 1f / 60f;
        for (int i = 0; i < 120 && !sawContact; i++)
        {
            var v = physics.GetCharacterVelocity(character);
            v.Y += gravity.Y * dt;
            physics.SetCharacterVelocity(character, v);
            physics.UpdateCharacter(character, dt, gravity);
            physics.Step(dt);
            var state = physics.GetCharacterGroundState(character);
            if (state != CharacterGroundState.InAir) { firstContactState = state; sawContact = true; }
        }

        Assert.True(sawContact, "character deveria ter encostado na rampa em algum momento da queda");
        Assert.True(firstContactState is CharacterGroundState.OnSteepGround or CharacterGroundState.NotSupported,
            "primeiro contato com rampa de 80° não pode ser classificado como OnGround normal");
    }

    [Test] public static void Character_SobeDegrauComUpdateCharacter()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        MakeBox(physics, new float3(1f, 0.5f, 10f), float3.Zero, quaternion.Identity, NativeMotionType.Static); // piso baixo
        MakeBox(physics, new float3(10f, 0.6f, 10f), new float3(11f, 0.1f, 0f), quaternion.Identity, NativeMotionType.Static); // degrau, x>=1

        var character = physics.CreateCharacter(CharacterDesc.Default, new float3(0f, 3f, 0f), quaternion.Identity);
        StepFreefall(physics, character, 120, 1f / 60f); // assenta no piso baixo

        physics.GetCharacterTransform(character, out var posBefore, out _);

        const float dt = 1f / 60f;
        for (int i = 0; i < 120; i++) // 2s andando para +X
        {
            // v.Y NÃO acumula a velocidade vertical anterior (mesmo padrão do teste nativo
            // irmão, test_character_bridge.cpp) — reinicia em 0 e soma só um passo de
            // gravidade a cada frame. Fisicamente incomum (a velocidade vertical nunca cresce
            // de verdade), mas é o que permite ExtendedUpdate subir o degrau sem o character
            // ter acumulado momento de queda para vencer a cada step-up.
            var v = new float3(2f, -9.81f * dt, 0f);
            if (physics.GetCharacterGroundState(character) != CharacterGroundState.InAir)
                v.X += physics.GetCharacterGroundVelocity(character).X;
            physics.SetCharacterVelocity(character, v);
            physics.UpdateCharacter(character, dt, new float3(0f, -9.81f, 0f));
            physics.Step(dt);
        }

        physics.GetCharacterTransform(character, out var posAfter, out _);
        Assert.True(posAfter.X > posBefore.X + 1f, "character deveria ter avançado em X, subindo o degrau via ExtendedUpdate");
    }

    [Test] public static void Character_GrudadoAPlataformaMovel_HerdaAVelocidadeDoChao()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        var platform = MakeBox(physics, new float3(5f, 0.5f, 5f), float3.Zero, quaternion.Identity, NativeMotionType.Kinematic);
        physics.SetLinearVelocity(platform, new float3(3f, 0f, 0f));

        var character = physics.CreateCharacter(CharacterDesc.Default, new float3(0f, 3f, 0f), quaternion.Identity);

        const float dt = 1f / 60f;
        for (int i = 0; i < 90; i++) // 1.5s: cai e assenta na plataforma
        {
            var v = float3.Zero;
            if (physics.GetCharacterGroundState(character) != CharacterGroundState.InAir)
            {
                var groundVel = physics.GetCharacterGroundVelocity(character);
                v.X = groundVel.X; v.Z = groundVel.Z;
            }
            v.Y = physics.GetCharacterVelocity(character).Y - 9.81f * dt;
            physics.SetCharacterVelocity(character, v);
            physics.UpdateCharacter(character, dt, new float3(0f, -9.81f, 0f));
            physics.Step(dt);
        }

        Assert.Equal(CharacterGroundState.OnGround, physics.GetCharacterGroundState(character),
            "character deveria estar assentado sobre a plataforma antes de medir arrasto");
        physics.GetCharacterTransform(character, out var posBefore, out _);

        for (int i = 0; i < 60; i++) // mais 1s sobre a plataforma em movimento
        {
            var groundVel = physics.GetCharacterGroundVelocity(character);
            var v = new float3(groundVel.X, physics.GetCharacterVelocity(character).Y - 9.81f * dt, groundVel.Z);
            physics.SetCharacterVelocity(character, v);
            physics.UpdateCharacter(character, dt, new float3(0f, -9.81f, 0f));
            physics.Step(dt);
        }

        physics.GetCharacterTransform(character, out var posAfter, out _);
        float deltaX = posAfter.X - posBefore.X;
        Assert.True(deltaX > 1.5f, "character deveria ter sido arrastado pela plataforma móvel em X");
    }

    [Test] public static void AgacharTrocaParaCapsulaMenor_ELevantarTrocaDeVolta()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        MakeBox(physics, new float3(10f, 0.5f, 10f), float3.Zero, quaternion.Identity, NativeMotionType.Static);

        var character = physics.CreateCharacter(CharacterDesc.Default, new float3(0f, 5f, 0f), quaternion.Identity);
        StepFreefall(physics, character, 120, 1f / 60f);

        Assert.True(physics.SetCharacterCrouching(character, true), "agachar em espaço livre deveria ter sucesso");
        Assert.True(physics.SetCharacterCrouching(character, false), "levantar em espaço livre deveria ter sucesso");
    }

    [Test] public static void AgacharDebaixoDeTetoBaixo_ImpedeLevantar()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);
        MakeBox(physics, new float3(10f, 0.5f, 10f), float3.Zero, quaternion.Identity, NativeMotionType.Static); // piso, topo y=0.5

        var desc = CharacterDesc.Default; // pé 2.4m, agachado 1.4m
        // Vão de 1.6m (piso topo=0.5, teto base=2.1): cabe agachado (0.2m de folga), não cabe de
        // pé (2.4m). Geometria validada experimentalmente contra o Jolt real (ver comentário
        // equivalente em test_character_bridge.cpp) — um vão menor que a altura agachada faz o
        // penetration recovery do Jolt empurrar o character para CIMA do teto fino em vez de
        // mantê-lo no chão (resolve sempre pelo caminho de menor penetração; não é bug nosso).
        MakeBox(physics, new float3(2f, 0.1f, 2f), new float3(0f, 2.2f, 0f), quaternion.Identity, NativeMotionType.Static);

        var character = physics.CreateCharacter(desc, new float3(0f, 0.5f, 0f), quaternion.Identity);
        Assert.True(physics.SetCharacterCrouching(character, true), "agachar antes de assentar deveria ter sucesso");
        StepFreefall(physics, character, 60, 1f / 60f); // 1s, assenta de vez

        physics.GetCharacterTransform(character, out var settledPos, out _);
        Assert.Close(0.5f, settledPos.Y, eps: 0.05f, what: "character agachado deveria assentar sobre o piso");

        Assert.False(physics.SetCharacterCrouching(character, false),
            "levantar debaixo de um teto baixo demais para a altura de pé deveria falhar");
    }

    [Test] public static void DestroyCharacter_ChamadoDuasVezes_NaoLancaEHandleReciclado_TemGeracaoDiferente()
    {
        if (!NativeLibraryAvailable()) return;
        using var physics = new PhysicsWorld(new float3(0f, -9.81f, 0f), 16);

        var first = physics.CreateCharacter(CharacterDesc.Default, new float3(0f, 5f, 0f), quaternion.Identity);
        Assert.True(first.IsValid, "primeiro character deveria ser criado");
        physics.DestroyCharacter(first);
        physics.DestroyCharacter(first); // chamar de novo não pode lançar

        var second = physics.CreateCharacter(CharacterDesc.Default, new float3(5f, 5f, 0f), quaternion.Identity);
        Assert.True(second.IsValid, "segundo character (reciclando o slot) deveria ser criado");
        Assert.NotEqual(first, second, "handle reciclado deveria ter geração diferente do handle antigo");
    }
}
