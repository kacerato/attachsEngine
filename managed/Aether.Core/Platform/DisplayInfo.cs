namespace Aether.Platform;

/// <summary>
/// Item 1.1.4 do plano: descreve um display físico conectado — o principal (a tela do próprio
/// aparelho) ou um externo (via USB-C/wireless casting, "display externo" citado pelo item). Struct
/// blittable: quando a captação real existir, a plataforma preenche isto a partir de
/// <c>Display.getMode()</c>/<c>DisplayCutout</c> no Android ou <c>UIScreen</c> no iOS e empurra para
/// <see cref="WindowState"/>, sem o Core precisar saber qual API por trás foi usada.
/// </summary>
public readonly struct DisplayInfo
{
    /// <summary>Identificador estável enquanto o display permanece conectado — índice lógico da
    /// plataforma, não um handle geracional (mesma justificativa de <see
    /// cref="Aether.Input.GamepadState.Index"/>: a plataforma já garante não reciclar enquanto
    /// conectado).</summary>
    public readonly int Id;

    public readonly int WidthPixels;
    public readonly int HeightPixels;

    /// <summary>Pixels físicos por dp lógico (density-independent pixel) — mesma escala usada por
    /// Android (<c>DisplayMetrics.density</c>) e iOS (<c>UIScreen.scale</c>). Multiplica um valor em
    /// dp para obter pixels físicos.</summary>
    public readonly float DensityScale;

    /// <summary>Taxa de atualização corrente em Hz — pode variar em tempo real em displays com taxa
    /// adaptativa (ex.: 60/90/120 Hz dinâmico), daí não ser só uma constante de <see
    /// cref="DisplayInfo"/> mas parte do que <see cref="WindowState.SetPrimaryDisplay"/> atualiza a
    /// cada mudança relatada pela plataforma.</summary>
    public readonly float RefreshRateHz;

    public readonly SafeAreaInsets SafeArea;

    /// <summary>Verdadeiro para o display embutido do próprio aparelho; falso para um display
    /// externo conectado (o "display externo" citado pelo item 1.1.4).</summary>
    public readonly bool IsBuiltIn;

    public DisplayInfo(int id, int widthPixels, int heightPixels, float densityScale, float refreshRateHz, SafeAreaInsets safeArea, bool isBuiltIn)
    {
        if (widthPixels < 0) throw new ArgumentOutOfRangeException(nameof(widthPixels), "largura não pode ser negativa");
        if (heightPixels < 0) throw new ArgumentOutOfRangeException(nameof(heightPixels), "altura não pode ser negativa");
        if (densityScale <= 0f) throw new ArgumentOutOfRangeException(nameof(densityScale), "escala de densidade deve ser positiva");
        if (refreshRateHz < 0f) throw new ArgumentOutOfRangeException(nameof(refreshRateHz), "taxa de atualização não pode ser negativa");

        Id = id;
        WidthPixels = widthPixels;
        HeightPixels = heightPixels;
        DensityScale = densityScale;
        RefreshRateHz = refreshRateHz;
        SafeArea = safeArea;
        IsBuiltIn = isBuiltIn;
    }

    /// <summary>Área útil em pixels, descontando <see cref="SafeArea"/> de cada borda. Nunca negativa
    /// — se os insets somados excederem a dimensão (configuração degenerada vinda de um bug de
    /// plataforma), satura em zero em vez de devolver um retângulo com largura/altura negativa.</summary>
    public (float Width, float Height) SafeAreaSize()
    {
        float width = MathF.Max(0f, WidthPixels - SafeArea.Left - SafeArea.Right);
        float height = MathF.Max(0f, HeightPixels - SafeArea.Top - SafeArea.Bottom);
        return (width, height);
    }
}
