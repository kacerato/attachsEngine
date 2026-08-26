using Aether;

namespace Aether.Input;

/// <summary>
/// Item 1.1.2 do plano: dados específicos de caneta/stylus (S Pen, Apple Pencil) que um dedo comum
/// não produz. Sempre acompanha um <see cref="TouchPoint"/> com o mesmo <see cref="TouchPoint.Id"/>
/// — não é um canal de input separado, é informação adicional sobre um contato que a plataforma
/// identificou como caneta.
/// </summary>
public readonly struct PenInfo
{
    /// <summary>Pressão normalizada [0, 1] reportada pela caneta — mais fina que <see
    /// cref="TouchPoint.Pressure"/> de dedo comum, que a maioria dos aparelhos fixa em 1.0.</summary>
    public readonly float Pressure;

    /// <summary>Inclinação do eixo X em radianos, 0 = caneta perpendicular à tela.</summary>
    public readonly float TiltX;

    /// <summary>Inclinação do eixo Y em radianos, 0 = caneta perpendicular à tela.</summary>
    public readonly float TiltY;

    /// <summary>Rotação da caneta em torno do próprio eixo, em radianos — só algumas canetas (ex.:
    /// Apple Pencil 2ª geração) reportam; 0 quando não suportado.</summary>
    public readonly float Rotation;

    /// <summary>Verdadeiro enquanto o botão lateral/barril da caneta está pressionado.</summary>
    public readonly bool BarrelButtonDown;

    /// <summary>Verdadeiro quando a ponta está próxima da tela mas ainda não encostou (hover) —
    /// só algumas combinações de hardware/driver reportam essa fase.</summary>
    public readonly bool IsHovering;

    public PenInfo(float pressure, float tiltX, float tiltY, float rotation, bool barrelButtonDown, bool isHovering)
    {
        Pressure = pressure;
        TiltX = tiltX;
        TiltY = tiltY;
        Rotation = rotation;
        BarrelButtonDown = barrelButtonDown;
        IsHovering = isHovering;
    }
}
