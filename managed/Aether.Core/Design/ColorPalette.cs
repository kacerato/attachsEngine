namespace Aether.Design;

/// <summary>
/// Item 0.3.4 do plano: paleta de cores por papel semântico (não por valor) — "qual cor é o fundo
/// do painel", não "qual cor é #191D24". É a mesma disciplina de custom properties CSS já validada
/// visualmente em <c>prototype/editor.html</c> (item 0.3.2), transcrita aqui como dados versionados
/// e testáveis em vez de estilo solto num arquivo HTML.
/// <para>
/// Duas instâncias — <see cref="Dark"/> e <see cref="Light"/> — cobrem os dois temas que o
/// protótipo já demonstra funcionar (a régua completa de <c>:root</c>/<c>[data-theme=light]</c> do
/// protótipo). Eixos X/Y/Z seguem a convenção de cor já fixada em CONVENCOES.md §5 (mão-esquerda,
/// Y-para-cima): vermelho/verde/azul para X/Y/Z, mesma associação universal de engines 3D.
/// </para>
/// </summary>
public sealed class ColorPalette
{
    public required ColorToken Ground { get; init; }
    public required ColorToken Panel { get; init; }
    public required ColorToken PanelHigh { get; init; }
    public required ColorToken Line { get; init; }
    public required ColorToken LineSoft { get; init; }
    public required ColorToken Text { get; init; }
    public required ColorToken TextDim { get; init; }
    public required ColorToken TextFaint { get; init; }
    public required ColorToken Accent { get; init; }
    public required ColorToken AccentDim { get; init; }
    public required ColorToken AccentInk { get; init; }
    public required ColorToken Warn { get; init; }
    public required ColorToken Critical { get; init; }
    public required ColorToken Ok { get; init; }
    public required ColorToken AxisX { get; init; }
    public required ColorToken AxisY { get; init; }
    public required ColorToken AxisZ { get; init; }

    /// <summary>Transcrito de <c>prototype/editor.html</c>, bloco <c>:root</c> (tema escuro,
    /// padrão do editor — CONVENCOES.md §1: "orientação landscape, fullscreen", ambiente tipicamente
    /// de baixa luz para sessão longa de edição).</summary>
    public static ColorPalette Dark { get; } = new()
    {
        Ground = ColorToken.FromHex(0x12151A),
        Panel = ColorToken.FromHex(0x191D24),
        PanelHigh = ColorToken.FromHex(0x1F242D),
        Line = ColorToken.FromHex(0x252B34),
        LineSoft = ColorToken.FromHex(0x1D222A),
        Text = ColorToken.FromHex(0xD6DDE6),
        TextDim = ColorToken.FromHex(0x8A94A3),
        TextFaint = ColorToken.FromHex(0x5C6675),
        Accent = ColorToken.FromHex(0x4CC9E0),
        AccentDim = ColorToken.FromHex(0x2A7A8A),
        AccentInk = ColorToken.FromHex(0x07222A),
        Warn = ColorToken.FromHex(0xE0A24C),
        Critical = ColorToken.FromHex(0xE0576B),
        Ok = ColorToken.FromHex(0x7BD16A),
        AxisX = ColorToken.FromHex(0xE0576B),
        AxisY = ColorToken.FromHex(0x7BD16A),
        AxisZ = ColorToken.FromHex(0x5B8FE0),
    };

    /// <summary>Transcrito de <c>prototype/editor.html</c>, bloco <c>[data-theme=light]</c>.</summary>
    public static ColorPalette Light { get; } = new()
    {
        Ground = ColorToken.FromHex(0xE8EBEF),
        Panel = ColorToken.FromHex(0xF4F6F8),
        PanelHigh = ColorToken.FromHex(0xFFFFFF),
        Line = ColorToken.FromHex(0xCBD2DA),
        LineSoft = ColorToken.FromHex(0xDDE2E8),
        Text = ColorToken.FromHex(0x1A1F26),
        TextDim = ColorToken.FromHex(0x5C6675),
        TextFaint = ColorToken.FromHex(0x949DA9),
        Accent = ColorToken.FromHex(0x0E7C92),
        AccentDim = ColorToken.FromHex(0x6FBCCB),
        AccentInk = ColorToken.FromHex(0xFFFFFF),
        Warn = ColorToken.FromHex(0xA66A12),
        Critical = ColorToken.FromHex(0xB3273C),
        Ok = ColorToken.FromHex(0x2F7A22),
        AxisX = ColorToken.FromHex(0xC22A40),
        AxisY = ColorToken.FromHex(0x3D8C2E),
        AxisZ = ColorToken.FromHex(0x2A5FB3),
    };
}
