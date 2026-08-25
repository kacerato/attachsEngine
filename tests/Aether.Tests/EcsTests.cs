using System.Diagnostics;

namespace Aether.Tests;

public static class EcsTests
{
    private struct Position { public float3 Value; public Position(float3 v) => Value = v; }
    private struct Velocity { public float3 Value; public Velocity(float3 v) => Value = v; }
    private struct Health { public int Value; public Health(int v) => Value = v; }
    private struct Tag { public int Marker = 1; public Tag() { } }

    // ---------------------------------------------------------------- criação / destruição / versão

    [Test] public static void CriarEDestruirEntidade_ExistsReflete()
    {
        var world = new World();
        var e = world.CreateEntity();
        Assert.True(world.Exists(e), "entidade recém-criada existe");
        Assert.Equal(1, world.EntityCount);

        world.DestroyEntity(e);
        Assert.False(world.Exists(e), "entidade destruída não existe mais");
        Assert.Equal(0, world.EntityCount);
    }

    [Test] public static void IdReciclado_ComVersaoNova_NaoEhConfundidoComOAntigo()
    {
        var world = new World();
        var primeiro = world.CreateEntity();
        world.DestroyEntity(primeiro);

        var segundo = world.CreateEntity();   // deve reciclar o mesmo índice
        Assert.Equal(primeiro.Index, segundo.Index, "o índice do slot livre foi reaproveitado");
        Assert.NotEqual(primeiro.Version, segundo.Version, "a versão avançou");
        Assert.False(world.Exists(primeiro), "o id antigo, mesmo com índice reciclado, não é válido");
        Assert.True(world.Exists(segundo));
    }

    [Test] public static void DestruirEntidadeQueNaoExiste_NaoAcontece_EhErroDeUso()
    {
        var world = new World();
        var e = world.CreateEntity();
        world.DestroyEntity(e);
        Assert.Throws<InvalidOperationException>(() => world.DestroyEntity(e), "destruir duas vezes deve falhar, não corromper o estado");
    }

    // ---------------------------------------------------------------- componentes

