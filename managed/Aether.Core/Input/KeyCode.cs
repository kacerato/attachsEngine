namespace Aether.Input;

/// <summary>
/// Item 1.1.2 do plano: conjunto portátil de teclas físicas, independente de plataforma. Cobre o
/// necessário para atalhos de editor (letras, números, modificadores, navegação) — não é um mapa de
/// layout completo de teclado (isso pertence à camada de texto/IME, fora do escopo desta fatia).
/// Valores numéricos estáveis: este enum é serializado em binds de atalho salvos em disco (item 1.5.4),
/// então a ordem não pode ser reordenada livremente depois que a primeira versão for lançada — só
/// adicionar no final, mesma disciplina de <c>ConfigValueKind</c>.
/// </summary>
public enum KeyCode
{
    Unknown = 0,

    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    Digit0, Digit1, Digit2, Digit3, Digit4,
    Digit5, Digit6, Digit7, Digit8, Digit9,

    Space, Enter, Escape, Tab, Backspace, Delete,
    ArrowLeft, ArrowRight, ArrowUp, ArrowDown,

    ShiftLeft, ShiftRight, ControlLeft, ControlRight, AltLeft, AltRight,

    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
}

/// <summary>Fase de um evento de teclado — mesma forma de <see cref="TouchPhase"/>, sem <c>Stationary</c>
/// (tecla segurada não gera evento por frame; repetição de texto é responsabilidade da camada de IME).</summary>
public enum KeyPhase : byte
{
    Down,
    Up,
}

/// <summary>Um evento discreto de tecla física. Struct blittable, mesma disciplina de <see
/// cref="TouchPoint"/>.</summary>
public readonly struct KeyEvent
{
    public readonly KeyCode Code;
    public readonly KeyPhase Phase;

    /// <summary>Verdadeiro se o evento é uma repetição de tecla segurada (auto-repeat do SO), não um
    /// novo toque físico. Irrelevante em <see cref="KeyPhase.Up"/>.</summary>
    public readonly bool IsRepeat;

    public readonly long TimestampTicks;

    public KeyEvent(KeyCode code, KeyPhase phase, bool isRepeat, long timestampTicks)
    {
        Code = code;
        Phase = phase;
        IsRepeat = isRepeat;
        TimestampTicks = timestampTicks;
    }
}
