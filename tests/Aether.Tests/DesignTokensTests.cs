using Aether.Design;

namespace Aether.Tests;

/// <summary>
/// Item 0.3.4 do plano: sistema de design (tokens de cor, tipografia, espaçamento, vocabulário
/// háptico). Valores de cor/dimensão transcritos de <c>prototype/editor.html</c> (item 0.3.2, já
/// validado visualmente) — estes testes garantem que a transcrição é exata e que a estrutura de
/// dados se mantém consistente, não que as escolhas de design em si estão "certas" (isso é
/// avaliação de UX, fora do escopo de um teste automatizado).
/// </summary>
public static class DesignTokensTests
{
    // ---------- ColorToken ----------

    [Test] public static void FromHex_CorConhecida_ConverteComponentesCorretamente()
    {
        var accent = ColorToken.FromHex(0x4CC9E0);
        Assert.Close(0x4C / 255f, accent.R, what: "componente R deve bater com o byte alto do hex");
        Assert.Close(0xC9 / 255f, accent.G, what: "componente G deve bater com o byte do meio do hex");
        Assert.Close(0xE0 / 255f, accent.B, what: "componente B deve bater com o byte baixo do hex");
        Assert.Close(1f, accent.A, what: "alpha default deve ser opaco");
    }

    [Test] public static void FromHex_Preto_TodosComponentesZero()
    {
        var preto = ColorToken.FromHex(0x000000);
        Assert.Close(0f, preto.R, what: "preto deve ter R zero");
        Assert.Close(0f, preto.G, what: "preto deve ter G zero");
        Assert.Close(0f, preto.B, what: "preto deve ter B zero");
    }

    [Test] public static void FromHex_Branco_TodosComponentesUm()
    {
        var branco = ColorToken.FromHex(0xFFFFFF);
        Assert.Close(1f, branco.R, what: "branco deve ter R máximo");
        Assert.Close(1f, branco.G, what: "branco deve ter G máximo");
        Assert.Close(1f, branco.B, what: "branco deve ter B máximo");
    }

    [Test] public static void FromHex_ComAlphaExplicito_PreservaAlpha()
    {
        var semitransparente = ColorToken.FromHex(0xFFFFFF, alpha: 0.5f);
        Assert.Close(0.5f, semitransparente.A, what: "alpha explícito deve ser preservado, não sobrescrito pelo default");
    }

    [Test] public static void Equals_MesmosComponentes_SaoIguais()
    {
        var a = new ColorToken(0.1f, 0.2f, 0.3f, 1f);
        var b = new ColorToken(0.1f, 0.2f, 0.3f, 1f);
        Assert.True(a.Equals(b), what: "cores com componentes idênticos devem ser iguais");
        Assert.True(a == b, what: "operador == deve refletir Equals");
    }

    [Test] public static void Equals_ComponentesDiferentes_NaoSaoIguais()
    {
        var a = new ColorToken(0.1f, 0.2f, 0.3f, 1f);
        var b = new ColorToken(0.1f, 0.2f, 0.4f, 1f);
        Assert.False(a.Equals(b), what: "componente B diferente deve tornar as cores diferentes");
        Assert.True(a != b, what: "operador != deve refletir a diferença");
    }

    // ---------- ColorPalette ----------

    [Test] public static void Dark_AccentBateComPrototipo()
    {
        var esperado = ColorToken.FromHex(0x4CC9E0);
        Assert.True(ColorPalette.Dark.Accent == esperado, what: "cor de destaque do tema escuro deve bater exatamente com --accent do protótipo");
    }

    [Test] public static void Light_AccentBateComPrototipo()
    {
        var esperado = ColorToken.FromHex(0x0E7C92);
        Assert.True(ColorPalette.Light.Accent == esperado, what: "cor de destaque do tema claro deve bater exatamente com --accent do protótipo");
    }

    [Test] public static void Dark_EixosSeguemConvencaoDeCores()
    {
        Assert.True(ColorPalette.Dark.AxisX == ColorToken.FromHex(0xE0576B), what: "eixo X deve ser vermelho, convenção universal de engines 3D");
        Assert.True(ColorPalette.Dark.AxisY == ColorToken.FromHex(0x7BD16A), what: "eixo Y deve ser verde");
        Assert.True(ColorPalette.Dark.AxisZ == ColorToken.FromHex(0x5B8FE0), what: "eixo Z deve ser azul");
    }

    [Test] public static void DarkELight_SaoPaletasDistintas()
    {
        Assert.True(ColorPalette.Dark.Ground != ColorPalette.Light.Ground, what: "fundo do tema escuro e claro devem ser cores diferentes — não faria sentido as duas paletas serem idênticas");
    }

    [Test] public static void Dark_FundoEhMaisEscuroQueTexto()
    {
        // Aproximação de luminância perceptual — só precisa confirmar a relação de contraste
        // básica esperada de um tema escuro, não replicar a fórmula WCAG completa.
        float luminanciaFundo = 0.2126f * ColorPalette.Dark.Ground.R + 0.7152f * ColorPalette.Dark.Ground.G + 0.0722f * ColorPalette.Dark.Ground.B;
        float luminanciaTexto = 0.2126f * ColorPalette.Dark.Text.R + 0.7152f * ColorPalette.Dark.Text.G + 0.0722f * ColorPalette.Dark.Text.B;
        Assert.True(luminanciaFundo < luminanciaTexto, what: "tema escuro deve ter fundo mais escuro que o texto, senão não é legível");
    }

