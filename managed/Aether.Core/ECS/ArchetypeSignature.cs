namespace Aether;

/// <summary>
/// Conjunto de tipos de componente de um arquétipo, como bitmask de 256 bits (4 x ulong). Suporta
/// até 256 tipos de componente distintos no processo — de sobra para uma engine mobile. Igualdade e
/// hash são baratos (4 comparações de ulong) e independem da ordem de inserção, ao contrário de um
/// hash calculado sobre uma lista/array de tipos.
/// </summary>
public struct ArchetypeSignature : IEquatable<ArchetypeSignature>
{
    private ulong _w0, _w1, _w2, _w3;

    public void Add(ComponentType t) => Set(t.Id, true);
    public void Remove(ComponentType t) => Set(t.Id, false);
    public readonly bool Has(ComponentType t) => Get(t.Id);

    private void Set(int id, bool value)
    {
        ulong bit = 1UL << (id & 63);
        switch (id >> 6)
        {
            case 0: if (value) _w0 |= bit; else _w0 &= ~bit; break;
            case 1: if (value) _w1 |= bit; else _w1 &= ~bit; break;
            case 2: if (value) _w2 |= bit; else _w2 &= ~bit; break;
            case 3: if (value) _w3 |= bit; else _w3 &= ~bit; break;
            default: throw new ArgumentOutOfRangeException(nameof(id), "mais de 256 tipos de componente distintos registrados no processo.");
        }
    }

    private readonly bool Get(int id)
    {
        ulong bit = 1UL << (id & 63);
        return (id >> 6) switch
        {
            0 => (_w0 & bit) != 0,
            1 => (_w1 & bit) != 0,
            2 => (_w2 & bit) != 0,
            3 => (_w3 & bit) != 0,
            _ => false,
        };
    }

    /// <summary>Verdadeiro se este sinal contém todos os bits exigidos por <paramref name="with"/>
    /// e nenhum dos bits proibidos por <paramref name="without"/>.</summary>
    public readonly bool Matches(in ArchetypeSignature with, in ArchetypeSignature without) =>
        (_w0 & with._w0) == with._w0 && (_w1 & with._w1) == with._w1 &&
        (_w2 & with._w2) == with._w2 && (_w3 & with._w3) == with._w3 &&
        (_w0 & without._w0) == 0 && (_w1 & without._w1) == 0 &&
        (_w2 & without._w2) == 0 && (_w3 & without._w3) == 0;

    public readonly bool Equals(ArchetypeSignature o) =>
        _w0 == o._w0 && _w1 == o._w1 && _w2 == o._w2 && _w3 == o._w3;
    public override readonly bool Equals(object? o) => o is ArchetypeSignature s && Equals(s);
    public override readonly int GetHashCode() => HashCode.Combine(_w0, _w1, _w2, _w3);
    public static bool operator ==(ArchetypeSignature a, ArchetypeSignature b) => a.Equals(b);
    public static bool operator !=(ArchetypeSignature a, ArchetypeSignature b) => !a.Equals(b);
}
