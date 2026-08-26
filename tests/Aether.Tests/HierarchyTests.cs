using System.Diagnostics;

namespace Aether.Tests;

/// <summary>
/// Hierarquia de cena por lista encadeada de irmãos. Estes testes existem porque a versão
/// anterior deste componente tinha teto de 8 filhos — o que quebraria o painel de Hierarquia
/// do editor no primeiro nível com muitos objetos.
/// </summary>
public static class HierarchyTests
{
    private struct PlanMoveMarker
    {
        public int Value;
        public PlanMoveMarker(int value) => Value = value;
    }

    private static World NovoMundo() => new();

    [Test] public static void Node_EhFachadaSemOwnershipDuplicadoESegueAGeracaoDaEntidade()
    {
        var world = NovoMundo();
        var node = world.CreateNode();
        node.Add(new LocalTransform(Transform.Identity));

        node.Write<LocalTransform>().Value = Transform.FromPosition(new float3(3, 4, 5));
        Assert.Close(new float3(3, 4, 5), world.Read<LocalTransform>(node.Id).Value.Position);
        Assert.Equal(1, world.EntityCount, "Node não cria um segundo objeto além da entidade");
        Assert.Equal(node, world.GetNode(node.Id));

        EntityId oldId = node.Id;
        node.Destroy();
        Assert.False(node.IsValid, "todas as cópias da fachada observam a entidade destruída");
        Assert.Throws<InvalidOperationException>(() => { _ = node.Read<LocalTransform>(); });

        var replacement = world.CreateNode();
        Assert.Equal(oldId.Index, replacement.Id.Index, "o slot pode ser reciclado");
        Assert.NotEqual(oldId.Version, replacement.Id.Version, "a geração impede alias com o Node antigo");
        Assert.True(node != replacement);
    }

    [Test] public static void Node_HierarquiaAmigavelMantemComponentesEIteraSemAlocar()
    {
        var world = NovoMundo();
        var parent = world.CreateNode();
        var childA = world.CreateNode();
        var childB = world.CreateNode();
        childA.SetParent(parent);
        childB.SetParent(parent);

        Assert.Equal(parent, childA.Parent);
        int count = 0;
        Assert.NoAlloc(() =>
        {
            count = 0;
            foreach (var child in parent.Children)
            {
                Assert.True(child == childA || child == childB);
                count++;
            }
        }, "Node.Children adapta a lista de EntityId sem alocar");
        Assert.Equal(2, count);

        childA.SetParent(default);
        Assert.False(childA.Parent.IsValid);
        Assert.Equal(1, Hierarchy.ChildCount(world, parent.Id));
    }

    [Test] public static void Node_RecusaParentDeOutroWorld()
    {
        var child = NovoMundo().CreateNode();
        var foreignParent = NovoMundo().CreateNode();
        Assert.Throws<ArgumentException>(() => child.SetParent(foreignParent));
    }

    [Test] public static void SetParent_PrendeEDesprende()
    {
        var w = NovoMundo();
        var pai = w.CreateEntity();
        var filho = w.CreateEntity();

        Hierarchy.SetParent(w, filho, pai);
        Assert.True(w.HasComponent<Parent>(filho), "o filho passou a ter pai");
        Assert.Equal(pai, w.Read<Parent>(filho).Value);
        Assert.Equal(1, Hierarchy.ChildCount(w, pai));

        Hierarchy.SetParent(w, filho, EntityId.Null);
        Assert.False(w.HasComponent<Parent>(filho), "prender ao nulo torna a entidade raiz");
        Assert.Equal(0, Hierarchy.ChildCount(w, pai));
    }

