namespace Aether.Design;

/// <summary>
/// Item 0.3.4 do plano ("hápticos" na definição do sistema de design): vocabulário nomeado de
/// feedback háptico — qual <em>intenção</em> de interação corresponde a qual pulso, não a API real
/// de vibração de nenhuma plataforma. A integração com <c>Vibrator</c> (Android) ou
/// <c>UIFeedbackGenerator</c> (iOS) é trabalho futuro do item 3.1.4 ("Feedback háptico com
/// vocabulário definido"); esta fatia entrega o vocabulário em si, para que a UI (item 3.1.x) e a
/// manipulação de gizmo (item 3.3.x, citada no plano com "feedback háptico no descolar"/"snap...com
/// háptico") tenham um contrato comum a chamar, mesmo antes de existir a integração de plataforma
/// por trás.
/// </summary>
public enum HapticCue
{
    /// <summary>Um objeto foi selecionado no viewport — pulso curto e leve.</summary>
    Selection,

    /// <summary>Um gizmo/objeto encaixou numa grade, vértice, superfície ou ângulo (plano §8.4,
    /// item 3.3.5) — pulso distinto de <see cref="Selection"/>, confirmação tátil de precisão
    /// atingida.</summary>
    Snap,

    /// <summary>Uma ação de duplicar "descolou" do objeto original (plano §8.4: "Duplicar com
    /// feedback háptico no descolar").</summary>
    DragDetach,

    /// <summary>Uma ação destrutiva ou inválida foi rejeitada — pulso mais forte, distinto dos
    /// pulsos de sucesso, para que o usuário sinta a diferença sem olhar a tela.</summary>
    Error,

    /// <summary>Uma ação foi confirmada com sucesso (ex.: salvar, aplicar uma mudança de lote).</summary>
    Confirmation,

    /// <summary>O menu radial abriu (plano §8.5) — pulso de reconhecimento de que o toque-e-segurar
    /// foi registrado.</summary>
    RadialMenuOpen,
}

/// <summary>Intensidade relativa do pulso — a tradução exata para duração/amplitude de motor é
/// responsabilidade da integração de plataforma (fora desta fatia); aqui é só a ordenação relativa
/// que todo <see cref="HapticCue"/> precisa ter.</summary>
public enum HapticIntensity
{
    Light,
    Medium,
    Strong,
}

/// <summary>
/// Mapeia cada <see cref="HapticCue"/> para sua <see cref="HapticIntensity"/> — o "vocabulário
/// definido" citado pelo item 3.1.4. Central e imutável de propósito: duas partes diferentes da UI
/// nunca devem escolher intensidades diferentes para a mesma intenção de interação, isso quebraria
/// a consistência tátil que a UX do plano depende (plano §8.4, "recursos anti-oclusão" e a
/// linguagem de gestos completa contam com feedback consistente).
/// </summary>
public static class HapticVocabulary
{
    private static readonly Dictionary<HapticCue, HapticIntensity> Map = new()
    {
        [HapticCue.Selection] = HapticIntensity.Light,
        [HapticCue.Snap] = HapticIntensity.Medium,
        [HapticCue.DragDetach] = HapticIntensity.Medium,
        [HapticCue.Error] = HapticIntensity.Strong,
        [HapticCue.Confirmation] = HapticIntensity.Medium,
        [HapticCue.RadialMenuOpen] = HapticIntensity.Light,
    };

    /// <summary>Devolve a intensidade definida para <paramref name="cue"/>. Todo valor do enum
    /// <see cref="HapticCue"/> tem entrada garantida — coberto por teste de regressão
    /// (<c>HapticVocabulary_TodoHapticCue_TemIntensidadeDefinida</c>), para que adicionar um novo
    /// <see cref="HapticCue"/> sem mapear sua intensidade quebre a suíte, não falhe silenciosamente
    /// em runtime.</summary>
    public static HapticIntensity IntensityFor(HapticCue cue) => Map[cue];
}
