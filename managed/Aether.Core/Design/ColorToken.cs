namespace Aether.Design;

/// <summary>
/// Item 0.3.4 do plano: cor RGBA normalizada [0,1] — mesma faixa usada pelo resto da engine
/// gráfica (shaders, materiais), para que um token de design possa alimentar diretamente um
/// uniform sem conversão. Struct blittable, sem alocação.
/// </summary>
public readonly struct ColorToken : IEquatable<ColorToken>
{
    public readonly float R;
    public readonly float G;
    public readonly float B;
    public readonly float A;

    public ColorToken(float r, float g, float b, float a = 1f)
    {
        R = r; G = g; B = b; A = a;
    }

    /// <summary>Constrói a partir de um literal hexadecimal RGB (ex.: 0x4CC9E0), como os usados no
    /// protótipo (<c>prototype/editor.html</c>, custom properties <c>--accent</c> etc.) — ponto de
    /// entrada natural para transcrever uma paleta já validada visualmente sem erro de conversão
    /// manual de byte para float.</summary>
    public static ColorToken FromHex(uint rgb, float alpha = 1f)
    {
        float r = ((rgb >> 16) & 0xFF) / 255f;
        float g = ((rgb >> 8) & 0xFF) / 255f;
        float b = (rgb & 0xFF) / 255f;
        return new ColorToken(r, g, b, alpha);
    }

    public bool Equals(ColorToken other) => R == other.R && G == other.G && B == other.B && A == other.A;
    public override bool Equals(object? obj) => obj is ColorToken other && Equals(other);
    public override int GetHashCode() => HashCode.Combine(R, G, B, A);
    public static bool operator ==(ColorToken left, ColorToken right) => left.Equals(right);
    public static bool operator !=(ColorToken left, ColorToken right) => !left.Equals(right);
}