    [Test] public static void Hierarquia_AguentaMuitosFilhos()
    {
        // O caso que motivou a reescrita: um nó "Nível" com centenas de objetos.
        var w = NovoMundo();
        var nivel = w.CreateEntity();
        const int total = 500;

        var criados = new List<EntityId>();
        for (int i = 0; i < total; i++)
        {
            var e = w.CreateEntity();
            Hierarchy.SetParent(w, e, nivel);
            criados.Add(e);
        }

        Assert.Equal(total, Hierarchy.ChildCount(w, nivel), "nenhum teto de filhos");

        var vistos = new HashSet<EntityId>();
        foreach (var c in Hierarchy.EnumerateChildren(w, nivel)) vistos.Add(c);
        Assert.Equal(total, vistos.Count, "cada filho aparece exatamente uma vez na travessia");
        foreach (var e in criados) Assert.True(vistos.Contains(e), "todo filho criado é alcançável");
    }

    [Test] public static void Detach_CosturaAListaNoMeio()
    {
        var w = NovoMundo();
        var pai = w.CreateEntity();
        var a = w.CreateEntity();
        var b = w.CreateEntity();
        var c = w.CreateEntity();
        Hierarchy.SetParent(w, a, pai);
        Hierarchy.SetParent(w, b, pai);
        Hierarchy.SetParent(w, c, pai);

        Hierarchy.Detach(w, b);   // remove do meio da lista

        Assert.Equal(2, Hierarchy.ChildCount(w, pai));
        var restantes = new List<EntityId>();
        foreach (var x in Hierarchy.EnumerateChildren(w, pai)) restantes.Add(x);
        Assert.True(restantes.Contains(a) && restantes.Contains(c), "os irmãos sobreviventes continuam ligados");
        Assert.False(restantes.Contains(b), "o removido saiu da lista");
    }

    [Test] public static void Detach_RemoveOPrimeiroDaLista()
    {
        var w = NovoMundo();
        var pai = w.CreateEntity();
        var a = w.CreateEntity();
        var b = w.CreateEntity();
        Hierarchy.SetParent(w, a, pai);
        Hierarchy.SetParent(w, b, pai);   // b vira a cabeça da lista

        Hierarchy.Detach(w, b);
        Assert.Equal(1, Hierarchy.ChildCount(w, pai));
        Assert.Equal(a, w.Read<FirstChild>(pai).Value, "a cabeça passou a ser o irmão seguinte");
    }

    [Test] public static void SetParent_TrocarDePaiNaoDeixaRestoNoAntigo()
    {
        var w = NovoMundo();
        var pai1 = w.CreateEntity();
        var pai2 = w.CreateEntity();
        var filho = w.CreateEntity();

        Hierarchy.SetParent(w, filho, pai1);
        Hierarchy.SetParent(w, filho, pai2);

        Assert.Equal(0, Hierarchy.ChildCount(w, pai1), "o pai antigo não guarda referência órfã");
        Assert.Equal(1, Hierarchy.ChildCount(w, pai2));
        Assert.Equal(pai2, w.Read<Parent>(filho).Value);
    }

    [Test] public static void SetParent_RecusaCiclo()
    {
        var w = NovoMundo();
        var avo = w.CreateEntity();
        var pai = w.CreateEntity();
        var neto = w.CreateEntity();
        Hierarchy.SetParent(w, pai, avo);
        Hierarchy.SetParent(w, neto, pai);

        // Prender o avô ao neto fecharia um ciclo — e um ciclo aqui travaria a propagação de transform.
        Assert.Throws<ArgumentException>(() => Hierarchy.SetParent(w, avo, neto), "ciclo indireto é recusado");
        Assert.Throws<ArgumentException>(() => Hierarchy.SetParent(w, pai, pai), "ser pai de si mesmo é recusado");
    }

    [Test] public static void SetParent_RecusaProfundidadePatologicaAntesDeAlterarAHierarquia()
    {
        var world = NovoMundo();
        var nodes = new EntityId[Hierarchy.MaxDepth + 2];
        for (int i = 0; i < nodes.Length; i++) nodes[i] = world.CreateEntity();
        for (int i = 1; i <= Hierarchy.MaxDepth; i++)
            Hierarchy.SetParent(world, nodes[i], nodes[i - 1]);

        Assert.Throws<ArgumentException>(() =>
            Hierarchy.SetParent(world, nodes[Hierarchy.MaxDepth + 1], nodes[Hierarchy.MaxDepth]),
            "cadeia acima do limite precisa falhar antes de criar custo não limitado");
        Assert.False(world.HasComponent<Parent>(nodes[Hierarchy.MaxDepth + 1]),
            "falha de validação não pode deixar metade do reparent aplicada");
    }

