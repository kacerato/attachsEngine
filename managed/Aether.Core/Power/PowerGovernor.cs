namespace Aether.Power;

/// <summary>
/// Estado térmico do aparelho, do melhor para o pior. A ordem importa: usada
/// para decidir se uma leitura nova é uma piora (adota na hora) ou uma melhora
/// (só adota depois de se sustentar — ver histerese em <see cref="PowerGovernor"/>).
/// </summary>
public enum ThermalState { Nominal, Fair, Moderate, Severe, Critical }

/// <summary>Como o viewport do editor deve ser redesenhado, do mais caro ao mais barato.</summary>
public enum ViewportRedrawMode { Continuous, OnDemand, Static }

/// <summary>Fonte de estado térmico. Interface para poder ser falsificada em teste.</summary>
public interface IThermalSource
{
    ThermalState Sample();
}

/// <summary>Fonte nula: sempre relata estado nominal. Útil como padrão em plataformas sem sensor.</summary>
public sealed class NullThermalSource : IThermalSource
{
    public ThermalState Sample() => ThermalState.Nominal;
}

/// <summary>
/// Perfil de energia imutável: o conjunto de concessões de qualidade que o
/// <see cref="PowerGovernor"/> decidiu para o estado atual do aparelho.
/// </summary>
public readonly record struct PowerProfile(
    int FrameRateCap,
    float ResolutionScale,
    float ShadowScale,
    float GiUpdateBudget,
    bool EnableScreenSpaceReflections,
    ViewportRedrawMode ViewportRedrawMode);

/// <summary>
/// Implementa a tabela térmica x perfil de energia (plano, parte 3.4). Entrada:
/// estado térmico (via <see cref="IThermalSource"/>), nível de bateria e se está
/// carregando. Saída: um <see cref="PowerProfile"/> imutável.
///
/// Transições têm histerese: piorar (esquentar) é adotado na hora, porque
/// proteção térmica não pode esperar; melhorar (esfriar) só é adotado depois de
/// se sustentar por <see cref="UpgradeStreak"/> amostras seguidas, senão a
/// imagem fica oscilando de qualidade a cada leitura de sensor que tremula perto
/// do limiar.
/// </summary>
public sealed class PowerGovernor
{
    private const int UpgradeStreak = 5;

    private const float BatteryLowFraction = 0.2f;
    private const float BatteryCriticalFraction = 0.1f;

    private readonly IThermalSource _thermalSource;
    private ThermalState _effectiveState = ThermalState.Nominal;
    private int _betterStreak;

    public PowerGovernor(IThermalSource? thermalSource = null)
    {
        _thermalSource = thermalSource ?? new NullThermalSource();
        CurrentProfile = ProfileFor(_effectiveState);
    }

    public PowerProfile CurrentProfile { get; private set; }

    /// <summary>Amostra a fonte térmica, aplica histerese e devolve o perfil resultante (já considerando a bateria).</summary>
    public PowerProfile Update(float batteryLevel, bool isCharging)
    {
        ApplyHysteresis(_thermalSource.Sample());
        CurrentProfile = ApplyBatteryPolicy(ProfileFor(_effectiveState), batteryLevel, isCharging);
        return CurrentProfile;
    }

    private void ApplyHysteresis(ThermalState sampled)
    {
        if (sampled >= _effectiveState)
        {
            // Igual ou pior: adota na hora. Não dá pra esperar confirmação quando o
            // aparelho está esquentando.
            _effectiveState = sampled;
            _betterStreak = 0;
        }
        else
        {
            // Melhora: só sobe de qualidade depois de ver isso se sustentar, senão a
            // imagem oscila a cada leitura de sensor que tremula perto do limiar.
            _betterStreak++;
            if (_betterStreak >= UpgradeStreak)
            {
                _effectiveState = sampled;
                _betterStreak = 0;
            }
        }
    }

    private static PowerProfile ProfileFor(ThermalState state) => state switch
    {
        ThermalState.Nominal  => new PowerProfile(60, 1.00f, 1.00f, 1.00f, true,  ViewportRedrawMode.Continuous),
        ThermalState.Fair     => new PowerProfile(60, 0.90f, 0.85f, 0.75f, true,  ViewportRedrawMode.Continuous),
        ThermalState.Moderate => new PowerProfile(45, 0.75f, 0.60f, 0.50f, false, ViewportRedrawMode.Continuous),
        ThermalState.Severe   => new PowerProfile(30, 0.60f, 0.40f, 0.25f, false, ViewportRedrawMode.OnDemand),
        ThermalState.Critical => new PowerProfile(15, 0.50f, 0.25f, 0.00f, false, ViewportRedrawMode.Static),
        _ => throw new ArgumentOutOfRangeException(nameof(state)),
    };

    private static PowerProfile ApplyBatteryPolicy(PowerProfile profile, float batteryLevel, bool isCharging)
    {
        if (isCharging) return profile;

        // Bateria baixa sem carregador reduz o teto de fps, independente do estado
        // térmico — economizar energia importa mesmo com o aparelho frio.
        if (batteryLevel < BatteryCriticalFraction)
            return profile with { FrameRateCap = Math.Min(profile.FrameRateCap, 15) };
        if (batteryLevel < BatteryLowFraction)
            return profile with { FrameRateCap = Math.Min(profile.FrameRateCap, 30) };
        return profile;
    }
}
