namespace Astra;

/// <summary>Wrap Mode do clipe, na ordem do componente nativo `astra.animation`.</summary>
public enum AnimationWrapMode : uint { Once, Loop, PingPong, ClampForever }
/// <summary>PlayMode da Unity: parar os clipes da mesma camada ou todos.</summary>
public enum AnimationPlayMode : uint { StopSameLayer, StopAll }
/// <summary>Operação na ABI de animação (v9).</summary>
public enum AnimationCommandKind : uint { Play, CrossFade, Blend, Stop, Rewind }

/// <summary>Estado de um clipe num componente (AnimationState da Unity), como a ABI o transporta.</summary>
public readonly record struct AnimationStateValue(AssetGuid Clip, bool Enabled, float Time, float Speed, float Weight,
                                                  float Length, uint Layer, AnimationWrapMode WrapMode);
/// <summary>Entrada autoral da lista de clipes. ElementId permanece igual ao reordenar a lista.</summary>
public readonly record struct AnimationClipEntry(ulong ElementId, AssetGuid Clip, string Name);

/// <summary>
/// Reprodutor de um componente Animation (`astra.animation`) no mundo Play, no modelo do Animation legado
/// da Unity: Play, CrossFade, Blend, Stop e Rewind por clipe, e um <see cref="AnimationState"/> por clipe com
/// tempo, velocidade, peso, camada e repetição. Os clipes são os da lista do componente, por nome; um nome
/// fora da lista é recusado. Tocando, tempo e peso são estado de execução: o Play sempre parte do zero.
/// </summary>
public readonly struct AnimationPlayer(Component component)
{
    private readonly ISceneAccess _scene = component.Scene;
    public Component Component => component;

    private void Check(bool ok, string operation)
    {
        if (!ok) throw new WorldException(_scene.LastStatus, operation);
    }

    /// <summary>Nomes dos clipes do componente, na ordem da lista.</summary>
    public IReadOnlyList<string> ClipNames
    {
        get
        {
            var count = _scene.AnimationClipAt(component.Object.ObjectId, component.InstanceId, 0, out _, out _);
            if (count < 0) throw new WorldException(_scene.LastStatus, "listar clipes");
            var names = new List<string>(count);
            for (var i = 0u; i < (uint)count; ++i)
            {
                _scene.AnimationClipAt(component.Object.ObjectId, component.InstanceId, i, out _, out var name);
                names.Add(name);
            }
            return names;
        }
    }

    /// <summary>Clipes e identidades persistentes da lista, na ordem atual.</summary>
    public IReadOnlyList<AnimationClipEntry> ClipEntries
    {
        get
        {
            var count = _scene.AnimationClipAt(component.Object.ObjectId, component.InstanceId, 0, out _, out _);
            if (count < 0) throw new WorldException(_scene.LastStatus, "listar entradas de clipe");
            var entries = new List<AnimationClipEntry>(count);
            for (var i = 0u; i < (uint)count; ++i)
            {
                var actual = _scene.AnimationClipAt(component.Object.ObjectId, component.InstanceId, i, out _, out var name);
                if (actual < 0) throw new WorldException(_scene.LastStatus, "listar entradas de clipe");
                if (actual != count) throw new WorldException(WorldStatus.Rejected, "lista de clipes mudou durante leitura");
                var id = component.ResourceElementId("clips", i);
                entries.Add(new(id, component.GetResourceByElementId("clips", id), name));
            }
            return entries;
        }
    }

    /// <summary>Troca o recurso da entrada por ID; a posição atual da entrada não importa.</summary>
    public void SetClip(ulong elementId, AssetGuid clip) => component.SetResourceByElementId("clips", elementId, clip);

    /// <summary>Adiciona um clipe resolvido à lista desta sessão de Play e devolve o ID da entrada.</summary>
    public ulong AddClip(AssetGuid clip)
    {
        Check(_scene.AppendAnimationClip(component.Object.ObjectId, component.InstanceId, clip, out var elementId), "adicionar clipe");
        return elementId;
    }

    /// <summary>Remove uma entrada por ID. Um clipe sem outra entrada e sem ser padrão para no próximo passo.</summary>
    public void RemoveClip(ulong elementId) =>
        Check(_scene.RemoveAnimationClip(component.Object.ObjectId, component.InstanceId, elementId), "remover clipe");

    /// <summary>Move a entrada para o índice indicado sem alterar seu ID.</summary>
    public void MoveClip(ulong elementId, uint targetIndex) =>
        Check(_scene.MoveAnimationClip(component.Object.ObjectId, component.InstanceId, elementId, targetIndex), "mover clipe");

    /// <summary>Identidade do clipe pelo nome; lança quando o componente não lista o nome.</summary>
    public AssetGuid Clip(string name)
    {
        ArgumentNullException.ThrowIfNull(name);
        var count = _scene.AnimationClipAt(component.Object.ObjectId, component.InstanceId, 0, out _, out _);
        if (count < 0) throw new WorldException(_scene.LastStatus, "procurar clipe");
        for (var i = 0u; i < (uint)count; ++i)
            if (_scene.AnimationClipAt(component.Object.ObjectId, component.InstanceId, i, out var clip, out var found) >= 0 &&
                found == name) return clip;
        throw new WorldException(WorldStatus.ClipNotInComponent, "procurar clipe " + name);
    }

    private void Command(AnimationCommandKind op, AssetGuid clip, float seconds, float weight, AnimationPlayMode mode, string what) =>
        Check(_scene.AnimationCommand(component.Object.ObjectId, component.InstanceId, op, clip, seconds, weight, mode), what);

    /// <summary>Toca o clipe padrão, parando os outros da mesma camada. Não rebobina um clipe que já toca.</summary>
    public void Play(AnimationPlayMode mode = AnimationPlayMode.StopSameLayer) =>
        Command(AnimationCommandKind.Play, default, 0, 1, mode, "tocar");
    public void Play(string clip, AnimationPlayMode mode = AnimationPlayMode.StopSameLayer) =>
        Command(AnimationCommandKind.Play, Clip(clip), 0, 1, mode, "tocar " + clip);
    /// <summary>Entra no clipe em <paramref name="seconds"/> enquanto os outros da camada saem.</summary>
    public void CrossFade(string clip, float seconds = 0.3f, AnimationPlayMode mode = AnimationPlayMode.StopSameLayer) =>
        Command(AnimationCommandKind.CrossFade, Clip(clip), seconds, 1, mode, "cross-fade " + clip);
    /// <summary>Leva o peso do clipe até <paramref name="targetWeight"/> sem mexer nos outros.</summary>
    public void Blend(string clip, float targetWeight = 1, float seconds = 0.3f) =>
        Command(AnimationCommandKind.Blend, Clip(clip), seconds, targetWeight, AnimationPlayMode.StopSameLayer, "misturar " + clip);
    /// <summary>Para e rebobina todos os clipes, como Animation.Stop da Unity. A pose atual fica.</summary>
    public void Stop() => Command(AnimationCommandKind.Stop, default, 0, 0, AnimationPlayMode.StopSameLayer, "parar");
    public void Stop(string clip) => Command(AnimationCommandKind.Stop, Clip(clip), 0, 0, AnimationPlayMode.StopSameLayer, "parar " + clip);
    public void Rewind() => Command(AnimationCommandKind.Rewind, default, 0, 0, AnimationPlayMode.StopSameLayer, "rebobinar");
    public void Rewind(string clip) => Command(AnimationCommandKind.Rewind, Clip(clip), 0, 0, AnimationPlayMode.StopSameLayer, "rebobinar " + clip);

    /// <summary>Algum clipe tocando.</summary>
    public bool IsPlaying
    {
        get
        {
            var count = _scene.AnimationClipAt(component.Object.ObjectId, component.InstanceId, 0, out _, out _);
            for (var i = 0u; i < (uint)Math.Max(count, 0); ++i)
            {
                _scene.AnimationClipAt(component.Object.ObjectId, component.InstanceId, i, out var clip, out _);
                if (_scene.TryGetAnimationState(component.Object.ObjectId, component.InstanceId, clip, out var state) && state.Enabled)
                    return true;
            }
            return false;
        }
    }
    public bool IsClipPlaying(string clip) => this[clip].Enabled;

    /// <summary>AnimationState do clipe (animation["nome"] na Unity).</summary>
    public AnimationState this[string clip] => new(this, Clip(clip));

    internal AnimationStateValue Read(AssetGuid clip)
    {
        Check(_scene.TryGetAnimationState(component.Object.ObjectId, component.InstanceId, clip, out var value), "ler estado");
        return value;
    }
    internal void Write(in AnimationStateValue value) =>
        Check(_scene.SetAnimationState(component.Object.ObjectId, component.InstanceId, value), "escrever estado");

    // Configuração autoral do componente, pelas propriedades persistentes.
    public bool PlayAutomatically
    {
        get => component.GetBool("play_automatically");
        set => component.SetBool("play_automatically", value);
    }
    public AnimationWrapMode WrapMode
    {
        get => (AnimationWrapMode)component.GetEnum("wrap_mode");
        set => component.SetEnum("wrap_mode", (uint)value);
    }
}

