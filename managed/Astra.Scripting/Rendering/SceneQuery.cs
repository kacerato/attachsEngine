using Astra.Components;

namespace Astra;

/// <summary>
/// Busca de componentes no mundo de Play inteiro, como
/// <c>Object.FindFirstObjectByType&lt;T&gt;()</c> e <c>FindObjectsByType&lt;T&gt;()</c>
/// da Unity. Percorre a hierarquia em pré-ordem a partir da raiz, só objetos
/// ativos; é O(N) e serve para inicialização ou para os acessos estáticos
/// (<see cref="Astra.Rendering.RenderSettings"/>, <c>TimeOfDay.Current</c>), não
/// para todo quadro.
/// </summary>
public static class SceneQuery
{
    private static ISceneAccess? _scene;
    private static ulong _anchor;
    private static uint _world;

    // Qualquer objeto vivo serve de âncora: a raiz sai subindo pelos pais.
    internal static void Bind(ISceneAccess scene, ulong anchor) { _scene = scene; _anchor = anchor; _world = scene.WorldId; }
    internal static void Unbind() { _scene = null; _anchor = 0; _world = 0; }

    private static ISceneAccess Scene => _scene is not null && _scene.WorldId == _world
        ? _scene : throw new WorldException(WorldStatus.NotRunning, "buscar no mundo");

    /// <summary>A raiz do mundo de Play (sem componentes próprios).</summary>
    public static GameObject Root
    {
        get
        {
            var scene = Scene;
            if (_anchor == 0 || !scene.Exists(_anchor)) throw new WorldException(WorldStatus.UnknownObject, "achar a raiz do mundo");
            var current = GameObject.Resolve(scene, _anchor);
            while (current.Parent is { } parent) current = parent;
            return current;
        }
    }

    /// <summary>Todas as instâncias ativas do tipo de componente, em pré-ordem.</summary>
    public static IEnumerable<Component> FindComponents(string typeId)
    {
        ArgumentException.ThrowIfNullOrEmpty(typeId);
        var pending = new Stack<GameObject>();
        Root.PushAliveChildren(pending);
        while (pending.TryPop(out var current))
        {
            if (!current.IsAlive || !current.ActiveInHierarchy) continue;
            for (var ordinal = 0; current.GetComponent(typeId, ordinal) is { } component; ++ordinal) yield return component;
            current.PushAliveChildren(pending);
        }
    }

    public static T? FindFirstObjectByType<T>() where T : struct, IComponentFacade<T>
    {
        foreach (var component in FindComponents(T.TypeId)) return T.Wrap(component);
        return null;
    }

    public static T[] FindObjectsByType<T>() where T : struct, IComponentFacade<T> =>
        FindComponents(T.TypeId).Select(T.Wrap).ToArray();
}