    // ---------- SpacingScale ----------

    [Test] public static void SpacingScale_ProgressaoCrescente()
    {
        Assert.True(SpacingScale.XSmall < SpacingScale.Small, what: "escala deve ser estritamente crescente");
        Assert.True(SpacingScale.Small < SpacingScale.Medium, what: "escala deve ser estritamente crescente");
        Assert.True(SpacingScale.Medium < SpacingScale.Large, what: "escala deve ser estritamente crescente");
        Assert.True(SpacingScale.Large < SpacingScale.XLarge, what: "escala deve ser estritamente crescente");
        Assert.True(SpacingScale.XLarge < SpacingScale.XXLarge, what: "escala deve ser estritamente crescente");
    }

    [Test] public static void SpacingScale_DimensoesEstruturaisBatemComOPlano()
    {
        Assert.Close(40f, SpacingScale.TopBarHeight, what: "barra superior deve ter 40dp, conforme plano §8.2");
        Assert.Close(64f, SpacingScale.DockWidth, what: "doca de modos deve ter 64dp, conforme plano §8.2");
        Assert.Close(44f, SpacingScale.RailHeight, what: "trilho inferior deve ter 44dp, conforme plano §8.2");
    }

    [Test] public static void SpacingScale_RaiosDeCantoProgressaoCrescente()
    {
        Assert.True(SpacingScale.RadiusSmall < SpacingScale.RadiusMedium, what: "raios de canto devem crescer de small para medium");
        Assert.True(SpacingScale.RadiusMedium < SpacingScale.RadiusLarge, what: "raios de canto devem crescer de medium para large");
    }

    // ---------- TypographyScale ----------

    [Test] public static void TypographyScale_ProgressaoCrescenteDeCaptionAHeadline()
    {
        Assert.True(TypographyScale.Caption.SizeDp < TypographyScale.Body.SizeDp, what: "Caption deve ser menor que Body");
        Assert.True(TypographyScale.Body.SizeDp < TypographyScale.Title.SizeDp, what: "Body deve ser menor que Title");
        Assert.True(TypographyScale.Title.SizeDp < TypographyScale.Headline.SizeDp, what: "Title deve ser menor que Headline");
    }

    [Test] public static void TypographyScale_TodosOsTamanhosSaoPositivos()
    {
        Assert.True(TypographyScale.Caption.SizeDp > 0f, what: "tamanho de fonte deve ser positivo");
        Assert.True(TypographyScale.BodyMono.SizeDp > 0f, what: "tamanho de fonte deve ser positivo");
        Assert.True(TypographyScale.Overline.SizeDp > 0f, what: "tamanho de fonte deve ser positivo");
    }

    [Test] public static void TypographyScale_TodaAlturaDeLinhaEhMaiorQueUm()
    {
        // Altura de linha menor que 1x o tamanho da fonte cortaria descendentes de letra —
        // invariante básica de legibilidade que nenhum degrau deve violar.
        Assert.True(TypographyScale.Caption.LineHeightMultiplier >= 1f, what: "altura de linha não pode ser menor que o próprio tamanho da fonte");
        Assert.True(TypographyScale.BodyMono.LineHeightMultiplier >= 1f, what: "altura de linha não pode ser menor que o próprio tamanho da fonte");
        Assert.True(TypographyScale.Headline.LineHeightMultiplier >= 1f, what: "altura de linha não pode ser menor que o próprio tamanho da fonte");
    }

    // ---------- HapticVocabulary ----------

    [Test] public static void IntensityFor_TodoHapticCue_TemIntensidadeDefinida()
    {
        foreach (HapticCue cue in Enum.GetValues<HapticCue>())
        {
            // Não deve lançar KeyNotFoundException — todo membro do enum precisa estar mapeado.
            // Este teste é o que impede alguém de adicionar um novo HapticCue e esquecer de mapear
            // sua intensidade (ver comentário em HapticVocabulary.IntensityFor).
            HapticIntensity intensidade = HapticVocabulary.IntensityFor(cue);
            Assert.True(Enum.IsDefined(intensidade), what: $"intensidade de {cue} deve ser um valor válido de HapticIntensity");
        }
    }

    [Test] public static void IntensityFor_Erro_EhMaisForteQueSelecao()
    {
        Assert.True(HapticVocabulary.IntensityFor(HapticCue.Error) > HapticVocabulary.IntensityFor(HapticCue.Selection),
            what: "erro deve ter feedback tátil distinto e mais forte que seleção comum, para o usuário sentir a diferença sem olhar a tela");
    }

    [Test] public static void IntensityFor_SelecaoEAberturaDeMenuRadial_SaoAmbosLeves()
    {
        Assert.Equal(HapticIntensity.Light, HapticVocabulary.IntensityFor(HapticCue.Selection), what: "seleção deve ser um toque leve");
        Assert.Equal(HapticIntensity.Light, HapticVocabulary.IntensityFor(HapticCue.RadialMenuOpen), what: "abertura de menu radial deve ser um toque leve, é um reconhecimento de gesto, não uma confirmação de ação");
    }
}
