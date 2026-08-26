using Aether;

namespace Aether.Input;

[Flags]
public enum MouseButtons
{
    None = 0,
    Left = 1 << 0,
    Right = 1 << 1,
    Middle = 1 << 2,
}

/// <summary>
/// Item 1.1.2 do plano: estado de mouse/trackpad num frame. Existe para o editor rodando em desktop
/// de desenvolvimento (a engine roda no celular, mas o editor precisa continuar utilizável enquanto
/// se desenvolve nele antes de haver um aparelho na mão) — CONVENCOES.md §1 não abre mão do celular
/// como alvo, mas isso não impede que o mouse seja um canal de input válido a mais.
/// </summary>
public readonly struct MouseState
{
    public readonly float2 Position;
    public readonly float2 Delta;
    public readonly float ScrollDelta;
    public readonly MouseButtons ButtonsDown;

    public MouseState(float2 position, float2 delta, float scrollDelta, MouseButtons buttonsDown)
    {
        Position = position;
        Delta = delta;
        ScrollDelta = scrollDelta;
        ButtonsDown = buttonsDown;
    }

    public static MouseState Empty => new(default, default, 0f, MouseButtons.None);
}
