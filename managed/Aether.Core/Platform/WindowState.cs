namespace Aether.Platform;

/// <summary>
/// Item 1.1.4 do plano: agregador central de janela/display, mesmo papel de <see
/// cref="Aether.Input.InputState"/> para input — modelo push (a captação real de plataforma chama
/// <see cref="SetPrimaryDisplay"/>/<see cref="AddOrUpdateExternalDisplay"/>/<see
/// cref="RemoveDisplay"/> conforme eventos chegam do SO), sem este tipo saber nada de Android/iOS.
/// <para>
/// "Multi-janela" aqui significa múltiplos displays conectados simultaneamente ao mesmo processo
/// (ex.: um display externo via USB-C/cast), não múltiplas instâncias de app em split-screen — essa
/// segunda forma de multi-janela é decisão de lifecycle de processo (item 1.1.3), fora do escopo
/// deste tipo.
/// </para>
/// </summary>
public sealed class WindowState
{
    private DisplayInfo _primary;
    private readonly Dictionary<int, DisplayInfo> _externalDisplays = new();

    public WindowState(DisplayInfo primary)
    {
        if (!primary.IsBuiltIn) throw new ArgumentException("o display primário deve ter IsBuiltIn = true", nameof(primary));
        _primary = primary;
    }

    public DisplayInfo Primary => _primary;

    public IReadOnlyCollection<DisplayInfo> ExternalDisplays => _externalDisplays.Values;

    public bool HasExternalDisplay => _externalDisplays.Count > 0;

    /// <summary>Atualiza o display principal — chamado a cada mudança de configuração relatada pela
    /// plataforma (rotação, taxa de atualização adaptativa mudando, o aparelho entrando/saindo de
    /// modo de economia que reduz Hz). Lança se <paramref name="display"/> não tiver o mesmo <see
    /// cref="DisplayInfo.Id"/> do principal atual — trocar de display principal por completo não é
    /// uma "atualização", é um cenário que esta API não modela (o display embutido de um aparelho
    /// físico não troca de identidade em tempo de execução).</summary>
    public void SetPrimaryDisplay(DisplayInfo display)
    {
        if (!display.IsBuiltIn) throw new ArgumentException("display primário deve ter IsBuiltIn = true", nameof(display));
        if (display.Id != _primary.Id)
            throw new ArgumentException($"Id do display primário não pode mudar em tempo de execução (era {_primary.Id}, veio {display.Id}).", nameof(display));
        _primary = display;
    }

    /// <summary>Registra um display externo recém-conectado, ou atualiza um já conhecido (mesmo <see
    /// cref="DisplayInfo.Id"/>) — ex.: sua resolução/taxa mudou porque o cabo foi trocado.</summary>
    public void AddOrUpdateExternalDisplay(DisplayInfo display)
    {
        if (display.IsBuiltIn) throw new ArgumentException("display externo não pode ter IsBuiltIn = true — isso é o papel de SetPrimaryDisplay", nameof(display));
        _externalDisplays[display.Id] = display;
    }

    /// <summary>Remove um display externo desconectado. Não-erro se o Id já não estiver presente —
    /// mesma disciplina idempotente de <see cref="IFileSystem.DeleteFile"/>.</summary>
    public void RemoveDisplay(int id) => _externalDisplays.Remove(id);

    public bool TryGetExternalDisplay(int id, out DisplayInfo display) => _externalDisplays.TryGetValue(id, out display);
}
