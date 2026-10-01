namespace Astra.Components;

/// <summary>Queries over real native component instances; ordinal order is preserved.
/// Unity 6000.0 GetComponentsInChildren/GetComponentsInParent are the reference contracts.</summary>
public static class ComponentQueries
{
    public static T[] GetComponents<T>(this GameObject owner) where T : struct, IComponentFacade<T>
    {
        ArgumentNullException.ThrowIfNull(owner);
        return owner.Components().Where(c => c.TypeId == T.TypeId).Select(T.Wrap).ToArray();
    }

    /// <summary>Writes into caller-owned storage without allocating a result array.</summary>
    public static void GetComponents<T>(this GameObject owner, List<T> results) where T : struct, IComponentFacade<T>
    {
        ArgumentNullException.ThrowIfNull(owner); ArgumentNullException.ThrowIfNull(results);
        results.Clear(); Append(owner, results);
    }

    public static T[] GetComponentsInChildren<T>(this GameObject owner, bool includeInactive = false)
        where T : struct, IComponentFacade<T>
    {
        var results = new List<T>(); owner.GetComponentsInChildren(includeInactive, results); return results.ToArray();
    }

    /// <summary>Depth-first, self first, child order preserved. Self is searched even when inactive;
    /// inactive descendants are excluded by default. Disabled components remain discoverable.</summary>
    public static void GetComponentsInChildren<T>(this GameObject owner, bool includeInactive, List<T> results)
        where T : struct, IComponentFacade<T>
    {
        ArgumentNullException.ThrowIfNull(owner); ArgumentNullException.ThrowIfNull(results);
        results.Clear();
        var pending = new Stack<GameObject>(); var seen = new HashSet<ulong>(); pending.Push(owner);
        while (pending.TryPop(out var node))
        {
            if (!seen.Add(node.ObjectId)) throw new InvalidOperationException("Cycle in component hierarchy.");
            if (node != owner && !includeInactive && !node.ActiveInHierarchy) continue;
            Append(node, results);
            node.PushAliveChildren(pending);
        }
    }

    public static T[] GetComponentsInParent<T>(this GameObject owner, bool includeInactive = false)
        where T : struct, IComponentFacade<T>
    {
        ArgumentNullException.ThrowIfNull(owner);
        var results = new List<T>(); var seen = new HashSet<ulong>();
        for (GameObject? node = owner; node is not null; node = node.Parent)
        {
            if (!seen.Add(node.ObjectId)) throw new InvalidOperationException("Cycle in component hierarchy.");
            if (node == owner || includeInactive || node.ActiveInHierarchy) Append(node, results);
        }
        return results.ToArray();
    }

    public static bool TryGetComponent<T>(this GameObject owner, out T component) where T : struct, IComponentFacade<T>
    {
        ArgumentNullException.ThrowIfNull(owner);
        var found = owner.GetComponent<T>(); component = found.GetValueOrDefault(); return found.HasValue;
    }

    private static void Append<T>(GameObject owner, List<T> results) where T : struct, IComponentFacade<T>
    {
        foreach (var component in owner.Components())
            if (component.TypeId == T.TypeId) results.Add(T.Wrap(component));
    }
}
