namespace Aether.Tests;

/// <summary>
/// Hierarquia de cena por lista encadeada de irmãos. Estes testes existem porque a versão
/// anterior deste componente tinha teto de 8 filhos — o que quebraria o painel de Hierarquia
/// do editor no primeiro nível com muitos objetos.
/// </summary>
public static class HierarchyTests
{
    private static World NovoMundo() => new();

    [Test] public static void SetParent_PrendeEDesprende()
    {
        var w = NovoMundo();
        var pai = w.CreateEntity();
        var filho = w.CreateEntity();

        Hierarchy.SetParent(w, filho, pai);
        Assert.True(w.HasComponent<Parent>(filho), "o filho passou a ter pai");
        Assert.Equal(pai, w.GetComponent<Parent>(filho).Value);
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
        Assert.Equal(a, w.GetComponent<FirstChild>(pai).Value, "a cabeça passou a ser o irmão seguinte");
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
        Assert.Equal(pai2, w.GetComponent<Parent>(filho).Value);
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

        Assert.Close(new float3(10f, 0f, 2f), w.GetComponent<WorldTransform>(filho).Value.Position, 1e-4f,
            "a posição do filho no mundo é a do pai composta com a local");
    }
}
