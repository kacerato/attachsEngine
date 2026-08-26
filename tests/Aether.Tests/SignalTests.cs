namespace Aether.Tests;

/// <summary>Testes do item 1.5.3: eventos e sinais tipados.</summary>
public static class SignalTests
{
    [Test] public static void Publish_SemAssinantes_NaoLancaNemFazNada()
    {
        var signal = new Signal<int>();
        signal.Publish(42); // não deveria lançar
        Assert.Equal(0, signal.SubscriberCount);
    }

    [Test] public static void Subscribe_UmAssinante_RecebeOValorPublicado()
    {
        var signal = new Signal<int>();
        int received = -1;
        signal.Subscribe(v => received = v);
        signal.Publish(7);
        Assert.Equal(7, received, "o assinante deveria receber exatamente o valor publicado");
    }

    [Test] public static void Publish_VariosAssinantes_TodosRecebemNaOrdemDeInscricao()
    {
        var signal = new Signal<int>();
        var order = new System.Collections.Generic.List<int>();
        signal.Subscribe(v => order.Add(v * 10 + 1));
        signal.Subscribe(v => order.Add(v * 10 + 2));
        signal.Subscribe(v => order.Add(v * 10 + 3));
        signal.Publish(5);
        Assert.Equal(3, order.Count, "todos os três assinantes deveriam ter sido chamados");
        Assert.Equal(51, order[0]);
        Assert.Equal(52, order[1]);
        Assert.Equal(53, order[2]);
    }

    [Test] public static void Unsubscribe_RemoveAssinanteQueParaDeReceber()
    {
        var signal = new Signal<int>();
        int callCount = 0;
        var subscription = signal.Subscribe(_ => callCount++);
        signal.Publish(1);
        signal.Unsubscribe(subscription);
        signal.Publish(2);
        Assert.Equal(1, callCount, "após Unsubscribe, o assinante não deveria mais ser chamado");
    }

    [Test] public static void Unsubscribe_DuasVezes_EhIdempotente()
    {
        var signal = new Signal<int>();
        var subscription = signal.Subscribe(_ => { });
        signal.Unsubscribe(subscription);
        signal.Unsubscribe(subscription); // não deveria lançar nem corromper estado
        Assert.Equal(0, signal.SubscriberCount);
    }

    [Test] public static void Unsubscribe_ComAlcaInvalida_EhNoOp()
    {
        var signal = new Signal<int>();
        signal.Subscribe(_ => { });
        signal.Unsubscribe(SignalSubscription.Invalid);
        Assert.Equal(1, signal.SubscriberCount, "Unsubscribe com alça inválida não deveria afetar assinantes reais");
    }

    [Test] public static void Unsubscribe_ComGeracaoAntiga_NaoRemoveOAssinanteNovoDoSlotReciclado()
    {
        // Cenário central do handle geracional: A se inscreve, sai; B se inscreve depois e
        // recicla o MESMO slot (índice). A alça antiga de A não pode remover B.
        var signal = new Signal<int>();
        var subscriptionA = signal.Subscribe(_ => { });
        signal.Unsubscribe(subscriptionA);
        int bCallCount = 0;
        var subscriptionB = signal.Subscribe(_ => bCallCount++);

        signal.Unsubscribe(subscriptionA); // alça antiga, geração desatualizada

        signal.Publish(1);
        Assert.Equal(1, bCallCount, "a alça antiga de A não deveria remover o assinante B que reciclou o slot");
        Assert.Equal(1, signal.SubscriberCount);

        signal.Unsubscribe(subscriptionB);
        Assert.Equal(0, signal.SubscriberCount);
    }

    [Test] public static void Subscribe_ReusaSlotLivreAntesDeCrescerOArray()
    {
        var signal = new Signal<int>(initialCapacity: 2);
        var s1 = signal.Subscribe(_ => { });
        var s2 = signal.Subscribe(_ => { });
        signal.Unsubscribe(s1);
        // Reinscrever não deveria precisar crescer o array — o slot de s1 está livre.
        int callCount = 0;
        signal.Subscribe(v => callCount++);
        signal.Publish(1);
        Assert.Equal(1, callCount, "o novo assinante deveria ter reusado o slot livre e ser notificado normalmente");
        Assert.Equal(2, signal.SubscriberCount);
    }

    [Test] public static void Subscribe_MuitosAssinantes_CresceOArrayETodosContinuamFuncionando()
    {
        var signal = new Signal<int>(initialCapacity: 1);
        const int total = 50;
        int callCount = 0;
        for (int i = 0; i < total; i++) signal.Subscribe(_ => Interlocked.Increment(ref callCount));
        signal.Publish(1);
        Assert.Equal(total, callCount, "todos os assinantes deveriam sobreviver ao crescimento do array de slots");
    }

    [Test] public static void Unsubscribe_DuranteAPropriaPublicacao_NaoCorrompeAVarredura()
    {
        // Um assinante que se desinscreve a si mesmo durante Publish é um padrão legítimo
        // ("ouça só uma vez") — a checagem de geração garante que isso não corrompa os slots
        // seguintes nem cause null-ref na varredura.
        var signal = new Signal<int>();
        SignalSubscription selfSubscription = default;
        int selfCallCount = 0;
        selfSubscription = signal.Subscribe(_ =>
        {
            selfCallCount++;
            signal.Unsubscribe(selfSubscription);
        });
        int otherCallCount = 0;
        signal.Subscribe(_ => otherCallCount++);

        signal.Publish(1);
        Assert.Equal(1, selfCallCount, "o assinante que se remove sozinho deveria ter sido chamado exatamente uma vez");
        Assert.Equal(1, otherCallCount, "o outro assinante não deveria ser afetado pela auto-remoção");

        signal.Publish(2);
        Assert.Equal(1, selfCallCount, "após se desinscrever, o assinante não deveria ser chamado de novo");
        Assert.Equal(2, otherCallCount);
    }

    [Test] public static void Subscribe_ListenerNulo_Lanca()
    {
        var signal = new Signal<int>();
        Assert.Throws<ArgumentNullException>(() => signal.Subscribe(null!), "assinar com listener nulo não é um caso válido");
    }

    [Test] public static void Publish_ComPayloadStruct_PropagaValorPorCopia()
    {
        var signal = new Signal<TestEvent>();
        TestEvent received = default;
        signal.Subscribe(e => received = e);
        signal.Publish(new TestEvent(3, 4));
        Assert.Equal(3, received.A);
        Assert.Equal(4, received.B);
    }

    private readonly struct TestEvent(int a, int b)
    {
        public readonly int A = a;
        public readonly int B = b;
    }

    [Test] public static void Publish_AposTodosDesinscritos_NaoChamaNinguem()
    {
        var signal = new Signal<int>();
        var s1 = signal.Subscribe(_ => throw new InvalidOperationException("não deveria ser chamado"));
        var s2 = signal.Subscribe(_ => throw new InvalidOperationException("não deveria ser chamado"));
        signal.Unsubscribe(s1);
        signal.Unsubscribe(s2);
        signal.Publish(1); // não deveria lançar
        Assert.Equal(0, signal.SubscriberCount);
    }
}
