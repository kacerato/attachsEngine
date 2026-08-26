using System.Runtime.CompilerServices;

namespace Aether;

/// <summary>
/// Plano data-oriented da propagação de transforms. A hierarquia e as localizações físicas dos
/// componentes são compiladas após mudanças estruturais; frames estáveis percorrem somente um
/// array topológico e acessam diretamente as colunas já resolvidas.
/// </summary>
internal static class TransformPropagationPlanCache
{
    private static readonly ConditionalWeakTable<World, Plan> Plans = new();

    internal static void Propagate(World world)
    {
        var plan = Plans.GetValue(world, static _ => new Plan());
        if (!plan.IsCurrent(world))
            plan.Rebuild(world);
        plan.Execute();
    }

    private sealed class Plan
    {
        private Entry[] _entries = Array.Empty<Entry>();
        private NativeTransformPlanEntry[] _nativeEntries = Array.Empty<NativeTransformPlanEntry>();
        private Transform[] _computed = Array.Empty<Transform>();
        private OutputColumn[] _outputColumns = Array.Empty<OutputColumn>();
        private TrackedColumn[] _parentColumns = Array.Empty<TrackedColumn>();

        internal long StructureVersion = -1;
        internal long HierarchyVersion = -1;

        internal bool IsCurrent(World world)
        {
            if (StructureVersion != world.StructureVersion ||
                HierarchyVersion != world.HierarchyVersion)
                return false;
            for (int i = 0; i < _parentColumns.Length; i++)
                if (_parentColumns[i].Chunk.Versions[_parentColumns[i].Column] !=
                    _parentColumns[i].Version)
                    return false;
            return true;
        }

        internal void Rebuild(World world)
        {
            var unordered = new List<UnorderedEntry>(world.EntityCount);
            int capacity = world.Capacity;
            var entryByEntitySlot = new int[capacity];
            Array.Fill(entryByEntitySlot, -1);

            ComponentType localType = ComponentType.Of<LocalTransform>();
            ComponentType worldType = ComponentType.Of<WorldTransform>();

            foreach (var chunk in world.Query<LocalTransform, WorldTransform>())
            {
                int localColumn = chunk.Archetype.IndexOf(localType);
                int worldColumn = chunk.Archetype.IndexOf(worldType);
                var entities = chunk.Entities;
                for (int row = 0; row < chunk.Count; row++)
                {
                    int index = unordered.Count;
                    EntityId entity = entities[row];
                    unordered.Add(new UnorderedEntry(entity, chunk, localColumn, worldColumn, row));
                    entryByEntitySlot[entity.Index] = index;
                }
            }

            int count = unordered.Count;
            var parents = new int[count];
            var firstChild = new int[count];
            var nextSibling = new int[count];
            Array.Fill(parents, -1);
            Array.Fill(firstChild, -1);
            Array.Fill(nextSibling, -1);

            for (int i = 0; i < count; i++)
            {
                if (!world.TryGetComponent<Parent>(unordered[i].Entity, out var parent) ||
                    !world.Exists(parent.Value) ||
                    parent.Value.Index < 0 || parent.Value.Index >= entryByEntitySlot.Length)
                    continue;

                int parentIndex = entryByEntitySlot[parent.Value.Index];
                if (parentIndex < 0) continue; // pai sem Local+World: raiz lógica deste sistema
                parents[i] = parentIndex;
                nextSibling[i] = firstChild[parentIndex];
                firstChild[parentIndex] = i;
            }

            var queue = new int[count];
            var orderedOriginalIndices = new int[count];
            var emitted = new bool[count];
            int queueHead = 0;
            int queueTail = 0;
            int orderedCount = 0;

            for (int i = 0; i < count; i++)
                if (parents[i] < 0) queue[queueTail++] = i;
            DrainQueue();

            // Parent escrito diretamente pode introduzir um ciclo apesar da API Hierarchy impedir
            // isso. Quebramos cada componente cíclico de modo determinístico no menor índice ainda
            // não emitido, tratando-o como raiz lógica em vez de travar o editor.
            for (int i = 0; i < count; i++)
            {
                if (emitted[i]) continue;
                parents[i] = -1;
                queue[queueTail++] = i;
                DrainQueue();
            }

            var orderedPosition = new int[count];
            for (int i = 0; i < count; i++) orderedPosition[orderedOriginalIndices[i]] = i;

            var entries = new Entry[count];
            var outputSet = new HashSet<(Chunk Chunk, int Column)>();
            var outputs = new List<OutputColumn>();
            for (int orderedIndex = 0; orderedIndex < count; orderedIndex++)
            {
                int originalIndex = orderedOriginalIndices[orderedIndex];
                var source = unordered[originalIndex];
                int parentOriginal = parents[originalIndex];
                entries[orderedIndex] = new Entry(
                    source.Chunk,
                    source.LocalColumn,
                    source.WorldColumn,
                    source.Row,
                    parentOriginal < 0 ? -1 : orderedPosition[parentOriginal]);

                if (outputSet.Add((source.Chunk, source.WorldColumn)))
                    outputs.Add(new OutputColumn(source.Chunk, source.WorldColumn));
            }

            _entries = entries;
            _nativeEntries = new NativeTransformPlanEntry[count];
            for (int i = 0; i < count; i++)
            {
                ref readonly Entry entry = ref entries[i];
                _nativeEntries[i] = new NativeTransformPlanEntry(
                    entry.Chunk.GetAddressUnchecked<LocalTransform>(entry.LocalColumn, entry.Row),
                    entry.Chunk.GetAddressUnchecked<WorldTransform>(entry.WorldColumn, entry.Row),
                    entry.ParentIndex);
            }
            _computed = new Transform[count];
            _outputColumns = outputs.ToArray();

            ComponentType parentType = ComponentType.Of<Parent>();
            var parentColumns = new List<TrackedColumn>();
            foreach (var chunk in world.Query<Parent>())
            {
                int column = chunk.Archetype.IndexOf(parentType);
                parentColumns.Add(new TrackedColumn(chunk, column, chunk.Versions[column]));
            }
            _parentColumns = parentColumns.ToArray();
            StructureVersion = world.StructureVersion;
            HierarchyVersion = world.HierarchyVersion;

            void DrainQueue()
            {
                while (queueHead < queueTail)
                {
                    int current = queue[queueHead++];
                    if (emitted[current]) continue;
                    emitted[current] = true;
                    orderedOriginalIndices[orderedCount++] = current;
                    for (int child = firstChild[current]; child >= 0; child = nextSibling[child])
                        if (!emitted[child]) queue[queueTail++] = child;
                }
            }
        }

