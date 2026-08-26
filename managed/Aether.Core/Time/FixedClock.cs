namespace Aether;

/// <summary>
/// Relógio de fixed step (item 1.5.2 do plano): converte o delta de tempo real do frame numa
/// quantidade determinística de passos de simulação de duração fixa, mais a fração de
/// interpolação entre o penúltimo e o último estado simulado — o padrão "fix your timestep"
/// que desacopla física/gameplay (determinístico) da taxa de apresentação (variável).
/// <para>
/// Por que não é o <see cref="System.Diagnostics.Stopwatch"/> ou <c>DateTime</c> direto: o
/// chamador (shell Android/host de teste) é quem mede o tempo real e entrega o delta via
/// <see cref="Advance"/> — este tipo nunca lê o relógio do sistema, para poder ser avançado por
/// valores sintéticos em teste (sem `Thread.Sleep`) e para não presumir uma fonte de tempo
/// específica de plataforma.
/// </para>
/// </summary>
public struct FixedClock
{
    /// <summary>Duração de cada passo de simulação, em segundos. Nunca muda depois de criado —
    /// alterar o fixed step em runtime invalidaria replays/determinismo gravados.</summary>
    public readonly float FixedDeltaTime;

    /// <summary>
    /// Teto de passos executados numa única chamada a <see cref="Advance"/>. Existe para evitar a
    /// "espiral da morte": se o frame anterior demorou muito (troca de cena, GC pause, debugger
    /// pausado), sem teto o acumulador tentaria simular todo esse atraso de uma vez, o que demora
    /// ainda mais, gerando um delta real maior no próximo frame — um ciclo que nunca se recupera.
    /// Passos além do teto são descartados do acumulador (tempo perdido, não simulado), não
    /// executados enfileirados; <see cref="DroppedSteps"/> conta quantas vezes isso já aconteceu.
    /// </summary>
    public readonly int MaxStepsPerAdvance;

    /// <summary>Escala de tempo aplicada ao delta real antes de acumular — 1 = normal, 0 = tempo
    /// parado (mas distinto de <see cref="Paused"/>: escala é gameplay, ex. slow-motion; pausa é
    /// edição, ex. o Editor parado no debugger). 2 = duas vezes mais rápido. Negativo não é válido
    /// (o acumulador não modela tempo andando para trás).</summary>
    public float TimeScale;

    /// <summary>Enquanto verdadeiro, <see cref="Advance"/> não acumula tempo nem produz passos —
    /// distinto de <see cref="TimeScale"/> = 0 porque pausa é um requisito do plano (item 1.5.2)
    /// separado de escala, e um chamador pode querer pausar preservando a escala configurada para
    /// quando retomar.</summary>
    public bool Paused;

    private float _accumulator;

    /// <summary>Quantas vezes <see cref="Advance"/> precisou descartar tempo acumulado por
    /// exceder <see cref="MaxStepsPerAdvance"/>. Sobe para diagnóstico; nunca reseta sozinho.</summary>
    public int DroppedSteps { get; private set; }

    /// <summary>Fração entre o penúltimo e o último estado fixo simulado (0 = penúltimo, 1 =
    /// último), para o sistema de render interpolar posição/rotação em vez de saltar entre
    /// estados discretos. Atualizada a cada <see cref="Advance"/>, mesmo quando zero passos
    /// rodam (acumulador ainda não bateu o fixed step).</summary>
    public readonly float InterpolationAlpha => _accumulator / FixedDeltaTime;

    public FixedClock(float fixedDeltaTime, int maxStepsPerAdvance = 5)
    {
        if (!float.IsFinite(fixedDeltaTime) || fixedDeltaTime <= 0f)
            throw new ArgumentOutOfRangeException(nameof(fixedDeltaTime), "O fixed delta time precisa ser finito e positivo.");
        if (maxStepsPerAdvance <= 0)
            throw new ArgumentOutOfRangeException(nameof(maxStepsPerAdvance), "É preciso permitir ao menos 1 passo por Advance.");

        FixedDeltaTime = fixedDeltaTime;
        MaxStepsPerAdvance = maxStepsPerAdvance;
        TimeScale = 1f;
        Paused = false;
        _accumulator = 0f;
        DroppedSteps = 0;
    }

    /// <summary>
    /// Consome <paramref name="realDeltaTime"/> (tempo de frame real, não escalado) e devolve
    /// quantos passos de <see cref="FixedDeltaTime"/> o chamador deve simular agora, chamando
    /// <see cref="PhysicsSyncSystem"/>/sistemas de gameplay exatamente esse número de vezes antes
    /// do próximo frame de render.
    /// </summary>
    public int Advance(float realDeltaTime)
    {
        if (!float.IsFinite(realDeltaTime) || realDeltaTime < 0f)
            throw new ArgumentOutOfRangeException(nameof(realDeltaTime), "O delta real de frame precisa ser finito e não-negativo.");
        if (Paused) return 0;

        _accumulator += realDeltaTime * MathF.Max(TimeScale, 0f);

        int steps = (int)(_accumulator / FixedDeltaTime);
        if (steps > MaxStepsPerAdvance)
        {
            // Descarta o excedente do acumulador junto com os passos não executados — manter o
            // tempo extra no acumulador só adiaria a espiral para o próximo Advance.
            int discardedSteps = steps - MaxStepsPerAdvance;
            _accumulator -= discardedSteps * FixedDeltaTime;
            DroppedSteps += discardedSteps;
            steps = MaxStepsPerAdvance;
        }

        _accumulator -= steps * FixedDeltaTime;
        return steps;
    }

    /// <summary>Zera o acumulador sem alterar contadores de diagnóstico — usar ao retomar de uma
    /// pausa longa ou trocar de cena, para o próximo <see cref="Advance"/> não tentar recuperar
    /// tempo que não faz mais sentido simular (ex.: o app ficou em background por minutos).</summary>
    public void ResetAccumulator() => _accumulator = 0f;
}