    [Test] public static void GetSetComponent_LeemEEscrevemOMesmoArmazenamento()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(new float3(1, 2, 3)));

        Assert.Close(new float3(1, 2, 3), world.GetComponent<Position>(e).Value);

        world.GetComponent<Position>(e).Value = new float3(9, 9, 9);
        Assert.Close(new float3(9, 9, 9), world.GetComponent<Position>(e).Value, what: "escrita pela referência devolvida persiste");

        world.SetComponent(e, new Position(new float3(-1, -1, -1)));
        Assert.Close(new float3(-1, -1, -1), world.GetComponent<Position>(e).Value);
    }

    [Test] public static void TryGetComponent_DevolveFalseQuandoAusente()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(float3.Zero));

        Assert.True(world.TryGetComponent<Position>(e, out _));
        Assert.False(world.TryGetComponent<Velocity>(e, out _), "entidade não tem Velocity");
    }

    [Test] public static void HasComponent_ReflexoDoArquetipoAtual()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(float3.Zero));
        Assert.True(world.HasComponent<Position>(e));
        Assert.False(world.HasComponent<Velocity>(e));

        world.AddComponent(e, new Velocity(float3.One));
        Assert.True(world.HasComponent<Velocity>(e));
    }

    [Test] public static void AddComponent_MigraArquetipoPreservandoOsOutrosComponentes()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(new float3(1, 2, 3)));

        world.AddComponent(e, new Velocity(new float3(4, 5, 6)));

        Assert.Close(new float3(1, 2, 3), world.GetComponent<Position>(e).Value, what: "Position sobrevive à migração de arquétipo");
        Assert.Close(new float3(4, 5, 6), world.GetComponent<Velocity>(e).Value);
    }

    [Test] public static void RemoveComponent_MigraArquetipoPreservandoOsOutrosComponentes()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(new float3(1, 2, 3)), new Velocity(new float3(4, 5, 6)));

        world.RemoveComponent<Velocity>(e);

        Assert.False(world.HasComponent<Velocity>(e));
        Assert.Close(new float3(1, 2, 3), world.GetComponent<Position>(e).Value, what: "Position sobrevive à remoção de Velocity");
    }

    [Test] public static void AddComponent_DeTipoJaExistente_Lanca()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(float3.Zero));
        Assert.Throws<InvalidOperationException>(() => world.AddComponent(e, new Position(float3.One)),
            "componente duplicado deve usar SetComponent, não AddComponent");
    }

    [Test] public static void RemoveComponent_DeTipoAusente_Lanca()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(float3.Zero));
        Assert.Throws<InvalidOperationException>(() => world.RemoveComponent<Velocity>(e));
    }

    [Test] public static void GetComponent_EmEntidadeDestruida_LancaComMensagemEmPortugues()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(float3.Zero));
        world.DestroyEntity(e);

        try
        {
            world.GetComponent<Position>(e);
            throw new AssertException("esperado InvalidOperationException, nada foi lançado");
        }
        catch (InvalidOperationException ex)
        {
            Assert.True(ex.Message.Contains("não existe") || ex.Message.Contains("destruída"),
                "mensagem de erro deve ser compreensível em português");
        }
    }

    // ---------------------------------------------------------------- consultas

    [Test] public static void Query_ComWithSoDevolveEntidadesComTodosOsComponentes()
    {
        var world = new World();
        var comAmbos = world.CreateEntity(new Position(float3.Zero), new Velocity(float3.One));
        var soPosition = world.CreateEntity(new Position(float3.Zero));
        var soVelocity = world.CreateEntity(new Velocity(float3.One));

        var achados = new HashSet<EntityId>();
        foreach (var chunk in world.Query<Position, Velocity>())
        {
            var entities = chunk.Entities;
            for (int i = 0; i < chunk.Count; i++) achados.Add(entities[i]);
        }

        Assert.True(achados.Contains(comAmbos));
        Assert.False(achados.Contains(soPosition));
        Assert.False(achados.Contains(soVelocity));
        Assert.Equal(1, achados.Count);
    }

    [Test] public static void Query_ComWithoutExcluiEntidadesComOComponenteProibido()
    {
        var world = new World();
        var vivo = world.CreateEntity(new Position(float3.Zero), new Health(10));
        var morto = world.CreateEntity(new Position(float3.Zero), new Health(0));
        world.AddComponent(morto, new Tag());

        var achados = new HashSet<EntityId>();
        foreach (var chunk in world.Query().With<Position>().With<Health>().Without<Tag>())
        {
            var entities = chunk.Entities;
            for (int i = 0; i < chunk.Count; i++) achados.Add(entities[i]);
        }

        Assert.True(achados.Contains(vivo));
        Assert.False(achados.Contains(morto), "morto tem Tag, deve ser excluído por Without<Tag>");
    }

    [Test] public static void Query_SobreMundoVazio_NaoExplode()
    {
        var world = new World();
        int total = 0;
        foreach (var chunk in world.Query<Position, Velocity>()) total += chunk.Count;
        Assert.Equal(0, total);

        foreach (var chunk in world.Query().With<Position>().Without<Velocity>()) total += chunk.Count;
        Assert.Equal(0, total);
    }

    [Test] public static void Query_IteracaoNaoAloca()
    {
        var world = new World();
        for (int i = 0; i < 256; i++)
            world.CreateEntity(new Position(new float3(i, 0, 0)), new Velocity(float3.One));

        float acc = 0f;
        Assert.NoAlloc(() =>
        {
            foreach (var chunk in world.Query<Position, Velocity>())
            {
                var pos = chunk.GetSpan<Position>();
                var vel = chunk.GetSpan<Velocity>();
                for (int i = 0; i < chunk.Count; i++)
                {
                    pos[i].Value += vel[i].Value;
                    acc += pos[i].Value.X;
                }
            }
        }, "iteração por chunk sobre 256 entidades");
        Assert.True(!float.IsNaN(acc));
    }

    // ---------------------------------------------------------------- swap-back / integridade do chunk

    [Test] public static void DestruirEntidadeNoMeioDoChunk_SwapBackMantemAsOutrasIntegras()
    {
        var world = new World();
        var ids = new EntityId[10];
        for (int i = 0; i < ids.Length; i++)
            ids[i] = world.CreateEntity(new Position(new float3(i, 0, 0)), new Health(i * 10));

        world.DestroyEntity(ids[4]);   // remove do meio

        Assert.Equal(9, world.EntityCount);
        for (int i = 0; i < ids.Length; i++)
        {
            if (i == 4) { Assert.False(world.Exists(ids[i])); continue; }
            Assert.True(world.Exists(ids[i]), $"entidade {i} deveria sobreviver ao swap-back");
            Assert.Close((float)i, world.GetComponent<Position>(ids[i]).Value.X, what: $"Position da entidade {i} não pode ter sido embaralhada");
            Assert.Equal(i * 10, world.GetComponent<Health>(ids[i]).Value, what: $"Health da entidade {i} não pode ter sido embaralhado");
        }
    }

    // ---------------------------------------------------------------- EntityCommandBuffer

    [Test] public static void ECB_CriacaoDiferida_SoAparaceNoPlayback()
    {
        var world = new World();
        var ecb = new EntityCommandBuffer();
        var temp = ecb.CreateEntity();

        Assert.Equal(0, world.EntityCount, "nada foi criado antes do playback");
        ecb.Playback(world);
        Assert.Equal(1, world.EntityCount);
    }

    [Test] public static void ECB_DestruicaoDiferida()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(float3.Zero));
        var ecb = new EntityCommandBuffer();
        ecb.DestroyEntity(e);

        Assert.True(world.Exists(e), "destruição só acontece no playback");
        ecb.Playback(world);
        Assert.False(world.Exists(e));
    }

    [Test] public static void ECB_ReferenciaAEntidadeCriadaNoProprioBuffer()
    {
        var world = new World();
        var ecb = new EntityCommandBuffer();
        var tempPai = ecb.CreateEntity();
        var tempFilho = ecb.CreateEntity();
        ecb.AddComponent(tempPai, new Position(new float3(1, 0, 0)));
        // O valor do componente referencia outra entidade temporária do mesmo buffer: só pode ser
        // resolvido no playback, daí a variante de AddComponent com fábrica + resolvedor.
        ecb.AddComponent<Parent>(tempFilho, resolve => new Parent(resolve(tempPai)));

        ecb.Playback(world);

        Assert.Equal(2, world.EntityCount);
        EntityId pai = default, filho = default;
        bool paiAchado = false, filhoAchado = false;
        foreach (var chunk in world.Query<Position>())
        {
            var e = chunk.Entities;
            for (int i = 0; i < chunk.Count; i++) { pai = e[i]; paiAchado = true; }
        }
        foreach (var chunk in world.Query<Parent>())
        {
            var e = chunk.Entities;
            for (int i = 0; i < chunk.Count; i++) { filho = e[i]; filhoAchado = true; }
        }
        Assert.True(paiAchado); Assert.True(filhoAchado);
        Assert.True(world.GetComponent<Parent>(filho).Value == pai, "a referência ao id temporário do pai foi resolvida corretamente no playback");
    }

    [Test] public static void ECB_AddComponentDiferidoEmEntidadeJaExistente()
    {
        var world = new World();
        var e = world.CreateEntity();
        var ecb = new EntityCommandBuffer();
        ecb.AddComponent(e, new Position(new float3(7, 7, 7)));
        ecb.Playback(world);
        Assert.Close(new float3(7, 7, 7), world.GetComponent<Position>(e).Value);
    }

    // ---------------------------------------------------------------- hierarquia

    [Test] public static void Hierarquia_PaiMovidoMoveOFilho()
    {
        var world = new World();
        var pai = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(10, 0, 0))),
            new WorldTransform(Transform.Identity));
        var filho = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(0, 0, 2))),
            new WorldTransform(Transform.Identity));
        world.AddComponent(filho, new Parent(pai));

        TransformSystem.Propagate(world);
        Assert.Close(new float3(10, 0, 2), world.GetComponent<WorldTransform>(filho).Value.Position, what: "filho segue o pai");

        world.GetComponent<LocalTransform>(pai).Value = Transform.FromPosition(new float3(100, 0, 0));
        TransformSystem.Propagate(world);
        Assert.Close(new float3(100, 0, 2), world.GetComponent<WorldTransform>(filho).Value.Position, what: "mover o pai move o filho");
    }

    [Test] public static void Hierarquia_TresNiveis_PropagaCorretamente()
    {
        var world = new World();
        var avo = world.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(1, 0, 0))), new WorldTransform(Transform.Identity));
        var pai = world.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(0, 1, 0))), new WorldTransform(Transform.Identity));
        var neto = world.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(0, 0, 1))), new WorldTransform(Transform.Identity));
        world.AddComponent(pai, new Parent(avo));
        world.AddComponent(neto, new Parent(pai));

        TransformSystem.Propagate(world);

        Assert.Close(new float3(1, 0, 0), world.GetComponent<WorldTransform>(avo).Value.Position);
        Assert.Close(new float3(1, 1, 0), world.GetComponent<WorldTransform>(pai).Value.Position);
        Assert.Close(new float3(1, 1, 1), world.GetComponent<WorldTransform>(neto).Value.Position, what: "neto acumula translação de avô e pai");
    }

    [Test] public static void Hierarquia_DestruirPai_NetoNaoTravaEEhTratadoComoRaiz()
    {
        var world = new World();
        var pai = world.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(5, 0, 0))), new WorldTransform(Transform.Identity));
        var filho = world.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(0, 3, 0))), new WorldTransform(Transform.Identity));
        world.AddComponent(filho, new Parent(pai));

        world.DestroyEntity(pai);

        TransformSystem.Propagate(world);   // não pode lançar nem travar
        Assert.Close(new float3(0, 3, 0), world.GetComponent<WorldTransform>(filho).Value.Position,
            what: "órfão vira raiz: WorldTransform = LocalTransform");
    }

    // ---------------------------------------------------------------- escala

    [Test] public static void Escala_CemMilEntidades_ConsultaEAtualizacaoCorretasSemAlocar()
    {
        const int n = 100_000;
        var world = new World();

        var sw = Stopwatch.StartNew();
        for (int i = 0; i < n; i++)
            world.CreateEntity(new Position(new float3(i, 0, 0)), new Velocity(new float3(0, 1, 0)));
        sw.Stop();
        Console.WriteLine($"    [escala] criar {n} entidades: {sw.ElapsedMilliseconds} ms");

        const float dt = 1f / 60f;
        sw.Restart();
        Assert.NoAlloc(() =>
        {
            foreach (var chunk in world.Query<Position, Velocity>())
            {
                var pos = chunk.GetSpan<Position>();
                var vel = chunk.GetSpan<Velocity>();
                for (int i = 0; i < chunk.Count; i++)
                    pos[i].Value += vel[i].Value * dt;
            }
        }, "atualização de 100k entidades por chunk");
        sw.Stop();
        Console.WriteLine($"    [escala] iterar+atualizar {n} entidades: {sw.ElapsedMilliseconds} ms");

        // Cada Position.X deveria ter avançado exatamente dt em Y (a Velocity é (0,1,0)) e mantido X
        // original — verifica que a atualização "por chunk" não misturou linhas de entidades diferentes.
        int visited = 0;
        foreach (var chunk in world.Query<Position, Velocity>())
        {
            var pos = chunk.GetSpan<Position>();
            var entities = chunk.Entities;
            for (int i = 0; i < chunk.Count; i++)
            {
                Assert.Close((float)entities[i].Index, pos[i].Value.X, 1e-3f, what: "X não deveria ter mudado (velocidade só afeta Y)");
                // Assert.NoAlloc roda a ação duas vezes (aquecimento + medição), então dt foi aplicado duas vezes.
                Assert.Close(2f * dt, pos[i].Value.Y, 1e-3f, what: "Y avançou dois passos de dt (NoAlloc roda a ação duas vezes)");
            }
            visited += chunk.Count;
        }
        Assert.Equal(n, visited, "todas as entidades foram visitadas exatamente uma vez");
        Assert.Equal(n, world.EntityCount);
    }
}
