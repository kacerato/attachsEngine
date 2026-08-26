using Aether;

namespace Aether.Input;

[Flags]
public enum GamepadButtons : uint
{
    None = 0,
    South = 1u << 0,       // A (Xbox) / Cross (PlayStation)
    East = 1u << 1,        // B / Circle
    West = 1u << 2,        // X / Square
    North = 1u << 3,       // Y / Triangle
    LeftShoulder = 1u << 4,
    RightShoulder = 1u << 5,
    LeftStick = 1u << 6,   // clique do analógico esquerdo
    RightStick = 1u << 7,  // clique do analógico direito
    DPadUp = 1u << 8,
    DPadDown = 1u << 9,
    DPadLeft = 1u << 10,
    DPadRight = 1u << 11,
    Start = 1u << 12,
    Select = 1u << 13,
}

/// <summary>
/// Item 1.1.2 do plano: estado de um gamepad num frame. Struct blittable — plana de propósito (sem
/// array de botões) para caber inteira num registrador/cache line e não exigir alocação por
/// controle conectado (CONVENCOES.md §3, zero GC no caminho de frame).
/// </summary>
public readonly struct GamepadState
{
    /// <summary>Índice estável do controle enquanto permanece conectado (0-based). Reatribuído se o
    /// controle desconectar e reconectar — não é um handle geracional, porque a plataforma (Android/
    /// iOS) já garante esse índice não ser reciclado enquanto o controle está fisicamente conectado.</summary>
    public readonly int Index;

    public readonly bool IsConnected;

    /// <summary>Componentes em [-1, 1], já com deadzone aplicada pela camada de captação (não deste
    /// tipo — este struct é só o estado consumível, não onde a deadzone é decidida).</summary>
    public readonly float2 LeftStick;
    public readonly float2 RightStick;

    /// <summary>Gatilhos analógicos em [0, 1].</summary>
    public readonly float LeftTrigger;
    public readonly float RightTrigger;

    public readonly GamepadButtons ButtonsDown;

    public GamepadState(int index, bool isConnected, float2 leftStick, float2 rightStick, float leftTrigger, float rightTrigger, GamepadButtons buttonsDown)
    {
        Index = index;
        IsConnected = isConnected;
        LeftStick = leftStick;
        RightStick = rightStick;
        LeftTrigger = leftTrigger;
        RightTrigger = rightTrigger;
        ButtonsDown = buttonsDown;
    }

    public static GamepadState Disconnected(int index) => new(index, false, default, default, 0f, 0f, GamepadButtons.None);
}
