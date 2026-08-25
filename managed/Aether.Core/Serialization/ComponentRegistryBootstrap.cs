namespace Aether.Serialization;

/// <summary>
/// Ponto único de registro dos componentes que já existem hoje na engine. Precisa ser chamado
/// explicitamente antes de qualquer serialização (<see cref="ComponentRegistry"/> começa vazio de
/// propósito — ver o comentário em <see cref="ComponentDescriptor"/> sobre por que não é reflexão
/// automática de assembly).
/// </summary>
public static class ComponentRegistryBootstrap
{
    private static bool s_registered;
    private static readonly object s_lock = new();

    /// <summary>
    /// Registra os componentes embutidos. Idempotente de propósito: cada teste que precisa do registro
    /// populado chama isto no início, e <see cref="ComponentRegistry.Register{T}"/> trata registro
    /// duplicado do MESMO tipo como erro — sem essa proteção aqui, a segunda chamada em qualquer
    /// processo (dois testes, ou um app que reinicializa um subsistema) explodiria.
    /// </summary>
    public static void RegisterBuiltins()
    {
        lock (s_lock)
        {
            if (s_registered) return;

            ComponentRegistry.Register<LocalTransform>("Aether.LocalTransform", schemaVersion: 1);
            ComponentRegistry.Register<WorldTransform>("Aether.WorldTransform", schemaVersion: 1);
            ComponentRegistry.Register<Parent>("Aether.Parent", schemaVersion: 1);
            ComponentRegistry.Register<FirstChild>("Aether.FirstChild", schemaVersion: 1);
            ComponentRegistry.Register<NextSibling>("Aether.NextSibling", schemaVersion: 1);

            s_registered = true;
        }
    }
}
