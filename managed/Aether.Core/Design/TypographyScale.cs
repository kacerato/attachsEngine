namespace Aether.Design;

/// <summary>
/// Item 0.3.4 do plano: um degrau nomeado da escala tipográfica. <see cref="SizeDp"/> segue a
/// mesma unidade de <see cref="SpacingScale"/> (dp, não pixel físico).
/// </summary>
public readonly struct TypographyStyle
{
    public readonly float SizeDp;

    /// <summary>Espaçamento entre linhas como múltiplo de <see cref="SizeDp"/> (ex.: 1.5 = 150%).</summary>
    public readonly float LineHeightMultiplier;

    /// <summary>Espaçamento entre letras em dp — só os rótulos em versalete do protótipo usam um
    /// valor não-zero (<c>letter-spacing: .04em</c> em <c>.sheet-head</c>); a maioria é 0.</summary>
    public readonly float LetterSpacingDp;

    public TypographyStyle(float sizeDp, float lineHeightMultiplier, float letterSpacingDp = 0f)
    {
        SizeDp = sizeDp;
        LineHeightMultiplier = lineHeightMultiplier;
        LetterSpacingDp = letterSpacingDp;
    }
}

/// <summary>
/// Item 0.3.4 do plano: escala tipográfica nomeada. <c>prototype/editor.html</c> usa uma faixa
/// contínua e não-sistemática de 9 a 19px (9, 10.5, 11, 11.5, 12, 12.5, 13, 14, 17, 19px) — esta
/// escala formaliza 6 degraus nomeados que cobrem a mesma faixa observada, para que todo texto do
/// editor escolha entre um conjunto finito e consistente, não um valor solto por elemento.
/// <para>
/// Duas famílias de fonte, ambas já citadas no protótipo: <see cref="UiFontFamily"/> (interface
/// geral) e <see cref="MonoFontFamily"/> (código/dados tabulares — usada no Modo Lista do
/// AetherFlow, item 5.4, e em telemetria numérica).
/// </para>
/// </summary>
public static class TypographyScale
{
    /// <summary>Rótulos pequenos: badges, timestamps, texto de apoio secundário.</summary>
    public static TypographyStyle Caption { get; } = new(sizeDp: 11f, lineHeightMultiplier: 1.3f);

    /// <summary>Corpo de texto padrão: labels de controle, valores de propriedade no Inspector.</summary>
    public static TypographyStyle Body { get; } = new(sizeDp: 13f, lineHeightMultiplier: 1.4f);

    /// <summary>Corpo de texto em contexto denso (código do Modo Lista, dados tabulares) — usa
    /// <see cref="MonoFontFamily"/>, altura de linha maior para legibilidade de código.</summary>
    public static TypographyStyle BodyMono { get; } = new(sizeDp: 12.5f, lineHeightMultiplier: 1.85f);

    /// <summary>Rótulo em versalete de cabeçalho de seção (ex.: cabeçalho do painel deslizante).</summary>
    public static TypographyStyle Overline { get; } = new(sizeDp: 12f, lineHeightMultiplier: 1.2f, letterSpacingDp: 0.5f);

    /// <summary>Título de painel/seção.</summary>
    public static TypographyStyle Title { get; } = new(sizeDp: 14f, lineHeightMultiplier: 1.3f);

    /// <summary>Título principal — nome do projeto na barra superior, títulos de destaque.</summary>
    public static TypographyStyle Headline { get; } = new(sizeDp: 19f, lineHeightMultiplier: 1.2f);

    /// <summary>Transcrito de <c>prototype/editor.html</c>: <c>"Barlow Semi Condensed", "Roboto
    /// Condensed", system-ui, sans-serif</c> — condensada, escolhida para caber mais texto na
    /// largura estreita dos painéis laterais (plano §8.2, painéis entre 20% e 55% da largura da
    /// tela).</summary>
    public const string UiFontFamily = "Barlow Semi Condensed, Roboto Condensed, system-ui, sans-serif";

    /// <summary>Transcrito de <c>prototype/editor.html</c>: <c>"IBM Plex Mono", ui-monospace, "SF
    /// Mono", Menlo, monospace</c>.</summary>
    public const string MonoFontFamily = "IBM Plex Mono, ui-monospace, SF Mono, Menlo, monospace";
}
