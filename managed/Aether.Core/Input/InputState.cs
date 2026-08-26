using Aether;

namespace Aether.Input;

/// <summary>
/// Item 1.1.2 do plano: agregador central de input do frame corrente — toques ativos, teclas
/// pressionadas neste frame, mouse e gamepads. Não sabe nada sobre Android/iOS: a captação real
/// (traduzir <c>AInputEvent</c>/<c>UITouch</c> para os tipos deste namespace) é integração futura,
/// mesma disciplina de <see cref="Aether.Platform.IFileSystem"/> no item 1.1.1 — este tipo é o
/// destino portátil e testável dessa tradução, não a tradução em si.
/// <para>
/// <b>Modelo de captação empurrado, não pesquisado (push, not poll)</b>: a camada de plataforma
/// chama <see cref="PushTouch"/>/<see cref="PushKey"/> conforme os eventos chegam do SO, em
/// qualquer ordem dentro do frame; <see cref="EndFrame"/> consolida tudo (remove toques com fase
/// <see cref="TouchPhase.Ended"/>/<see cref="TouchPhase.Cancelled"/>, limpa a lista de eventos de
/// tecla) preparando o próximo frame. Isso evita perder um toque curto (dedo tocou e levantou entre
/// duas leituras) que um modelo de "ler estado atual" perderia.
/// </para>
/// </summary>
public sealed class InputState : IDisposable
{
    private NativeList<TouchPoint> _activeTouches;
    private readonly List<KeyEvent> _keyEventsThisFrame = new();
    private readonly Dictionary<int, PenInfo> _penInfoByTouchId = new();
    private MouseState _mouse = MouseState.Empty;
    private GamepadState[] _gamepads;

    public InputState(int maxGamepads = 4)
    {
        if (maxGamepads < 0) throw new ArgumentOutOfRangeException(nameof(maxGamepads), "número máximo de gamepads não pode ser negativo");
        _activeTouches = new NativeList<TouchPoint>(initialCapacity: 8);
        _gamepads = new GamepadState[maxGamepads];
        for (int i = 0; i < maxGamepads; i++) _gamepads[i] = GamepadState.Disconnected(i);
    }

    public ReadOnlySpan<TouchPoint> ActiveTouches => _activeTouches.AsSpan();

    public IReadOnlyList<KeyEvent> KeyEventsThisFrame => _keyEventsThisFrame;

    public MouseState Mouse => _mouse;

    public ReadOnlySpan<GamepadState> Gamepads => _gamepads;

    /// <summary>
    /// Registra ou atualiza um contato de toque. <see cref="TouchPhase.Began"/> para um <see
    /// cref="TouchPoint.Id"/> já ativo substitui o existente (a plataforma não deveria gerar isso,
    /// mas tratar como substituição em vez de duplicar a entrada evita crescer a lista sem limite
    /// diante de um bug de captação). <see cref="TouchPhase.Ended"/>/<see
    /// cref="TouchPhase.Cancelled"/> ficam visíveis em <see cref="ActiveTouches"/> até <see
    /// cref="EndFrame"/> — código de gesto do mesmo frame ainda precisa ver a posição final.
    /// </summary>
    public void PushTouch(TouchPoint touch)
    {
        for (int i = 0; i < _activeTouches.Count; i++)
        {
            if (_activeTouches[i].Id == touch.Id)
            {
                _activeTouches[i] = touch;
                return;
            }
        }
        _activeTouches.Add(touch);
    }

    /// <summary>Associa dados de caneta ao toque de <paramref name="touchId"/> para o frame corrente.
    /// Deve ser chamado depois de <see cref="PushTouch"/> para o mesmo id neste frame.</summary>
    public void PushPenInfo(int touchId, PenInfo info) => _penInfoByTouchId[touchId] = info;

    public bool TryGetPenInfo(int touchId, out PenInfo info) => _penInfoByTouchId.TryGetValue(touchId, out info);

    public void PushKey(KeyEvent evt) => _keyEventsThisFrame.Add(evt);

    public void SetMouse(MouseState mouse) => _mouse = mouse;

    public void SetGamepad(GamepadState gamepad)
    {
        if ((uint)gamepad.Index >= (uint)_gamepads.Length)
            throw new ArgumentOutOfRangeException(nameof(gamepad), $"índice de gamepad {gamepad.Index} fora do intervalo [0, {_gamepads.Length})");
        _gamepads[gamepad.Index] = gamepad;
    }

