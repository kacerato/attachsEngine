using Aether.Platform;

namespace Aether.Tests;

/// <summary>
/// Item 1.1.4 do plano: janela/display portátil (taxa de atualização, notch/safe area, multi-janela
/// via displays externos). A captação real de Android (<c>Display</c>/<c>DisplayCutout</c>) e iOS
/// (<c>UIScreen</c>) é integração futura — este tipo é o destino testável dessa tradução, mesma
/// disciplina de 1.1.1/1.1.2.
/// </summary>
public static class WindowStateTests
{
    private static DisplayInfo NovoDisplayPrincipal(int id = 0, int width = 2400, int height = 1080, float density = 3f, float refreshHz = 120f, SafeAreaInsets? safeArea = null)
        => new(id, width, height, density, refreshHz, safeArea ?? SafeAreaInsets.Zero, isBuiltIn: true);

    private static DisplayInfo NovoDisplayExterno(int id, int width = 1920, int height = 1080, float density = 1f, float refreshHz = 60f)
        => new(id, width, height, density, refreshHz, SafeAreaInsets.Zero, isBuiltIn: false);

    // ---------- DisplayInfo: validação de construtor ----------

    [Test] public static void Constructor_LarguraNegativa_Lanca()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new DisplayInfo(0, -1, 1080, 3f, 60f, SafeAreaInsets.Zero, true),
            what: "largura negativa é uma configuração de display sem sentido físico");
    }

    [Test] public static void Constructor_AlturaNegativa_Lanca()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new DisplayInfo(0, 1920, -1, 3f, 60f, SafeAreaInsets.Zero, true),
            what: "altura negativa é uma configuração de display sem sentido físico");
    }

    [Test] public static void Constructor_DensidadeZeroOuNegativa_Lanca()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new DisplayInfo(0, 1920, 1080, 0f, 60f, SafeAreaInsets.Zero, true),
            what: "densidade zero tornaria qualquer conversão dp->pixel indefinida (divisão por zero)");
    }

    [Test] public static void Constructor_TaxaDeAtualizacaoNegativa_Lanca()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new DisplayInfo(0, 1920, 1080, 3f, -60f, SafeAreaInsets.Zero, true),
            what: "taxa de atualização negativa não existe fisicamente");
    }

    [Test] public static void Constructor_TaxaDeAtualizacaoZero_EhAceita()
    {
        var display = new DisplayInfo(0, 1920, 1080, 3f, 0f, SafeAreaInsets.Zero, true);
        Assert.Close(0f, display.RefreshRateHz, what: "zero é um valor válido para 'taxa desconhecida ainda', diferente de negativo");
    }

    // ---------- DisplayInfo: SafeAreaSize ----------

    [Test] public static void SafeAreaSize_SemInsets_DevolveDimensoesCompletas()
    {
        var display = NovoDisplayPrincipal(width: 2400, height: 1080);
        var (largura, altura) = display.SafeAreaSize();
        Assert.Close(2400f, largura, what: "sem safe area, a área útil é a tela inteira");
        Assert.Close(1080f, altura, what: "sem safe area, a área útil é a tela inteira");
    }

    [Test] public static void SafeAreaSize_ComInsetsLaterais_DescontaDosDoisLados()
    {
        var display = NovoDisplayPrincipal(width: 2400, height: 1080, safeArea: new SafeAreaInsets(left: 60, top: 0, right: 40, bottom: 0));
        var (largura, _) = display.SafeAreaSize();
        Assert.Close(2300f, largura, what: "2400 - 60 (notch esquerdo) - 40 (barra de gestos direita) = 2300");
    }

    [Test] public static void SafeAreaSize_InsetsExcedendoADimensao_SaturaEmZeroSemFicarNegativo()
    {
        var display = NovoDisplayPrincipal(width: 100, height: 100, safeArea: new SafeAreaInsets(left: 80, top: 0, right: 80, bottom: 0));
        var (largura, _) = display.SafeAreaSize();
        Assert.Close(0f, largura, what: "insets absurdos (bug de plataforma) não devem produzir largura negativa");
    }

    [Test] public static void SafeAreaInsets_Zero_EhIsZeroVerdadeiro()
    {
        Assert.True(SafeAreaInsets.Zero.IsZero, what: "SafeAreaInsets.Zero deve ser reconhecido como zero por IsZero");
    }

    [Test] public static void SafeAreaInsets_ComQualquerLadoNaoZero_IsZeroEhFalso()
    {
        var insets = new SafeAreaInsets(left: 1, top: 0, right: 0, bottom: 0);
        Assert.False(insets.IsZero, what: "um único lado não-zero já invalida IsZero");
    }

    // ---------- WindowState: construção e display principal ----------

    [Test] public static void Constructor_ComDisplayNaoBuiltIn_Lanca()
    {
        Assert.Throws<ArgumentException>(() => new WindowState(NovoDisplayExterno(id: 0)),
            what: "o display principal deve sempre ser o embutido do aparelho, nunca um externo");
    }

    [Test] public static void Primary_ApósConstrucao_DevolveODisplayPassado()
    {
        var principal = NovoDisplayPrincipal(refreshHz: 90f);
        var window = new WindowState(principal);
        Assert.Close(90f, window.Primary.RefreshRateHz, what: "display principal deve ser exatamente o passado no construtor");
    }

    [Test] public static void SetPrimaryDisplay_MesmoId_AtualizaOsCampos()
    {
        var window = new WindowState(NovoDisplayPrincipal(id: 0, refreshHz: 60f));
        window.SetPrimaryDisplay(NovoDisplayPrincipal(id: 0, refreshHz: 120f));
        Assert.Close(120f, window.Primary.RefreshRateHz, what: "taxa adaptativa mudando deve refletir na consulta seguinte");
    }

    [Test] public static void SetPrimaryDisplay_IdDiferente_Lanca()
    {
        var window = new WindowState(NovoDisplayPrincipal(id: 0));
        Assert.Throws<ArgumentException>(() => window.SetPrimaryDisplay(NovoDisplayPrincipal(id: 1)),
            what: "o display embutido de um aparelho físico não troca de identidade em tempo de execução");
    }

    [Test] public static void SetPrimaryDisplay_NaoBuiltIn_Lanca()
    {
        var window = new WindowState(NovoDisplayPrincipal(id: 0));
        var externoComMesmoId = new DisplayInfo(0, 1920, 1080, 1f, 60f, SafeAreaInsets.Zero, isBuiltIn: false);
        Assert.Throws<ArgumentException>(() => window.SetPrimaryDisplay(externoComMesmoId),
            what: "SetPrimaryDisplay não deve aceitar um display marcado como não-embutido, mesmo com Id compatível");
    }

    // ---------- WindowState: displays externos (multi-janela) ----------

    [Test] public static void HasExternalDisplay_SemNenhumAdicionado_EhFalso()
    {
        var window = new WindowState(NovoDisplayPrincipal());
        Assert.False(window.HasExternalDisplay, what: "nenhum display externo foi conectado ainda");
    }

    [Test] public static void AddOrUpdateExternalDisplay_ComBuiltInTrue_Lanca()
    {
        var window = new WindowState(NovoDisplayPrincipal());
        Assert.Throws<ArgumentException>(() => window.AddOrUpdateExternalDisplay(NovoDisplayPrincipal(id: 1)),
            what: "um display externo não pode se declarar embutido — isso é papel exclusivo de SetPrimaryDisplay");
    }

    [Test] public static void AddOrUpdateExternalDisplay_NovoId_ApareceEmExternalDisplays()
    {
        var window = new WindowState(NovoDisplayPrincipal());
        window.AddOrUpdateExternalDisplay(NovoDisplayExterno(id: 1));

        Assert.True(window.HasExternalDisplay, what: "display externo recém-conectado deve ficar visível");
        Assert.Equal(1, window.ExternalDisplays.Count, what: "exatamente um display externo foi adicionado");
    }

    [Test] public static void AddOrUpdateExternalDisplay_MesmoIdDuasVezes_AtualizaEmVezDeDuplicar()
    {
        var window = new WindowState(NovoDisplayPrincipal());
        window.AddOrUpdateExternalDisplay(NovoDisplayExterno(id: 1, refreshHz: 60f));
        window.AddOrUpdateExternalDisplay(NovoDisplayExterno(id: 1, refreshHz: 144f));

        Assert.Equal(1, window.ExternalDisplays.Count, what: "mesmo Id deve atualizar a entrada existente, não duplicar");
        Assert.True(window.TryGetExternalDisplay(1, out var display), what: "display atualizado deve continuar recuperável pelo Id");
        Assert.Close(144f, display.RefreshRateHz, what: "a atualização mais recente deve prevalecer");
    }

    [Test] public static void RemoveDisplay_IdExistente_RemoveDeExternalDisplays()
    {
        var window = new WindowState(NovoDisplayPrincipal());
        window.AddOrUpdateExternalDisplay(NovoDisplayExterno(id: 1));
        window.RemoveDisplay(1);

        Assert.False(window.HasExternalDisplay, what: "display desconectado não deve mais aparecer");
    }

    [Test] public static void RemoveDisplay_IdInexistente_EhNoOp()
    {
        var window = new WindowState(NovoDisplayPrincipal());
        window.RemoveDisplay(999); // não deve lançar
        Assert.True(true, what: "remover um Id nunca adicionado é idempotente, não é erro");
    }

    [Test] public static void TryGetExternalDisplay_IdInexistente_DevolveFalso()
    {
        var window = new WindowState(NovoDisplayPrincipal());
        Assert.False(window.TryGetExternalDisplay(42, out _), what: "consultar um display externo nunca conectado deve devolver falso, não lançar");
    }

    [Test] public static void MultiplosDisplaysExternos_SaoTodosRastreadosIndependentemente()
    {
        var window = new WindowState(NovoDisplayPrincipal());
        window.AddOrUpdateExternalDisplay(NovoDisplayExterno(id: 1));
        window.AddOrUpdateExternalDisplay(NovoDisplayExterno(id: 2));

        Assert.Equal(2, window.ExternalDisplays.Count, what: "dois displays externos distintos devem coexistir (cenário de multi-janela do item 1.1.4)");
    }
}
