namespace Aether.Tests;

/// <summary>Testes do item 1.5.2: fixed step, interpolação, escala de tempo e pausa.</summary>
public static class TimeTests
{
    [Test] public static void Advance_ExatamenteUmFixedDelta_ProduzUmPasso()
    {
        var clock = new FixedClock(1f / 60f);
        Assert.Equal(1, clock.Advance(1f / 60f), "um frame do tamanho exato do fixed step produz exatamente um passo");
    }

    [Test] public static void Advance_MetadeDoFixedDelta_NaoProduzPassoAindaEAcumulaAlpha()
    {
        var clock = new FixedClock(1f / 60f);
        Assert.Equal(0, clock.Advance(1f / 120f), "meio fixed step não é suficiente para completar um passo");
        Assert.Close(0.5f, clock.InterpolationAlpha, eps: 1e-4f, what: "meio fixed step acumulado deveria refletir 50% de progresso até o próximo passo");
    }

    [Test] public static void Advance_DoisFixedDeltasEMeio_ProduzDoisPassosEMantemResto()
    {
        var clock = new FixedClock(1f / 60f);
        int steps = clock.Advance(2.5f / 60f);
        Assert.Equal(2, steps, "2,5 fixed steps deveriam produzir 2 passos completos");
        Assert.Close(0.5f, clock.InterpolationAlpha, eps: 1e-4f, what: "o meio passo restante deveria sobrar no acumulador como alpha de interpolação");
    }

    [Test] public static void Advance_AcumulaEntreChamadas_NaoPerdeTempoFracionado()
    {
        var clock = new FixedClock(1f / 60f);
        int total = 0;
        // 180 frames de 1/144s (taxa de tela mais rápida que o fixed step) devem produzir
        // aproximadamente 180/144*60 = 75 passos no total, sem perder nem duplicar tempo por
        // causa de arredondamento entre chamadas sucessivas.
        for (int i = 0; i < 180; i++) total += clock.Advance(1f / 144f);
        Assert.True(total is >= 74 and <= 76, $"180 frames a 144 Hz deveriam produzir ~75 passos a 60 Hz, produziu {total}");
    }

    [Test] public static void Advance_UmUnicoFrameGigante_RespeitaMaxStepsENaoTravaEmEspiral()
    {
        // Simula uma pausa de 5 segundos inteiros (troca de cena, breakpoint) num clock de fixed
        // step de 60 Hz — sem o teto, isso pediria 300 passos de uma vez, e cada passo real gasto
        // simulando alimentaria um "atraso" ainda maior no próximo frame: a espiral da morte.
        var clock = new FixedClock(1f / 60f, maxStepsPerAdvance: 5);
        int steps = clock.Advance(5f);
        Assert.Equal(5, steps, "um frame gigante nunca deveria produzir mais passos que o teto configurado");
        Assert.True(clock.DroppedSteps > 0, "tempo além do teto deveria ser contabilizado como descartado, não silenciosamente absorvido");
    }

    [Test] public static void Advance_ChamadasRepetidasAposEspiral_NaoAcumulaDividaInfinita()
    {
        // Depois de um frame gigante descartar o excesso, o clock precisa voltar a se comportar
        // normalmente nos frames seguintes — não pode ficar "devendo" passos indefinidamente.
        var clock = new FixedClock(1f / 60f, maxStepsPerAdvance: 5);
        clock.Advance(5f); // dispara o teto e descarta o excedente
        int steps = clock.Advance(1f / 60f);
        Assert.Equal(1, steps, "depois do descarte do frame gigante, um frame normal volta a produzir um passo normal");
    }

    [Test] public static void TimeScale_Zero_NaoAcumulaTempo()
    {
        var clock = new FixedClock(1f / 60f) { TimeScale = 0f };
        for (int i = 0; i < 600; i++) clock.Advance(1f / 60f);
        Assert.Close(0f, clock.InterpolationAlpha, eps: 1e-4f, what: "com escala zero, nenhum tempo deveria se acumular mesmo após muitos frames");
    }

    [Test] public static void TimeScale_Dobro_ProduzODobroDePassos()
    {
        var clock = new FixedClock(1f / 60f) { TimeScale = 2f };
        int steps = clock.Advance(1f / 60f);
        Assert.Equal(2, steps, "escala 2x deveria dobrar quantos passos um mesmo delta real produz");
    }

    [Test] public static void TimeScale_Negativa_EhTratadaComoZero()
    {
        // O acumulador não modela tempo andando para trás — uma escala negativa por engano do
        // chamador deveria congelar a simulação, não corromper o acumulador com valor negativo.
        var clock = new FixedClock(1f / 60f) { TimeScale = -1f };
        int steps = clock.Advance(1f);
        Assert.Equal(0, steps, "escala negativa não deveria produzir passo algum nem acumular tempo negativo");
    }