        [MethodImpl(MethodImplOptions.AggressiveOptimization)]
        internal void Execute()
        {
            for (int i = 0; i < _outputColumns.Length; i++)
                _outputColumns[i].Chunk.MarkChanged(_outputColumns[i].WorldColumn);

            if (NativeTransformKernel.TryPropagate(_nativeEntries, _entries.Length)) return;

            for (int i = 0; i < _entries.Length; i++)
            {
                ref readonly Entry entry = ref _entries[i];
                ref readonly LocalTransform local = ref entry.Chunk
                    .GetReadOnlyRefUnchecked<LocalTransform>(entry.LocalColumn, entry.Row);
                Transform result = entry.ParentIndex < 0
                    ? local.Value
                    : _computed[entry.ParentIndex].TransformChild(local.Value);
                _computed[i] = result;
                entry.Chunk.GetWritableRefUnchecked<WorldTransform>(entry.WorldColumn, entry.Row).Value =
                    result;
            }
        }
    }

    private readonly struct UnorderedEntry(
        EntityId entity,
        Chunk chunk,
        int localColumn,
        int worldColumn,
        int row)
    {
        internal readonly EntityId Entity = entity;
        internal readonly Chunk Chunk = chunk;
        internal readonly int LocalColumn = localColumn;
        internal readonly int WorldColumn = worldColumn;
        internal readonly int Row = row;
    }

    private readonly struct Entry(
        Chunk chunk,
        int localColumn,
        int worldColumn,
        int row,
        int parentIndex)
    {
        internal readonly Chunk Chunk = chunk;
        internal readonly int LocalColumn = localColumn;
        internal readonly int WorldColumn = worldColumn;
        internal readonly int Row = row;
        internal readonly int ParentIndex = parentIndex;
    }

    private readonly struct OutputColumn(Chunk chunk, int worldColumn)
    {
        internal readonly Chunk Chunk = chunk;
        internal readonly int WorldColumn = worldColumn;
    }

    private readonly struct TrackedColumn(Chunk chunk, int column, int version)
    {
        internal readonly Chunk Chunk = chunk;
        internal readonly int Column = column;
        internal readonly int Version = version;
    }
}