/// <summary>Um clipe dentro de um componente Animation: tempo, velocidade, peso, camada e repetição.</summary>
public readonly struct AnimationState
{
    private readonly AnimationPlayer _player;
    public AssetGuid Clip { get; }
    internal AnimationState(AnimationPlayer player, AssetGuid clip) { _player = player; Clip = clip; }
    private AnimationStateValue Value => _player.Read(Clip);
    public bool Enabled { get => Value.Enabled; set => _player.Write(Value with { Enabled = value }); }
    public float Time { get => Value.Time; set => _player.Write(Value with { Time = value }); }
    public float Speed { get => Value.Speed; set => _player.Write(Value with { Speed = value }); }
    public float Weight { get => Value.Weight; set => _player.Write(Value with { Weight = value }); }
    public uint Layer { get => Value.Layer; set => _player.Write(Value with { Layer = value }); }
    public AnimationWrapMode WrapMode { get => Value.WrapMode; set => _player.Write(Value with { WrapMode = value }); }
    /// <summary>Duração do clipe em segundos.</summary>
    public float Length => Value.Length;
    /// <summary>Tempo sobre a duração (AnimationState.normalizedTime).</summary>
    public float NormalizedTime
    {
        get { var v = Value; return v.Length > 0 ? v.Time / v.Length : 0; }
        set { var v = Value; _player.Write(v with { Time = value * v.Length }); }
    }
}

/// <summary>
/// Malha deformável (`astra.render.skinned_mesh`): os pesos dos blend shapes, na escala da Unity
/// (0 forma base, 100 alvo inteiro), como SkinnedMeshRenderer.Set/GetBlendShapeWeight.
/// </summary>
public readonly struct DeformableMesh(Component component)
{
    public Component Component => component;
    public float GetBlendShapeWeight(int index) =>
        component.GetSlotFloat("blend_shape_weight", checked((uint)index));
    public void SetBlendShapeWeight(int index, float weight) =>
        component.SetSlotFloat("blend_shape_weight", weight, checked((uint)index));
}