    [Test] public static void Paused_IgnoraAdvanceComPletamente()
    {
        var clock = new FixedClock(1f / 60f) { Paused = true };
        int steps = clock.Advance(10f);
        Assert.Equal(0, steps, "clock pausado não deveria produzir passo algum mesmo com delta real grande");
        Assert.Equal(0, clock.DroppedSteps, "pausa não é o mesmo que estourar o teto de passos — não deveria contar como descarte");
    }

    [Test] public static void Paused_DistinguivelDeTimeScaleZero_RetomaSemPerderConfiguracao()
    {
        var clock = new FixedClock(1f / 60f) { TimeScale = 0.5f, Paused = true };
        clock.Advance(1f);
        Assert.Equal(0.5f, clock.TimeScale, "pausar não deveria alterar a escala de tempo configurada");
        clock.Paused = false;
        int steps = clock.Advance(1f / 30f); // 1/30s a escala 0.5 = 1/60s reais de simulação
        Assert.Equal(1, steps, "ao retomar, a escala configurada antes da pausa deveria continuar valendo");
    }

    [Test] public static void ResetAccumulator_ZeraProgressoSemAlterarContadores()
    {
        var clock = new FixedClock(1f / 60f, maxStepsPerAdvance: 2);
        clock.Advance(1f); // dispara o teto, incrementa DroppedSteps
        int droppedBefore = clock.DroppedSteps;
        clock.Advance(1f / 120f); // meio passo no acumulador
        clock.ResetAccumulator();
        Assert.Close(0f, clock.InterpolationAlpha, eps: 1e-4f, what: "ResetAccumulator deveria zerar o progresso fracionado");
        Assert.Equal(droppedBefore, clock.DroppedSteps, "ResetAccumulator não deveria mexer em contadores de diagnóstico");
    }

    [Test] public static void InterpolationAlpha_AtualizaMesmoQuandoZeroPassosRodam()
    {
        var clock = new FixedClock(1f / 60f);
        clock.Advance(1f / 240f); // um quarto do fixed step, nenhum passo completo
        Assert.Close(0.25f, clock.InterpolationAlpha, eps: 1e-4f, what: "alpha deveria refletir progresso parcial mesmo sem produzir passo algum");
    }

    [Test] public static void Advance_MilharesDeFramesPequenos_NaoSofreDriftDePrecisao()
    {
        // Precision drift: se o acumulador usasse subtração ingênua sem cuidado, milhares de
        // frames de delta não-múltiplo do fixed step (ex.: 1/59.94s, taxa real de muitos
        // displays) acumulariam erro de ponto flutuante suficiente para desviar visivelmente do
        // total de tempo real decorrido. Aqui validamos que a soma de passos produzidos após
        // 10.000 frames bate com o esperado dentro de margem de meio passo — não diverge sem
        // limite com o tempo.
        var clock = new FixedClock(1f / 60f, maxStepsPerAdvance: 1000);
        const float frameDelta = 1f / 59.94f;
        const int frameCount = 10_000;
        int total = 0;
        for (int i = 0; i < frameCount; i++) total += clock.Advance(frameDelta);

        float expectedRealTime = frameDelta * frameCount;
        float expectedSteps = expectedRealTime / (1f / 60f);
        Assert.True(MathF.Abs(total - expectedSteps) < 1f,
            $"após {frameCount} frames, {total} passos deveriam estar a menos de 1 passo de {expectedSteps:F1} esperados (sem drift acumulado)");
    }

    [Test] public static void Constructor_FixedDeltaTimeInvalido_Lanca()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new FixedClock(0f), "fixed delta time zero não é válido");
        Assert.Throws<ArgumentOutOfRangeException>(() => new FixedClock(-1f / 60f), "fixed delta time negativo não é válido");
        Assert.Throws<ArgumentOutOfRangeException>(() => new FixedClock(float.NaN), "fixed delta time NaN não é válido");
        Assert.Throws<ArgumentOutOfRangeException>(() => new FixedClock(float.PositiveInfinity), "fixed delta time infinito não é válido");
    }

    [Test] public static void Constructor_MaxStepsInvalido_Lanca()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new FixedClock(1f / 60f, maxStepsPerAdvance: 0),
            "é preciso permitir ao menos 1 passo por Advance");
    }

    [Test] public static void Advance_DeltaRealInvalido_Lanca()
    {
        var clock = new FixedClock(1f / 60f);
        Assert.Throws<ArgumentOutOfRangeException>(() => clock.Advance(-0.1f), "delta real negativo não é válido");
        Assert.Throws<ArgumentOutOfRangeException>(() => clock.Advance(float.NaN), "delta real NaN não é válido");
    }

    [Test] public static void Advance_DeltaZero_ProduzZeroPassosSemLancar()
    {
        var clock = new FixedClock(1f / 60f);
        Assert.Equal(0, clock.Advance(0f), "delta real zero é um caso válido (frame instantâneo), não deveria lançar nem produzir passo");
    }

    [Test] public static void Advance_NaoAloca()
    {
        var clock = new FixedClock(1f / 60f);
        clock.Advance(1f / 120f); // aquece
        Assert.NoAlloc(() => clock.Advance(1f / 60f), "Advance é caminho de frame — não pode alocar");
    }
}
