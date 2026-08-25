using Aether.Power;

namespace Aether.Tests;

public static class PowerTests
{
    private sealed class FakeThermalSource : IThermalSource
    {
        public ThermalState State;
        public ThermalState Sample() => State;
    }

    [Test] public static void CadaEstadoTermico_ProduzOPerfilDaTabela()
    {
        var fonte = new FakeThermalSource { State = ThermalState.Nominal };
        var gov = new PowerGovernor(fonte);
        var nominal = gov.Update(batteryLevel: 1f, isCharging: true);
        Assert.Equal(60, nominal.FrameRateCap);
        Assert.Close(1.0f, nominal.ResolutionScale);
        Assert.True(nominal.EnableScreenSpaceReflections);
        Assert.Equal(ViewportRedrawMode.Continuous, nominal.ViewportRedrawMode);

        // Cada teste usa um governor novo: piorar é imediato (sem histerese),
        // então dá pra ler cada estado isoladamente sem esperar transições.
        AssertEstado(ThermalState.Fair,     frameCap: 60, ssr: true,  redraw: ViewportRedrawMode.Continuous);
        AssertEstado(ThermalState.Moderate, frameCap: 45, ssr: false, redraw: ViewportRedrawMode.Continuous);
        AssertEstado(ThermalState.Severe,   frameCap: 30, ssr: false, redraw: ViewportRedrawMode.OnDemand);
        AssertEstado(ThermalState.Critical, frameCap: 15, ssr: false, redraw: ViewportRedrawMode.Static);
    }

    private static void AssertEstado(ThermalState state, int frameCap, bool ssr, ViewportRedrawMode redraw)
    {
        var fonte = new FakeThermalSource { State = state };
        var gov = new PowerGovernor(fonte);
        var perfil = gov.Update(batteryLevel: 1f, isCharging: true);
        Assert.Equal(frameCap, perfil.FrameRateCap, $"{state}: teto de fps da tabela");
        Assert.Equal(ssr, perfil.EnableScreenSpaceReflections, $"{state}: SSR da tabela");
        Assert.Equal(redraw, perfil.ViewportRedrawMode, $"{state}: modo de redraw da tabela");
    }

    [Test] public static void PiorarEhImediato_MasMelhorarExigeHisterese()
    {
        var fonte = new FakeThermalSource { State = ThermalState.Nominal };
        var gov = new PowerGovernor(fonte);
        gov.Update(1f, true);

        fonte.State = ThermalState.Severe;
        var apiosPiora = gov.Update(1f, true);
        Assert.Equal(30, apiosPiora.FrameRateCap, "piora térmica precisa refletir na primeira leitura, sem esperar");

        fonte.State = ThermalState.Nominal;
        var logoDepois = gov.Update(1f, true);
        Assert.Equal(30, logoDepois.FrameRateCap, "uma única leitura melhor não deve subir a qualidade ainda (histerese)");
    }

    [Test] public static void OscilarPertoDoLimiar_NaoOscilaASaida()
    {
        var fonte = new FakeThermalSource { State = ThermalState.Moderate };
        var gov = new PowerGovernor(fonte);
        gov.Update(1f, true); // fixa em Moderate

        // Sensor tremulando entre Moderate e Fair: nunca chega a se sustentar o
        // suficiente pra contar como melhora real.
        for (int i = 0; i < 20; i++)
        {
            fonte.State = i % 2 == 0 ? ThermalState.Fair : ThermalState.Moderate;
            var perfil = gov.Update(1f, true);
            Assert.Equal(45, perfil.FrameRateCap, $"iteração {i}: oscilação perto do limiar não deveria mudar o perfil de saída");
        }
    }

    [Test] public static void MelhoraSustentada_EventualmenteSobeDeQualidade()
    {
        var fonte = new FakeThermalSource { State = ThermalState.Severe };
        var gov = new PowerGovernor(fonte);
        gov.Update(1f, true);

        fonte.State = ThermalState.Nominal;
        PowerProfile ultimo = default;
        for (int i = 0; i < 10; i++) ultimo = gov.Update(1f, true);

        Assert.Equal(60, ultimo.FrameRateCap, "depois de várias leituras boas seguidas, a qualidade sobe de verdade");
    }

    [Test] public static void BateriaBaixaSemCarregador_ReduzOTetoDeFps()
    {
        var fonte = new FakeThermalSource { State = ThermalState.Nominal };
        var gov = new PowerGovernor(fonte);

        var comCarregador = gov.Update(batteryLevel: 0.05f, isCharging: true);
        Assert.Equal(60, comCarregador.FrameRateCap, "carregando, a bateria baixa não precisa restringir o fps");

        var semCarregador = gov.Update(batteryLevel: 0.05f, isCharging: false);
        Assert.True(semCarregador.FrameRateCap <= 15, "bateria crítica sem carregador restringe bastante o teto de fps");

        var bateriaMedia = gov.Update(batteryLevel: 0.5f, isCharging: false);
        Assert.Equal(60, bateriaMedia.FrameRateCap, "bateria confortável não deveria restringir nada");
    }

    [Test] public static void NullThermalSource_SempreDevolveNominal()
    {
        var fonte = new NullThermalSource();
        Assert.Equal(ThermalState.Nominal, fonte.Sample());
    }
}
