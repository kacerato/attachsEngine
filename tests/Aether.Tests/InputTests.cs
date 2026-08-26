using Aether.Input;

namespace Aether.Tests;

/// <summary>
/// Item 1.1.2 do plano: camada portátil de input (toque multi-ponto com histórico/predição, caneta,
/// teclado, mouse, gamepad). Cobre <see cref="InputState"/> como agregador de frame — a captação real
/// de eventos do SO (Android/iOS) é integração futura, fora do escopo desta fatia (mesma disciplina
/// de <see cref="Aether.Platform.IFileSystem"/> no item 1.1.1).
/// </summary>
public static class InputTests
{
    private static TouchPoint NovoToque(int id, TouchPhase phase, float x, float y, float prevX, float prevY, float pressure = 1f)
        => new(id, phase, new float2(x, y), new float2(prevX, prevY), pressure, radiusMajor: 0f, timestampTicks: 0);

    // ---------- Ciclo de vida de toque ----------

    [Test] public static void PushTouch_Began_AparecemEmActiveTouches()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Began, 10, 20, 10, 20));

        Assert.Equal(1, input.ActiveTouches.Length, what: "um toque Began deve aparecer na lista de ativos");
        Assert.Equal(TouchPhase.Began, input.ActiveTouches[0].Phase, what: "fase deve ser preservada");
    }

    [Test] public static void PushTouch_MesmoIdDuasVezes_AtualizaEmVezDeDuplicar()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Began, 10, 20, 10, 20));
        input.PushTouch(NovoToque(1, TouchPhase.Moved, 15, 25, 10, 20));

        Assert.Equal(1, input.ActiveTouches.Length, what: "mesmo Id deve substituir, não duplicar a entrada");
        Assert.Close(15f, input.ActiveTouches[0].Position.X, what: "posição deve refletir a atualização mais recente");
    }

    [Test] public static void PushTouch_DoisIdsDiferentes_AmbosFicamAtivosIndependentemente()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Began, 10, 20, 10, 20));
        input.PushTouch(NovoToque(2, TouchPhase.Began, 100, 200, 100, 200));

        Assert.Equal(2, input.ActiveTouches.Length, what: "toques com Id diferentes são contatos multi-touch independentes");
    }

    [Test] public static void EndFrame_ToqueEnded_EhRemovidoDeActiveTouches()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Began, 10, 20, 10, 20));
        input.EndFrame();
        input.PushTouch(NovoToque(1, TouchPhase.Ended, 12, 22, 10, 20));
        input.EndFrame();

        Assert.Equal(0, input.ActiveTouches.Length, what: "toque finalizado não deve sobreviver ao EndFrame que o processou");
    }

    [Test] public static void EndFrame_ToqueCancelled_EhRemovidoDeActiveTouches()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Began, 10, 20, 10, 20));
        input.EndFrame();
        input.PushTouch(NovoToque(1, TouchPhase.Cancelled, 12, 22, 10, 20));
        input.EndFrame();

        Assert.Equal(0, input.ActiveTouches.Length, what: "toque cancelado pela plataforma deve ser removido, mesma disciplina de Ended");
    }

    [Test] public static void EndFrame_ToqueSemNovoEventoNoFrame_VirarStationary()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Began, 10, 20, 10, 20));
        input.EndFrame(); // nenhum novo evento chega no frame seguinte

        Assert.Equal(TouchPhase.Stationary, input.ActiveTouches[0].Phase,
            what: "toque presente sem novo evento no frame deve virar Stationary, não continuar Began");
    }

    [Test] public static void EndFrame_MultiplosToquesComAlgunsFinalizados_PreservaOsRestantesNaOrdemCerta()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Began, 1, 1, 1, 1));
        input.PushTouch(NovoToque(2, TouchPhase.Began, 2, 2, 2, 2));
        input.PushTouch(NovoToque(3, TouchPhase.Began, 3, 3, 3, 3));
        input.EndFrame();

        input.PushTouch(NovoToque(2, TouchPhase.Ended, 2, 2, 2, 2));
        input.EndFrame();

        Assert.Equal(2, input.ActiveTouches.Length, what: "só o toque 2 foi finalizado, os outros dois permanecem");
        Assert.True(input.ActiveTouches[0].Id == 1 && input.ActiveTouches[1].Id == 3 ||
                    input.ActiveTouches[0].Id == 3 && input.ActiveTouches[1].Id == 1,
            what: "toques 1 e 3 devem sobreviver à remoção do toque 2 do meio da lista");
    }

    [Test] public static void TouchDelta_ApósMoved_ReflecteDiferencaDePosicao()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Moved, 15, 25, 10, 20));

        var delta = input.ActiveTouches[0].Delta;
        Assert.Close(5f, delta.X, what: "delta X = posição atual - posição anterior");
        Assert.Close(5f, delta.Y, what: "delta Y = posição atual - posição anterior");
    }

    // ---------- Predição ----------

    [Test] public static void PredictPosition_ComDeltaConstante_ExtrapolaLinearmente()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Moved, 20, 0, 10, 0)); // delta = (10, 0)

        var previsto = input.PredictPosition(1, secondsAhead: 2f);
        Assert.Close(40f, previsto.X, what: "posição atual (20) + delta (10) * secondsAhead (2) = 40");
    }

    [Test] public static void PredictPosition_SecondsAheadZero_DevolvePosicaoAtualSemExtrapolar()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Moved, 20, 0, 10, 0));

        var previsto = input.PredictPosition(1, secondsAhead: 0f);
        Assert.Close(20f, previsto.X, what: "sem horizonte de predição, devolve a posição atual exata");
    }

    [Test] public static void PredictPosition_ToqueInexistente_DevolvePadraoSemLancar()
    {
        using var input = new InputState();
        var previsto = input.PredictPosition(touchId: 999, secondsAhead: 1f);
        Assert.Close(0f, previsto.X, what: "toque inexistente não deve lançar, devolve default(float2)");
    }

    // ---------- Caneta ----------

    [Test] public static void PushPenInfo_DepoisTryGet_DevolveOsMesmosDados()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Began, 10, 10, 10, 10));
        input.PushPenInfo(1, new PenInfo(pressure: 0.75f, tiltX: 0.1f, tiltY: 0.2f, rotation: 0f, barrelButtonDown: true, isHovering: false));

        Assert.True(input.TryGetPenInfo(1, out var info), what: "informação de caneta associada ao toque deve ser recuperável");
        Assert.Close(0.75f, info.Pressure, what: "pressão da caneta deve ser preservada");
        Assert.True(info.BarrelButtonDown, what: "estado do botão lateral deve ser preservado");
    }

    [Test] public static void TryGetPenInfo_ToqueSemCaneta_DevolveFalso()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Began, 10, 10, 10, 10));
        Assert.False(input.TryGetPenInfo(1, out _), what: "toque de dedo comum não tem PenInfo associado");
    }

    [Test] public static void EndFrame_ToqueFinalizado_RemovePenInfoAssociado()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Began, 10, 10, 10, 10));
        input.PushPenInfo(1, new PenInfo(1f, 0f, 0f, 0f, false, false));
        input.EndFrame();

        input.PushTouch(NovoToque(1, TouchPhase.Ended, 10, 10, 10, 10));
        input.EndFrame();

        Assert.False(input.TryGetPenInfo(1, out _), what: "PenInfo não pode sobreviver ao Id do toque ser reciclado para outro contato depois");
    }

    // ---------- Teclado ----------

    [Test] public static void PushKey_Down_AparecemEmKeyEventsThisFrame()
    {
        using var input = new InputState();
        input.PushKey(new KeyEvent(KeyCode.Z, KeyPhase.Down, isRepeat: false, timestampTicks: 0));

        Assert.Equal(1, input.KeyEventsThisFrame.Count, what: "evento de tecla deve aparecer na lista do frame");
        Assert.True(input.WasKeyPressedThisFrame(KeyCode.Z), what: "consulta de conveniência deve encontrar a tecla pressionada");
    }

    [Test] public static void WasKeyPressedThisFrame_TeclaNaoPressionada_DevolveFalso()
    {
        using var input = new InputState();
        input.PushKey(new KeyEvent(KeyCode.A, KeyPhase.Down, false, 0));
        Assert.False(input.WasKeyPressedThisFrame(KeyCode.B), what: "só a tecla efetivamente pressionada deve retornar verdadeiro");
    }

    [Test] public static void EndFrame_LimpaKeyEventsDoFrameAnterior()
    {
        using var input = new InputState();
        input.PushKey(new KeyEvent(KeyCode.A, KeyPhase.Down, false, 0));
        input.EndFrame();

        Assert.Equal(0, input.KeyEventsThisFrame.Count, what: "eventos de tecla são por-frame, não devem persistir após EndFrame");
    }

    [Test] public static void PushKey_FaseUp_NaoContaComoPressed()
    {
        using var input = new InputState();
        input.PushKey(new KeyEvent(KeyCode.A, KeyPhase.Up, false, 0));
        Assert.False(input.WasKeyPressedThisFrame(KeyCode.A), what: "WasKeyPressedThisFrame só deve considerar fase Down");
    }

    // ---------- Mouse ----------

    [Test] public static void SetMouse_DepoisMouse_DevolveOMesmoEstado()
    {
        using var input = new InputState();
        var estado = new MouseState(new float2(5, 6), new float2(1, 0), scrollDelta: -1f, MouseButtons.Left | MouseButtons.Right);
        input.SetMouse(estado);

        Assert.Close(5f, input.Mouse.Position.X, what: "posição do mouse deve ser preservada");
        Assert.True((input.Mouse.ButtonsDown & MouseButtons.Left) != 0, what: "flag de botão esquerdo deve ser preservada");
        Assert.True((input.Mouse.ButtonsDown & MouseButtons.Right) != 0, what: "flag de botão direito deve ser preservada");
    }

    [Test] public static void Mouse_EstadoInicial_EhEmptySemBotoes()
    {
        using var input = new InputState();
        Assert.Equal(MouseButtons.None, input.Mouse.ButtonsDown, what: "antes de qualquer SetMouse, nenhum botão deve estar pressionado");
    }

    // ---------- Gamepad ----------

    [Test] public static void SetGamepad_IndiceValido_AtualizaOEstadoNaquelePodio()
    {
        using var input = new InputState(maxGamepads: 2);
        var estado = new GamepadState(0, isConnected: true, new float2(0.5f, -0.5f), default, 0.2f, 0.8f, GamepadButtons.South);
        input.SetGamepad(estado);

        Assert.True(input.Gamepads[0].IsConnected, what: "gamepad atualizado deve refletir conectado");
        Assert.Close(0.5f, input.Gamepads[0].LeftStick.X, what: "componente do stick deve ser preservado");
        Assert.True((input.Gamepads[0].ButtonsDown & GamepadButtons.South) != 0, what: "flag de botão deve ser preservada");
    }

    [Test] public static void SetGamepad_IndiceForaDoIntervalo_Lanca()
    {
        using var input = new InputState(maxGamepads: 1);
        Assert.Throws<ArgumentOutOfRangeException>(() => input.SetGamepad(new GamepadState(5, true, default, default, 0, 0, GamepadButtons.None)),
            what: "índice fora do número máximo de gamepads configurado deve ser rejeitado, não crescer silenciosamente");
    }

    [Test] public static void Gamepads_EstadoInicial_TodosDesconectados()
    {
        using var input = new InputState(maxGamepads: 4);
        foreach (var gamepad in input.Gamepads)
            Assert.False(gamepad.IsConnected, what: "nenhum gamepad deve começar conectado antes de qualquer SetGamepad");
    }

    [Test] public static void Constructor_MaxGamepadsNegativo_Lanca()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => new InputState(maxGamepads: -1),
            what: "número máximo de gamepads negativo não faz sentido");
    }

    // ---------- Zero alocação no consumo do frame ----------

    [Test] public static void ActiveTouches_LeituraRepetida_NaoAloca()
    {
        using var input = new InputState();
        input.PushTouch(NovoToque(1, TouchPhase.Began, 1, 1, 1, 1));
        input.PushTouch(NovoToque(2, TouchPhase.Began, 2, 2, 2, 2));

        Assert.NoAlloc(() =>
        {
            var touches = input.ActiveTouches;
            float sum = 0;
            for (int i = 0; i < touches.Length; i++) sum += touches[i].Position.X;
        }, what: "ler o span de toques ativos é o caminho quente de todo frame com input — não pode alocar");
    }
}