    [Test] public static void SetParent_RecusaEntidadeDestruida()
    {
        var w = NovoMundo();
        var pai = w.CreateEntity();
        var filho = w.CreateEntity();
        w.DestroyEntity(pai);
        Assert.Throws<ArgumentException>(() => Hierarchy.SetParent(w, filho, pai));
    }

    [Test] public static void EnumerateChildren_DeQuemNaoTemFilhosNaoExplode()
    {
        var w = NovoMundo();
        var solitaria = w.CreateEntity();
        int n = 0;
        foreach (var _ in Hierarchy.EnumerateChildren(w, solitaria)) n++;
        Assert.Equal(0, n);

        var destruida = w.CreateEntity();
        w.DestroyEntity(destruida);
        foreach (var _ in Hierarchy.EnumerateChildren(w, destruida)) n++;
        Assert.Equal(0, n, "iterar filhos de entidade destruída devolve vazio em vez de lançar");
    }

    [Test] public static void EnumerateChildren_NaoAloca()
    {
        var w = NovoMundo();
        var pai = w.CreateEntity();
        for (int i = 0; i < 32; i++) Hierarchy.SetParent(w, w.CreateEntity(), pai);

        int soma = 0;
        Assert.NoAlloc(() =>
        {
            soma = 0;
            foreach (var c in Hierarchy.EnumerateChildren(w, pai)) soma += c.Index;
        }, "travessia de filhos usa enumerador struct");
        Assert.True(soma > 0);
    }

    [Test] public static void Propagacao_AindaFuncionaComAHierarquiaEncadeada()
    {
        var w = NovoMundo();
        var pai = w.CreateEntity();
        w.AddComponent(pai, new LocalTransform(Transform.FromPosition(new float3(10f, 0f, 0f))));
        w.AddComponent(pai, new WorldTransform(Transform.Identity));

        var filho = w.CreateEntity();
        w.AddComponent(filho, new LocalTransform(Transform.FromPosition(new float3(0f, 0f, 2f))));
        w.AddComponent(filho, new WorldTransform(Transform.Identity));
        Hierarchy.SetParent(w, filho, pai);

        TransformSystem.Propagate(w);

        Assert.Close(new float3(10f, 0f, 2f), w.Read<WorldTransform>(filho).Value.Position, 1e-4f,
            "a posição do filho no mundo é a do pai composta com a local");
    }

