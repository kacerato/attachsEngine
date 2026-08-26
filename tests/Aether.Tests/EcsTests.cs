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

        Assert.Close(new float3(1, 2, 3), world.Read<Position>(e).Value);

        world.Write<Position>(e).Value = new float3(9, 9, 9);
        Assert.Close(new float3(9, 9, 9), world.Read<Position>(e).Value, what: "escrita pela referência devolvida persiste");

        world.SetComponent(e, new Position(new float3(-1, -1, -1)));
        Assert.Close(new float3(-1, -1, -1), world.Read<Position>(e).Value);
    }

    [Test] public static void TryGetComponent_DevolveFalseQuandoAusente()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(float3.Zero));

        Assert.True(world.TryGetComponent<Position>(e, out _));
        Assert.False(world.TryGetComponent<Velocity>(e, out _), "entidade não tem Velocity");
    }

    [Test] public static void ChangeDetection_LeituraNaoMarca_EscritaMarcaSomenteAColuna()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(float3.Zero), new Velocity(float3.One));
        Chunk chunk = null!;
        foreach (var candidate in world.Query<Position, Velocity>()) { chunk = candidate; break; }

        int positionBefore = chunk.GetChangeVersion<Position>();
        int velocityBefore = chunk.GetChangeVersion<Velocity>();

        _ = world.Read<Position>(e).Value;
        Assert.True(world.TryGetComponent<Position>(e, out _));
        _ = chunk.GetReadOnlySpan<Position>()[0];
        Assert.Equal(positionBefore, chunk.GetChangeVersion<Position>(), "leituras não podem sujar Position");
        Assert.Equal(velocityBefore, chunk.GetChangeVersion<Velocity>(), "ler Position não pode sujar Velocity");

        ref var position = ref world.Write<Position>(e);
        position.Value = new float3(1, 2, 3);
        position.Value.X = 4;
        Assert.Equal(positionBefore + 1, chunk.GetChangeVersion<Position>(),
            "uma referência Write marca uma vez, não uma vez por campo alterado");
        Assert.Equal(velocityBefore, chunk.GetChangeVersion<Velocity>());

        var writable = chunk.GetWritableSpan<Position>();
        writable[0].Value.Y = 8;
        writable[0].Value.Z = 9;
        Assert.Equal(positionBefore + 2, chunk.GetChangeVersion<Position>(),
            "um span mutável marca uma vez por acesso lógico");
    }

    [Test] public static void ChangeDetection_EscritaMarcaExatamenteOChunkAcessado()
    {
        var world = new World();
        var first = world.CreateEntity(new Position(float3.Zero));
        Chunk firstChunk = null!;
        foreach (var candidate in world.Query<Position>()) { firstChunk = candidate; break; }
        for (int i = 1; i <= firstChunk.Capacity; i++)
            world.CreateEntity(new Position(new float3(i, 0, 0)));

        var chunks = new List<Chunk>();
        foreach (var chunk in world.Query<Position>()) chunks.Add(chunk);
        Assert.Equal(2, chunks.Count, "o teste precisa de dois chunks do mesmo arquétipo");
        int firstBefore = chunks[0].GetChangeVersion<Position>();
        int secondBefore = chunks[1].GetChangeVersion<Position>();

        chunks[1].GetWritableSpan<Position>()[0].Value = float3.One;

        Assert.Equal(firstBefore, chunks[0].GetChangeVersion<Position>(),
            "escrever no segundo chunk não pode sujar o primeiro");
        Assert.Equal(secondBefore + 1, chunks[1].GetChangeVersion<Position>());
        Assert.True(world.Exists(first));
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

        Assert.Close(new float3(1, 2, 3), world.Read<Position>(e).Value, what: "Position sobrevive à migração de arquétipo");
        Assert.Close(new float3(4, 5, 6), world.Read<Velocity>(e).Value);
    }

    [Test] public static void RemoveComponent_MigraArquetipoPreservandoOsOutrosComponentes()
    {
        var world = new World();
        var e = world.CreateEntity(new Position(new float3(1, 2, 3)), new Velocity(new float3(4, 5, 6)));

        world.RemoveComponent<Velocity>(e);

        Assert.False(world.HasComponent<Velocity>(e));
        Assert.Close(new float3(1, 2, 3), world.Read<Position>(e).Value, what: "Position sobrevive à remoção de Velocity");
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
            _ = world.Read<Position>(e);
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

    [Test] public static void QueryCompilada_ReusaCacheEMantemArquetiposNovosEExclusoes()
    {
        var world = new World();
        var query = world.Query().With<Position>().Without<Tag>().Compile();
        int CountEntities()
        {
            int count = 0;
            foreach (var chunk in query) count += chunk.Count;
            return count;
        }

        Assert.Equal(0, CountEntities());
        world.CreateEntity(new Position(float3.Zero));
        Assert.Equal(1, CountEntities(), "consulta compilada precisa observar arquétipo criado depois dela");
        world.CreateEntity(new Position(float3.One), new Health(10));
        Assert.Equal(2, CountEntities(), "novo arquétipo compatível invalida somente o cache de matches");
        var excluded = world.CreateEntity(new Position(float3.One));
        world.AddComponent(excluded, new Tag());
        Assert.Equal(2, CountEntities(), "Without continua aplicado depois de mudanças estruturais");

        Assert.NoAlloc(() => _ = CountEntities(), "consulta compilada estável reutiliza cache sem GC");
    }

    [Test] public static void FiltroDeMudanca_EntregaSomenteChunkEColunaAlteradosESemGcEstavel()
    {
        var world = new World();
        var first = world.CreateEntity(new Position(float3.Zero), new Velocity(float3.One));
        Chunk firstChunk = null!;
        foreach (var chunk in world.Query<Position, Velocity>()) { firstChunk = chunk; break; }
        EntityId secondChunkEntity = default;
        for (int i = 1; i <= firstChunk.Capacity; i++)
            secondChunkEntity = world.CreateEntity(new Position(new float3(i, 0, 0)), new Velocity(float3.One));

        var filter = new ComponentChangeFilter<Position>();
        var changed = world.Query().With<Velocity>().Changed(filter);
        int CountChanged(out Chunk? last)
        {
            int count = 0;
            last = null;
            foreach (var chunk in changed) { count++; last = chunk; }
            return count;
        }

        Assert.Equal(2, CountChanged(out _), "primeira observação entrega todos os chunks compatíveis");
        Assert.Equal(0, CountChanged(out _), "sem escrita, nenhum chunk reaparece");
        _ = world.Read<Position>(first).Value;
        world.Write<Velocity>(first).Value = float3.Zero;
        Assert.Equal(0, CountChanged(out _), "leitura de Position e escrita em Velocity não disparam Position");

        world.Write<Position>(secondChunkEntity).Value = new float3(9, 9, 9);
        Assert.Equal(1, CountChanged(out var changedChunk));
        Assert.True(changedChunk is not null && changedChunk != firstChunk,
            "somente o segundo chunk, realmente escrito, deve passar pelo filtro");
        Assert.Equal(0, CountChanged(out _));

        int stableCount = -1;
        Assert.NoAlloc(() => stableCount = CountChanged(out _),
            "filtro aquecido sem mudanças não aloca no caminho de frame");
        Assert.Equal(0, stableCount);

        filter.Reset();
        Assert.Equal(2, CountChanged(out _), "Reset torna todos os chunks observáveis novamente");
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
                var pos = chunk.GetWritableSpan<Position>();
                var vel = chunk.GetReadOnlySpan<Velocity>();
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
            Assert.Close((float)i, world.Read<Position>(ids[i]).Value.X, what: $"Position da entidade {i} não pode ter sido embaralhada");
            Assert.Equal(i * 10, world.Read<Health>(ids[i]).Value, what: $"Health da entidade {i} não pode ter sido embaralhado");
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
        Assert.True(world.Read<Parent>(filho).Value == pai, "a referência ao id temporário do pai foi resolvida corretamente no playback");
    }

    [Test] public static void ECB_AddComponentDiferidoEmEntidadeJaExistente()
    {
        var world = new World();
        var e = world.CreateEntity();
        var ecb = new EntityCommandBuffer();
        ecb.AddComponent(e, new Position(new float3(7, 7, 7)));
        ecb.Playback(world);
        Assert.Close(new float3(7, 7, 7), world.Read<Position>(e).Value);
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
        Assert.Close(new float3(10, 0, 2), world.Read<WorldTransform>(filho).Value.Position, what: "filho segue o pai");

        world.Write<LocalTransform>(pai).Value = Transform.FromPosition(new float3(100, 0, 0));
        TransformSystem.Propagate(world);
        Assert.Close(new float3(100, 0, 2), world.Read<WorldTransform>(filho).Value.Position, what: "mover o pai move o filho");
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

        Assert.Close(new float3(1, 0, 0), world.Read<WorldTransform>(avo).Value.Position);
        Assert.Close(new float3(1, 1, 0), world.Read<WorldTransform>(pai).Value.Position);
        Assert.Close(new float3(1, 1, 1), world.Read<WorldTransform>(neto).Value.Position, what: "neto acumula translação de avô e pai");
    }

    [Test] public static void Hierarquia_DestruirPai_NetoNaoTravaEEhTratadoComoRaiz()
    {
        var world = new World();
        var pai = world.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(5, 0, 0))), new WorldTransform(Transform.Identity));
        var filho = world.CreateEntity(new LocalTransform(Transform.FromPosition(new float3(0, 3, 0))), new WorldTransform(Transform.Identity));
        world.AddComponent(filho, new Parent(pai));

        world.DestroyEntity(pai);

        TransformSystem.Propagate(world);   // não pode lançar nem travar
        Assert.Close(new float3(0, 3, 0), world.Read<WorldTransform>(filho).Value.Position,
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
                var pos = chunk.GetWritableSpan<Position>();
                var vel = chunk.GetReadOnlySpan<Velocity>();
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
            var pos = chunk.GetReadOnlySpan<Position>();
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

    [Test] public static void Benchmark_MudancasEstruturaisEmDezMilEntidades_PreservaIdsEDados()
    {
        const int total = 10_000;
        var world = new World();
        var entities = new EntityId[total];
        for (int i = 0; i < total; i++)
            entities[i] = world.CreateEntity(new Position(new float3(i, 0, 0)));

        var add = Stopwatch.StartNew();
        for (int i = 0; i < total; i++) world.AddComponent(entities[i], new Velocity(float3.One));
        add.Stop();
        var remove = Stopwatch.StartNew();
        for (int i = 0; i < total; i++) world.RemoveComponent<Velocity>(entities[i]);
        remove.Stop();

        for (int i = 0; i < total; i++)
        {
            Assert.True(world.Exists(entities[i]), "mudança de arquétipo preserva id+geração");
            Assert.Close((float)i, world.Read<Position>(entities[i]).Value.X,
                what: "componente sobrevivente não pode ser embaralhado");
            Assert.False(world.HasComponent<Velocity>(entities[i]));
        }
        Console.WriteLine(
            $"    [estrutural-10k] add={add.ElapsedMilliseconds} ms, " +
            $"remove={remove.ElapsedMilliseconds} ms, ids/dados preservados");
    }
}