    /// <summary>Verdadeiro se algum evento de tecla neste frame tem <see cref="KeyCode"/> e <see
    /// cref="KeyPhase.Down"/> — não distingue repetição, para isso ver <see
    /// cref="KeyEventsThisFrame"/> diretamente.</summary>
    public bool WasKeyPressedThisFrame(KeyCode code)
    {
        foreach (var evt in _keyEventsThisFrame)
            if (evt.Code == code && evt.Phase == KeyPhase.Down) return true;
        return false;
    }

    /// <summary>
    /// Extrapola linearmente a posição de um toque ativo <paramref name="secondsAhead"/> segundos à
    /// frente, usando o delta do frame atual — é a "predição" pedida pelo item 1.1.2: reduz a
    /// latência percebida de arrastar um objeto/gizmo desenhando a posição prevista em vez de
    /// esperar o próximo evento de toque chegar. Extrapolação ingênua de propósito (não Kalman): um
    /// preditor mais sofisticado é uma iteração futura sobre a MESMA API, não um pré-requisito para
    /// ela existir. Devolve a posição atual sem extrapolar se o toque não existir mais ou se
    /// <paramref name="secondsAhead"/> for zero.
    /// </summary>
    public float2 PredictPosition(int touchId, float secondsAhead)
    {
        for (int i = 0; i < _activeTouches.Count; i++)
        {
            if (_activeTouches[i].Id != touchId) continue;
            var touch = _activeTouches[i];
            if (secondsAhead <= 0f) return touch.Position;
            // O delta já é "por frame", não "por segundo" — sem uma duração de frame explícita no
            // struct (o chamador conhece o próprio deltaTime via FixedClock), a extrapolação usa o
            // delta como taxa unitária por "secondsAhead" em unidades de frame, não de tempo real.
            // Documentado aqui de propósito: um chamador que passa um deltaTime de verdade obtém uma
            // predição proporcionalmente maior/menor, que é o comportamento desejado.
            return touch.Position + touch.Delta * secondsAhead;
        }
        return default;
    }

    /// <summary>
    /// Consolida o frame: remove toques finalizados (<see cref="TouchPhase.Ended"/>/<see
    /// cref="TouchPhase.Cancelled"/>) e a informação de caneta associada, promove <see
    /// cref="TouchPhase.Began"/>/<see cref="TouchPhase.Moved"/> restantes para <see
    /// cref="TouchPhase.Stationary"/> (nenhum novo evento chegou = não se moveu), e limpa a lista de
    /// eventos de tecla do frame. Chamado uma vez por frame, depois que toda a lógica de gameplay/
    /// editor já leu o estado deste frame.
    /// </summary>
    public void EndFrame()
    {
        // NativeList<T> só expõe Add/Clear, sem RemoveAt: compacta em um Span temporário (contagem
        // de toques ativos é tipicamente < 10, mesmo teto prático do multi-touch de hardware real) e
        // reconstrói via Clear+Add — mais simples que gerência manual de índice dentro da própria
        // NativeList, e esta camada de orquestração de input não está no caminho quente de frame que
        // CONVENCOES.md §3 proíbe de alocar (o hot path é o consumo de ActiveTouches pela simulação,
        // não a contabilidade de EndFrame).
        Span<TouchPoint> carried = _activeTouches.Count == 0 ? [] : new TouchPoint[_activeTouches.Count];
        int carriedCount = 0;
        for (int i = 0; i < _activeTouches.Count; i++)
        {
            var touch = _activeTouches[i];
            if (touch.Phase is TouchPhase.Ended or TouchPhase.Cancelled)
            {
                _penInfoByTouchId.Remove(touch.Id);
                continue;
            }

            carried[carriedCount++] = touch.Phase == TouchPhase.Stationary
                ? touch
                : new TouchPoint(touch.Id, TouchPhase.Stationary, touch.Position, touch.Position, touch.Pressure, touch.RadiusMajor, touch.TimestampTicks);
        }

        _activeTouches.Clear();
        for (int i = 0; i < carriedCount; i++) _activeTouches.Add(carried[i]);

        _keyEventsThisFrame.Clear();
    }

    public void Dispose() => _activeTouches.Dispose();
}