    [Test] public static void Propagacao_RecompilaPlanoAposReparentEMigracaoDeArquetipo()
    {
        var world = NovoMundo();
        var rootA = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(10, 0, 0))),
            new WorldTransform(Transform.Identity));
        var rootB = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(20, 0, 0))),
            new WorldTransform(Transform.Identity));
        var child = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(1, 0, 0))),
            new WorldTransform(Transform.Identity));

        Hierarchy.SetParent(world, child, rootA);
        TransformSystem.Propagate(world);
        Assert.Close(11f, world.Read<WorldTransform>(child).Value.Position.X);

        Hierarchy.SetParent(world, child, rootB);
        world.AddComponent(child, new PlanMoveMarker(7)); // move de chunk/arquétipo após o plano existir
        TransformSystem.Propagate(world);

        Assert.Close(21f, world.Read<WorldTransform>(child).Value.Position.X,
            what: "reparent e migração invalidam localizações compiladas sem deixar referência velha");
        Assert.Equal(7, world.Read<PlanMoveMarker>(child).Value);
    }

    [Test] public static void Propagacao_RecompilaPlanoAposSwapBackPorDestruicao()
    {
        var world = NovoMundo();
        var root = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(10, 0, 0))),
            new WorldTransform(Transform.Identity));
        var removed = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(1, 0, 0))),
            new WorldTransform(Transform.Identity));
        var survivor = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(2, 0, 0))),
            new WorldTransform(Transform.Identity));
        Hierarchy.SetParent(world, removed, root);
        Hierarchy.SetParent(world, survivor, root);
        TransformSystem.Propagate(world);

        Hierarchy.Detach(world, removed);
        world.DestroyEntity(removed); // pode mover survivor para a linha liberada via swap-back
        world.Write<LocalTransform>(root).Value = Transform.FromPosition(new float3(30, 0, 0));
        TransformSystem.Propagate(world);

        Assert.Close(32f, world.Read<WorldTransform>(survivor).Value.Position.X,
            what: "destruição invalida linhas físicas compiladas");
    }

    [Test] public static void Propagacao_ParentLegadoDiretoInvalidaPlanoSemListaDeIrmaos()
    {
        var world = NovoMundo();
        var rootA = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(10, 0, 0))),
            new WorldTransform(Transform.Identity));
        var rootB = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(20, 0, 0))),
            new WorldTransform(Transform.Identity));
        var child = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(1, 0, 0))),
            new WorldTransform(Transform.Identity));

        world.AddComponent(child, new Parent(rootA)); // dado legado: sem FirstChild/NextSibling
        TransformSystem.Propagate(world);
        Assert.Close(11f, world.Read<WorldTransform>(child).Value.Position.X);

        world.SetComponent(child, new Parent(rootB)); // não move arquétipo; depende de HierarchyVersion
        TransformSystem.Propagate(world);
        Assert.Close(21f, world.Read<WorldTransform>(child).Value.Position.X,
            what: "alterar Parent existente recompila a topologia mesmo sem mudança estrutural");
    }

    [Test] public static void Propagacao_MarcaWorldTransformUmaVezPorChunk()
    {
        var world = NovoMundo();
        for (int i = 0; i < 1_000; i++)
            world.CreateEntity(
                new LocalTransform(Transform.FromPosition(new float3(i, 0, 0))),
                new WorldTransform(Transform.Identity));

        TransformSystem.Propagate(world); // compila e aquece
        var chunks = new List<(Chunk Chunk, int Before)>();
        foreach (var chunk in world.Query<WorldTransform>())
            chunks.Add((chunk, chunk.GetChangeVersion<WorldTransform>()));

        TransformSystem.Propagate(world);
        foreach (var (chunk, before) in chunks)
            Assert.Equal(before + 1, chunk.GetChangeVersion<WorldTransform>(),
                "a escrita batched avança dirty version uma vez por chunk, não por entidade");
    }

    [Test] public static void Propagacao_RecompilaAposParentAlteradoPorSpanDeChunk()
    {
        var world = NovoMundo();
        EntityId rootA = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(10, 0, 0))),
            new WorldTransform(Transform.Identity));
        EntityId rootB = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(20, 0, 0))),
            new WorldTransform(Transform.Identity));
        EntityId child = world.CreateEntity(
            new LocalTransform(Transform.FromPosition(new float3(1, 0, 0))),
            new WorldTransform(Transform.Identity));
        Hierarchy.SetParent(world, child, rootA);
        TransformSystem.Propagate(world);
        Assert.Close(11, world.Read<WorldTransform>(child).Value.Position.X);

        foreach (var chunk in world.Query<Parent>())
        {
            var entities = chunk.Entities;
            var parents = chunk.GetWritableSpan<Parent>();
            for (int row = 0; row < entities.Length; row++)
                if (entities[row] == child) parents[row] = new Parent(rootB);
        }

        TransformSystem.Propagate(world);
        Assert.Close(21, world.Read<WorldTransform>(child).Value.Position.X,
            what: "dirty version da coluna Parent também invalida o plano compilado");
    }


    [Test] public static void Benchmark_CemMilTransformsEmHierarquiaRealista_SemGcEComPercentis()
    {
        const int total = 100_000;
        const int branchingFactor = 8;
        const int samples = 60;
        var world = NovoMundo();
        var nodes = new EntityId[total];

        var setup = Stopwatch.StartNew();
        for (int i = 0; i < total; i++)
            nodes[i] = world.CreateEntity(
                new LocalTransform(Transform.FromPosition(new float3(1, 0, 0))),
                new WorldTransform(Transform.Identity));
        for (int i = 1; i < total; i++)
            Hierarchy.SetParent(world, nodes[i], nodes[(i - 1) / branchingFactor]);
        setup.Stop();

        TransformSystem.Propagate(world); // compila o plano topológico e aquece caches
        for (int i = 0; i < 16; i++)
        {
            AnimateLocals(world, i * 0.001f);
            TransformSystem.Propagate(world);
        }
        int expectedDepth = 1;
        for (int cursor = total - 1; cursor > 0; cursor = (cursor - 1) / branchingFactor)
            expectedDepth++;
        Assert.Close((float)expectedDepth, world.Read<WorldTransform>(nodes[^1]).Value.Position.X,
            what: "folha acumula a transform de todos os ancestrais");

        Assert.NoAlloc(() =>
        {
            AnimateLocals(world, 0.25f);
            TransformSystem.Propagate(world);
        }, "animação + propagação hierárquica de 100 mil entidades não aloca por frame");

        var elapsedTicks = new long[samples];
        var animationTicks = new long[samples];
        var propagationTicks = new long[samples];
        for (int i = 0; i < samples; i++)
        {
            long start = Stopwatch.GetTimestamp();
            AnimateLocals(world, i * 0.001f);
            long afterAnimation = Stopwatch.GetTimestamp();
            TransformSystem.Propagate(world);
            long end = Stopwatch.GetTimestamp();
            animationTicks[i] = afterAnimation - start;
            propagationTicks[i] = end - afterAnimation;
            elapsedTicks[i] = end - start;
        }
        Array.Sort(elapsedTicks);
        Array.Sort(animationTicks);
        Array.Sort(propagationTicks);
        static double Milliseconds(long ticks) => ticks * 1000.0 / Stopwatch.Frequency;
        double p50 = Milliseconds(elapsedTicks[(samples - 1) * 50 / 100]);
        double p95 = Milliseconds(elapsedTicks[(samples - 1) * 95 / 100]);
        double p99 = Milliseconds(elapsedTicks[(samples - 1) * 99 / 100]);
        double animationP50 = Milliseconds(animationTicks[(samples - 1) * 50 / 100]);
        double propagationP50 = Milliseconds(propagationTicks[(samples - 1) * 50 / 100]);
        Console.WriteLine(
            $"    [hierarquia-100k] backend={TransformSystem.ActiveBackend}, " +
            $"setup={setup.ElapsedMilliseconds} ms, depth={expectedDepth}, " +
            $"animation-p50={animationP50:F2} ms, propagation-p50={propagationP50:F2} ms, " +
            $"total-p50={p50:F2} ms, p95={p95:F2} ms, p99={p99:F2} ms, GC/frame=0");

        Assert.Close(expectedDepth * ((samples - 1) * 0.001f),
            world.Read<WorldTransform>(nodes[^1]).Value.Position.Y, 1e-4f,
            "a animação simples realmente alimenta a propagação medida");

        // O limite de < 6 ms pertence ao aparelho classe A, não ao host de desenvolvimento/CI.
        Assert.True(p50 > 0 && p95 >= p50 && p99 >= p95, "percentis precisam ser válidos e ordenados");
    }

    private static void AnimateLocals(World world, float y)
    {
        foreach (var chunk in world.Query<LocalTransform>())
        {
            var local = chunk.GetWritableSpan<LocalTransform>();
            for (int i = 0; i < local.Length; i++) local[i].Value.Position.Y = y;
        }
    }
}
