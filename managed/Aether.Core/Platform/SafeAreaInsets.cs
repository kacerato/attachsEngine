namespace Aether.Platform;

/// <summary>
/// Item 1.1.4 do plano: margem, em pixels de tela, que cada borda da janela perde para elementos do
/// sistema — notch/câmera furo-na-tela, barra de gestos, cantos arredondados, área de status bar
/// quando não totalmente ocultável. CONVENCOES.md §1 fixa o editor como landscape/fullscreen, então
/// <see cref="Top"/>/<see cref="Bottom"/> tendem a zero na maioria dos aparelhos modernos em
/// landscape — mas <see cref="Left"/>/<see cref="Right"/> são onde o notch e a barra de gestos
/// realmente cortam a área útil quando o aparelho é segurado deitado.
/// <para>
/// Consumida, não recalculada, por qualquer camada de UI (item 4.6.1, "safe area" de Canvas) — este
/// tipo só carrega o número que a plataforma informou.
/// </para>
/// </summary>
public readonly struct SafeAreaInsets
{
    public readonly float Left;
    public readonly float Top;
    public readonly float Right;
    public readonly float Bottom;

    public SafeAreaInsets(float left, float top, float right, float bottom)
    {
        Left = left;
        Top = top;
        Right = right;
        Bottom = bottom;
    }

    public static SafeAreaInsets Zero => default;

    public bool IsZero => Left == 0f && Top == 0f && Right == 0f && Bottom == 0f;
}
