using Aether;

namespace Aether.Input;

/// <summary>
/// Item 1.1.2 do plano: fase de um contato de toque dentro do seu ciclo de vida — mesmo vocabulário
/// usado por Android (<c>MotionEvent.ACTION_*</c>) e iOS (<c>UITouch.Phase</c>), para que a
/// integração futura de cada plataforma seja uma tradução direta, não uma reinterpretação.
/// </summary>
public enum TouchPhase : byte
{
    /// <summary>O dedo tocou a tela neste frame — primeiro evento deste <see cref="TouchPoint.Id"/>.</summary>
    Began,

    /// <summary>O dedo se moveu desde o último frame.</summary>
    Moved,

    /// <summary>O dedo está encostado, mas não se moveu desde o último frame (ainda presente na
    /// lista de toques ativos — diferente de não gerar nenhum evento).</summary>
    Stationary,

    /// <summary>O dedo foi levantado neste frame — último evento deste <see cref="TouchPoint.Id"/>,
    /// que deixa de existir a partir do frame seguinte.</summary>
    Ended,

    /// <summary>A plataforma interrompeu o toque sem o dedo ter sido levantado (ex.: uma notificação
    /// do sistema tomou o foco). Chamador deve tratar como abandono, não como <see cref="Ended"/> —
    /// não é seguro assumir que a posição final reflete uma intenção do usuário.</summary>
    Cancelled,
}

/// <summary>
/// Um contato de toque num frame, com posição atual/anterior para permitir delta sem estado externo.
/// Struct blittable — pode atravessar a fronteira nativa (CONVENCOES.md §2) quando a captação real de
/// Android/iOS for integrada; aqui dentro do Core é o formato canônico e portátil.
/// </summary>
public readonly struct TouchPoint
{
    /// <summary>Identificador estável deste contato enquanto o dedo permanece na tela — mesma
    /// identidade do <c>pointerId</c> do Android / <c>UITouch</c> do iOS, não um índice de slot
    /// reciclável (dois toques simultâneos nunca compartilham <see cref="Id"/>).</summary>
    public readonly int Id;

    public readonly TouchPhase Phase;

    /// <summary>Posição em pixels de tela neste frame, origem no canto superior esquerdo.</summary>
    public readonly float2 Position;

    /// <summary>Posição no frame anterior em que este <see cref="Id"/> teve um evento. Igual a
    /// <see cref="Position"/> em <see cref="TouchPhase.Began"/> (não há "anterior" ainda).</summary>
    public readonly float2 PreviousPosition;

    /// <summary>Pressão normalizada, tipicamente [0, 1]; 1.0 quando o hardware não reporta pressão
    /// (dedo comum, a maioria dos aparelhos Android/iOS sem 3D Touch).</summary>
    public readonly float Pressure;

    /// <summary>Raio de contato estimado em pixels de tela; 0 quando a plataforma não reporta.</summary>
    public readonly float RadiusMajor;

    /// <summary>Timestamp em ticks de <see cref="System.Diagnostics.Stopwatch.GetTimestamp"/> — mesma
    /// base de tempo usada por <c>JobDiagnostics</c> (item 1.2.4), para poder correlacionar latência
    /// de input com o restante da instrumentação sem conversão.</summary>
    public readonly long TimestampTicks;

    public TouchPoint(int id, TouchPhase phase, float2 position, float2 previousPosition, float pressure, float radiusMajor, long timestampTicks)
    {
        Id = id;
        Phase = phase;
        Position = position;
        PreviousPosition = previousPosition;
        Pressure = pressure;
        RadiusMajor = radiusMajor;
        TimestampTicks = timestampTicks;
    }

    /// <summary>Delta de posição desde o frame anterior deste contato. Zero em <see cref="TouchPhase.Began"/>.</summary>
    public float2 Delta => Position - PreviousPosition;
}
