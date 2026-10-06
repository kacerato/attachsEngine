using System.Numerics;

namespace Astra;

/// <summary>Raio no mundo: origem e direção normalizada.</summary>
public readonly record struct Ray(Vector3 Origin, Vector3 Direction)
{
    public Vector3 GetPoint(float distance) => Origin + Direction * distance;
}

/// <summary>Retângulo em pixels da vista de jogo, origem no canto inferior esquerdo.</summary>
public readonly record struct ScreenRect(float X, float Y, float Width, float Height);

public enum GameViewPlatform : uint { Host = 0, Android = 1 }

/// <summary>Estado da vista de jogo no quadro corrente.</summary>
public readonly record struct GameViewState(float Width, float Height, float Dpi, bool SafeAreaReported,
    ScreenRect SafeArea, GameViewPlatform Platform, ulong CameraObjectId);

/// <summary>Família <c>astra.view</c>; host sem ela não implementa a interface.</summary>
public interface IGameViewAccess
{
    bool ReadGameView(out GameViewState state);
    bool ScreenRay(float x, float y, out Ray ray);
    bool WorldToScreen(Vector3 world, out Vector3 screen);
}
/// <summary>Família <c>astra.debug</c>.</summary>
public interface IDebugDrawAccess
{
    bool DrawLine(Vector3 from, Vector3 to, uint argb, float seconds);
}
/// <summary>Família <c>astra.haptics</c>; existe só com vibrador real.</summary>
public interface IHapticsAccess
{
    /// <summary>Falso quando a plataforma não oferece vibrador.</summary>
    bool HapticsAvailable { get; }
    bool Vibrate(uint milliseconds, float amplitude);
}

/// <summary>
/// A vista de jogo como no Unity 6000.0 <c>Screen</c> e <c>Camera</c>: pixels
/// com origem no canto inferior esquerdo, câmera = a que produziu o quadro
/// (a autorada de maior prioridade; sem ela, a do editor).
/// </summary>
public readonly struct GameViewAccess
{
    private readonly ISceneAccess _scene;
    internal GameViewAccess(ISceneAccess scene) => _scene = scene;
    private IGameViewAccess Access => _scene as IGameViewAccess
        ?? throw new NotSupportedException("O host não oferece a vista de jogo (astra.view).");
    public GameViewState State => Access.ReadGameView(out var state) ? state : throw new WorldException(_scene.LastStatus, "ler vista de jogo");
    public float Width => State.Width;
    public float Height => State.Height;
    /// <summary>DPI informado pelo sistema; zero quando desconhecido.</summary>
    public float Dpi => State.Dpi;
    /// <summary>Falso quando a plataforma não informou recortes; então não há área segura a afirmar.</summary>
    public bool TryGetSafeArea(out ScreenRect area)
    {
        var state = State;
        area = state.SafeArea;
        return state.SafeAreaReported;
    }
    public Ray ScreenPointToRay(Vector2 pixel) => Access.ScreenRay(pixel.X, pixel.Y, out var ray)
        ? ray : throw new WorldException(_scene.LastStatus, "raio da tela");
    /// <summary>Viewport 0..1, origem no canto inferior esquerdo.</summary>
    public Ray ViewportPointToRay(Vector2 viewport)
    {
        var state = State;
        return ScreenPointToRay(new Vector2(viewport.X * state.Width, viewport.Y * state.Height));
    }
    /// <summary>x,y em pixels; z = profundidade de vista em metros (negativa atrás da câmera).</summary>
    public Vector3 WorldToScreenPoint(Vector3 world) => Access.WorldToScreen(world, out var screen)
        ? screen : throw new WorldException(_scene.LastStatus, "projetar ponto");
    public Vector3 WorldToViewportPoint(Vector3 world)
    {
        var state = State;
        var screen = WorldToScreenPoint(world);
        return new(screen.X / state.Width, screen.Y / state.Height, screen.Z);
    }
}

/// <summary>Linhas de depuração do Play (Unity 6000.0 <c>Debug.DrawLine</c>/<c>DrawRay</c>); nunca persistidas.</summary>
public readonly struct DebugDraw
{
    private readonly ISceneAccess _scene;
    internal DebugDraw(ISceneAccess scene) => _scene = scene;
    private IDebugDrawAccess Access => _scene as IDebugDrawAccess
        ?? throw new NotSupportedException("O host não oferece linhas de depuração (astra.debug).");
    /// <summary>Duração zero mostra a linha no quadro corrente.</summary>
    public void DrawLine(Vector3 from, Vector3 to, Color? color = null, float duration = 0)
    {
        if (!Access.DrawLine(from, to, ToArgb(color ?? Color.White), duration))
            throw new WorldException(_scene.LastStatus, "desenhar linha");
    }
    public void DrawRay(Vector3 origin, Vector3 direction, Color? color = null, float duration = 0) =>
        DrawLine(origin, origin + direction, color, duration);
    // A sobreposição usa a cor da interface (sRGB 0xAARRGGBB); Color é linear.
    public static uint ToArgb(Color color)
    {
        static uint Channel(float linear)
        {
            linear = Math.Clamp(float.IsFinite(linear) ? linear : 0, 0, 1);
            var srgb = linear <= .0031308f ? linear * 12.92f : 1.055f * MathF.Pow(linear, 1 / 2.4f) - .055f;
            return (uint)MathF.Round(srgb * 255);
        }
        var alpha = (uint)MathF.Round(Math.Clamp(float.IsFinite(color.A) ? color.A : 0, 0, 1) * 255);
        return alpha << 24 | Channel(color.R) << 16 | Channel(color.G) << 8 | Channel(color.B);
    }
}

/// <summary>Vibração do aparelho (Unity 6000.0 <c>Handheld.Vibrate</c>, com duração e intensidade).</summary>
public readonly struct HapticsAccess
{
    private readonly ISceneAccess _scene;
    internal HapticsAccess(ISceneAccess scene) => _scene = scene;
    public bool Available => _scene is IHapticsAccess { HapticsAvailable: true };
    /// <summary>1..5000 ms; amplitude 0..1, zero usa a do aparelho. Sem vibrador lança NotSupportedException.</summary>
    public void Vibrate(uint milliseconds = 60, float amplitude = 0)
    {
        if (_scene is not IHapticsAccess { HapticsAvailable: true } access)
            throw new NotSupportedException("Este host não tem vibrador (astra.haptics ausente).");
        if (milliseconds is < 1 or > 5000) throw new ArgumentOutOfRangeException(nameof(milliseconds));
        if (!float.IsFinite(amplitude) || amplitude is < 0 or > 1) throw new ArgumentOutOfRangeException(nameof(amplitude));
        if (!access.Vibrate(milliseconds, amplitude)) throw new WorldException(_scene.LastStatus, "vibrar");
    }
}

/// <summary>Mudança de hierarquia aplicada no ponto seguro (família <c>astra.hierarchy</c>).</summary>
public readonly record struct HierarchyChange(ulong ObjectId, uint Generation, bool ParentChanged);
public interface IHierarchyChangeAccess
{
    int PollHierarchyChanges(Span<HierarchyChange> destination);
}
