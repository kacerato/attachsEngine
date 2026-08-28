namespace Aether.Design;

/// <summary>
/// Item 0.3.4 do plano: escala de espaçamento e raio de canto, em dp (density-independent pixel —
/// mesma unidade de <see cref="Aether.Platform.DisplayInfo.DensityScale"/>, item 1.1.4; multiplicar
/// por essa densidade converte para pixels físicos de tela).
/// <para>
/// <c>prototype/editor.html</c> (item 0.3.2) usa valores de espaçamento soltos e inconsistentes
/// (4, 5, 6, 7, 8, 9, 10, 12, 14, 18px, sem progressão) — <see cref="MATRIZ-MARCOS.md"/> já registra
/// isso como "estilos ad hoc, não um sistema formal". Esta escala formaliza uma progressão
/// geométrica de base 4 (padrão comum em design mobile: Material Design, Human Interface
/// Guidelines), escolhida para cobrir a mesma faixa de valores observados no protótipo sem
/// replicar cada número solto.
/// </para>
/// </summary>
public static class SpacingScale
{
    /// <summary>Menor unidade de espaçamento — divisor entre elementos muito próximos (ex.: ícone e
    /// rótulo dentro do mesmo controle).</summary>
    public const float XSmall = 4f;

    /// <summary>Espaçamento padrão entre controles relacionados dentro de um grupo.</summary>
    public const float Small = 8f;

    /// <summary>Espaçamento entre grupos de controles distintos.</summary>
    public const float Medium = 12f;

    /// <summary>Margem interna padrão de um painel/card.</summary>
    public const float Large = 16f;

    /// <summary>Margem entre regiões estruturais da tela (ex.: entre o viewport e um painel
    /// lateral).</summary>
    public const float XLarge = 24f;

    /// <summary>Espaçamento generoso, usado com moderação — separação entre seções muito
    /// distintas.</summary>
    public const float XXLarge = 32f;

    /// <summary>Raio de canto pequeno — controles inline (chip, badge).</summary>
    public const float RadiusSmall = 4f;

    /// <summary>Raio de canto padrão — botões, campos.</summary>
    public const float RadiusMedium = 6f;

    /// <summary>Raio de canto grande — painéis, cards, o menu radial.</summary>
    public const float RadiusLarge = 14f;

    /// <summary>Altura da barra superior (CONVENCOES.md / plano §8.2: "Barra superior — topo, 40
    /// dp"). Constante estrutural, não parte da progressão geométrica — dimensão de layout fixa
    /// citada explicitamente pelo plano, transcrita aqui como dado único de verdade.</summary>
    public const float TopBarHeight = 40f;

    /// <summary>Largura da doca de modos lateral esquerda (plano §8.2: "Doca de modos — borda
    /// esquerda, 64 dp").</summary>
    public const float DockWidth = 64f;

    /// <summary>Altura do trilho inferior (plano §8.2: "Trilho inferior — base, 44 dp").</summary>
    public const float RailHeight = 44f;
}
