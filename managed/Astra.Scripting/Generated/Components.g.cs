// GERADO por scene::componentCSharpApi() a partir de native/scene/schemas/*.h — não edite.
// Regenerar: aether_tests --write-component-api managed/Astra.Scripting/Generated/Components.g.cs
#nullable enable
using System;
using System.Numerics;

namespace Astra.Components;

/// <summary>Fachada tipada de um tipo de componente nativo.</summary>
public interface IComponentFacade<TSelf> where TSelf : struct, IComponentFacade<TSelf>
{
    static abstract string TypeId { get; }
    static abstract TSelf Wrap(Component component);
    Component Component { get; }
}

/// <summary>Acesso tipado a partir do objeto: <c>obj.GetComponent&lt;PhysicsBody&gt;()</c>.</summary>
public static class ComponentFacadeExtensions
{
    public static T? GetComponent<T>(this GameObject owner, int ordinal = 0) where T : struct, IComponentFacade<T> =>
        owner.GetComponent(T.TypeId, ordinal) is { } component ? T.Wrap(component) : null;
    public static T AddComponent<T>(this GameObject owner) where T : struct, IComponentFacade<T> =>
        T.Wrap(owner.AddComponent(T.TypeId));
    public static bool HasComponent<T>(this GameObject owner) where T : struct, IComponentFacade<T> =>
        owner.HasComponent(T.TypeId);
}

/// <summary>Mola de posição: Segue fonte com velocidade e amortecimento persistentes no runtime. Família Lógica · Molas.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-PositionConstraint.html</remarks>
public readonly struct SpringPositionConstraint : IComponentFacade<SpringPositionConstraint>
{
    public static string TypeId => "astra.spring.position";
    public static SpringPositionConstraint Wrap(Component component) => new(component);
    public Component Component { get; }
    public SpringPositionConstraint(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Offset</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Influência</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Frequência (Hz)</summary>
    /// <remarks>Faixa válida: 0.01 a 60.</remarks>
    public float Frequency
    {
        get => Component.GetFloat("frequency");
        set => Component.SetFloat("frequency", value);
    }
    /// <summary>Razão de amortecimento</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float DampingRatio
    {
        get => Component.GetFloat("damping_ratio");
        set => Component.SetFloat("damping_ratio", value);
    }
    /// <summary>Velocidade máxima (m/s)</summary>
    /// <remarks>Faixa válida: 0.001 a 100000.</remarks>
    public float MaxSpeed
    {
        get => Component.GetFloat("max_speed");
        set => Component.SetFloat("max_speed", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Fonte</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Mola de rotação: Segue orientação por quaternion com amortecimento e caminho curto. Família Lógica · Molas.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-RotationConstraint.html</remarks>
public readonly struct SpringRotationConstraint : IComponentFacade<SpringRotationConstraint>
{
    public static string TypeId => "astra.spring.rotation";
    public static SpringRotationConstraint Wrap(Component component) => new(component);
    public Component Component { get; }
    public SpringRotationConstraint(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Offset</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Influência</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Frequência (Hz)</summary>
    /// <remarks>Faixa válida: 0.01 a 60.</remarks>
    public float Frequency
    {
        get => Component.GetFloat("frequency");
        set => Component.SetFloat("frequency", value);
    }
    /// <summary>Razão de amortecimento</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float DampingRatio
    {
        get => Component.GetFloat("damping_ratio");
        set => Component.SetFloat("damping_ratio", value);
    }
    /// <summary>Velocidade máxima (graus/s)</summary>
    /// <remarks>Faixa válida: 0.001 a 100000.</remarks>
    public float MaxSpeed
    {
        get => Component.GetFloat("max_speed");
        set => Component.SetFloat("max_speed", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Fonte</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Mola de escala: Segue escala positiva sem publicar escala singular. Família Lógica · Molas.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-ScaleConstraint.html</remarks>
public readonly struct SpringScaleConstraint : IComponentFacade<SpringScaleConstraint>
{
    public static string TypeId => "astra.spring.scale";
    public static SpringScaleConstraint Wrap(Component component) => new(component);
    public Component Component { get; }
    public SpringScaleConstraint(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Offset</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Influência</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Frequência (Hz)</summary>
    /// <remarks>Faixa válida: 0.01 a 60.</remarks>
    public float Frequency
    {
        get => Component.GetFloat("frequency");
        set => Component.SetFloat("frequency", value);
    }
    /// <summary>Razão de amortecimento</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float DampingRatio
    {
        get => Component.GetFloat("damping_ratio");
        set => Component.SetFloat("damping_ratio", value);
    }
    /// <summary>Velocidade máxima (x/s)</summary>
    /// <remarks>Faixa válida: 0.001 a 100000.</remarks>
    public float MaxSpeed
    {
        get => Component.GetFloat("max_speed");
        set => Component.SetFloat("max_speed", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Fonte</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Position Constraint: Restrição de mundo com uma fonte. Família Lógica · Restrições.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-PositionConstraint.html</remarks>
public readonly struct PositionConstraint : IComponentFacade<PositionConstraint>
{
    public static string TypeId => "astra.constraint.position";
    public static PositionConstraint Wrap(Component component) => new(component);
    public Component Component { get; }
    public PositionConstraint(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Deslocamento</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Aplicar X</summary>
    public bool AxisX
    {
        get => Component.GetBool("axis_x");
        set => Component.SetBool("axis_x", value);
    }
    /// <summary>Aplicar Y</summary>
    public bool AxisY
    {
        get => Component.GetBool("axis_y");
        set => Component.SetBool("axis_y", value);
    }
    /// <summary>Aplicar Z</summary>
    public bool AxisZ
    {
        get => Component.GetBool("axis_z");
        set => Component.SetBool("axis_z", value);
    }
    /// <summary>Fonte</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Rotation Constraint: Restrição de mundo com uma fonte. Família Lógica · Restrições.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-RotationConstraint.html</remarks>
public readonly struct RotationConstraint : IComponentFacade<RotationConstraint>
{
    public static string TypeId => "astra.constraint.rotation";
    public static RotationConstraint Wrap(Component component) => new(component);
    public Component Component { get; }
    public RotationConstraint(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Deslocamento</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Aplicar X</summary>
    public bool AxisX
    {
        get => Component.GetBool("axis_x");
        set => Component.SetBool("axis_x", value);
    }
    /// <summary>Aplicar Y</summary>
    public bool AxisY
    {
        get => Component.GetBool("axis_y");
        set => Component.SetBool("axis_y", value);
    }
    /// <summary>Aplicar Z</summary>
    public bool AxisZ
    {
        get => Component.GetBool("axis_z");
        set => Component.SetBool("axis_z", value);
    }
    /// <summary>Fonte</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Scale Constraint: Restrição de mundo com uma fonte. Família Lógica · Restrições.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-ScaleConstraint.html</remarks>
public readonly struct ScaleConstraint : IComponentFacade<ScaleConstraint>
{
    public static string TypeId => "astra.constraint.scale";
    public static ScaleConstraint Wrap(Component component) => new(component);
    public Component Component { get; }
    public ScaleConstraint(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Deslocamento</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Aplicar X</summary>
    public bool AxisX
    {
        get => Component.GetBool("axis_x");
        set => Component.SetBool("axis_x", value);
    }
    /// <summary>Aplicar Y</summary>
    public bool AxisY
    {
        get => Component.GetBool("axis_y");
        set => Component.SetBool("axis_y", value);
    }
    /// <summary>Aplicar Z</summary>
    public bool AxisZ
    {
        get => Component.GetBool("axis_z");
        set => Component.SetBool("axis_z", value);
    }
    /// <summary>Fonte</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Aim Constraint: Restrição de mundo com uma fonte. Família Lógica · Restrições.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-AimConstraint.html</remarks>
public readonly struct AimConstraint : IComponentFacade<AimConstraint>
{
    public static string TypeId => "astra.constraint.aim";
    public static AimConstraint Wrap(Component component) => new(component);
    public Component Component { get; }
    public AimConstraint(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Deslocamento</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Aplicar X</summary>
    public bool AxisX
    {
        get => Component.GetBool("axis_x");
        set => Component.SetBool("axis_x", value);
    }
    /// <summary>Aplicar Y</summary>
    public bool AxisY
    {
        get => Component.GetBool("axis_y");
        set => Component.SetBool("axis_y", value);
    }
    /// <summary>Aplicar Z</summary>
    public bool AxisZ
    {
        get => Component.GetBool("axis_z");
        set => Component.SetBool("axis_z", value);
    }
    public enum AimAxisOption : uint
    {
        X = 0,
        Y = 1,
        Z = 2,
        X3 = 3,
        Y4 = 4,
        Z5 = 5,
    }
    /// <summary>Eixo de mira</summary>
    public AimAxisOption AimAxis
    {
        get => (AimAxisOption)Component.GetEnum("aim_axis");
        set => Component.SetEnum("aim_axis", (uint)value);
    }
    public enum UpAxisOption : uint
    {
        Y = 0,
        Z = 1,
        X = 2,
    }
    /// <summary>Vertical de mundo</summary>
    public UpAxisOption UpAxis
    {
        get => (UpAxisOption)Component.GetEnum("up_axis");
        set => Component.SetEnum("up_axis", (uint)value);
    }
    /// <summary>Fonte</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Parent Constraint: Segue posição e rotação da fonte sem herdar escala. Família Lógica · Restrições.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-ParentConstraint.html</remarks>
public readonly struct ParentConstraint : IComponentFacade<ParentConstraint>
{
    public static string TypeId => "astra.constraint.parent";
    public static ParentConstraint Wrap(Component component) => new(component);
    public Component Component { get; }
    public ParentConstraint(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Posição offset</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Rotação offset</summary>
    public Vector3 RotationOffset
    {
        get => new(Component.GetFloat("rotation_offset_x"), Component.GetFloat("rotation_offset_y"), Component.GetFloat("rotation_offset_z"));
        set => Component.SetVector3("rotation_offset", value);
    }
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Aplicar X</summary>
    public bool AxisX
    {
        get => Component.GetBool("axis_x");
        set => Component.SetBool("axis_x", value);
    }
    /// <summary>Aplicar Y</summary>
    public bool AxisY
    {
        get => Component.GetBool("axis_y");
        set => Component.SetBool("axis_y", value);
    }
    /// <summary>Aplicar Z</summary>
    public bool AxisZ
    {
        get => Component.GetBool("axis_z");
        set => Component.SetBool("axis_z", value);
    }
    /// <summary>Rotação X</summary>
    public bool RotationX
    {
        get => Component.GetBool("rotation_x");
        set => Component.SetBool("rotation_x", value);
    }
    /// <summary>Rotação Y</summary>
    public bool RotationY
    {
        get => Component.GetBool("rotation_y");
        set => Component.SetBool("rotation_y", value);
    }
    /// <summary>Rotação Z</summary>
    public bool RotationZ
    {
        get => Component.GetBool("rotation_z");
        set => Component.SetBool("rotation_z", value);
    }
    /// <summary>Fonte</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Look At Constraint: Orienta +Z para a fonte com vertical e roll. Família Lógica · Restrições.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-LookAtConstraint.html</remarks>
public readonly struct LookAtConstraint : IComponentFacade<LookAtConstraint>
{
    public static string TypeId => "astra.constraint.look_at";
    public static LookAtConstraint Wrap(Component component) => new(component);
    public Component Component { get; }
    public LookAtConstraint(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Roll (graus)</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float Roll
    {
        get => Component.GetFloat("roll");
        set => Component.SetFloat("roll", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Aplicar X</summary>
    public bool AxisX
    {
        get => Component.GetBool("axis_x");
        set => Component.SetBool("axis_x", value);
    }
    /// <summary>Aplicar Y</summary>
    public bool AxisY
    {
        get => Component.GetBool("axis_y");
        set => Component.SetBool("axis_y", value);
    }
    /// <summary>Aplicar Z</summary>
    public bool AxisZ
    {
        get => Component.GetBool("axis_z");
        set => Component.SetBool("axis_z", value);
    }
    public enum UpAxisOption : uint
    {
        Y = 0,
        Z = 1,
        X = 2,
    }
    /// <summary>Vertical de mundo</summary>
    public UpAxisOption UpAxis
    {
        get => (UpAxisOption)Component.GetEnum("up_axis");
        set => Component.SetEnum("up_axis", (uint)value);
    }
    /// <summary>Fonte</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Transform Tween: Interpola canais locais com espera, curva e repetição. Família Lógica · Tempo.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_tween.html</remarks>
public readonly struct TransformTween : IComponentFacade<TransformTween>
{
    public static string TypeId => "astra.tween.transform";
    public static TransformTween Wrap(Component component) => new(component);
    public Component Component { get; }
    public TransformTween(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Posição destino</summary>
    public Vector3 PositionDestination
    {
        get => new(Component.GetFloat("position_x"), Component.GetFloat("position_y"), Component.GetFloat("position_z"));
        set => Component.SetVector3("position_destination", value);
    }
    /// <summary>Rotação destino</summary>
    public Vector3 RotationDestination
    {
        get => new(Component.GetFloat("rotation_x"), Component.GetFloat("rotation_y"), Component.GetFloat("rotation_z"));
        set => Component.SetVector3("rotation_destination", value);
    }
    /// <summary>Escala destino</summary>
    public Vector3 ScaleDestination
    {
        get => new(Component.GetFloat("scale_x"), Component.GetFloat("scale_y"), Component.GetFloat("scale_z"));
        set => Component.SetVector3("scale_destination", value);
    }
    /// <summary>Duração (s)</summary>
    /// <remarks>Faixa válida: 0.001 a 36000.</remarks>
    public float Duration
    {
        get => Component.GetFloat("duration");
        set => Component.SetFloat("duration", value);
    }
    /// <summary>Espera (s)</summary>
    /// <remarks>Faixa válida: 0 a 36000.</remarks>
    public float Delay
    {
        get => Component.GetFloat("delay");
        set => Component.SetFloat("delay", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Iniciar no Play</summary>
    public bool Autoplay
    {
        get => Component.GetBool("autoplay");
        set => Component.SetBool("autoplay", value);
    }
    /// <summary>Ida e volta</summary>
    public bool Pingpong
    {
        get => Component.GetBool("pingpong");
        set => Component.SetBool("pingpong", value);
    }
    /// <summary>Destino relativo</summary>
    public bool Relative
    {
        get => Component.GetBool("relative");
        set => Component.SetBool("relative", value);
    }
    /// <summary>Mover</summary>
    public bool Position
    {
        get => Component.GetBool("position");
        set => Component.SetBool("position", value);
    }
    /// <summary>Girar</summary>
    public bool Rotation
    {
        get => Component.GetBool("rotation");
        set => Component.SetBool("rotation", value);
    }
    /// <summary>Escalar</summary>
    public bool Scale
    {
        get => Component.GetBool("scale");
        set => Component.SetBool("scale", value);
    }
    /// <summary>Ignorar escala de tempo. Usa o intervalo não escalado aceito; pausa editorial interrompe ambos os modos</summary>
    public bool IgnoreTimeScale
    {
        get => Component.GetBool("ignore_time_scale");
        set => Component.SetBool("ignore_time_scale", value);
    }
    public enum EasingOption : uint
    {
        Linear = 0,
        Smoothstep = 1,
        QuadraticoEntrada = 2,
        QuadraticoSaida = 3,
    }
    /// <summary>Curva</summary>
    public EasingOption Easing
    {
        get => (EasingOption)Component.GetEnum("easing");
        set => Component.SetEnum("easing", (uint)value);
    }
    public enum LoopsOption : uint
    {
        Infinito = 0,
        UmaVez = 1,
        DuasVezes = 2,
        TresVezes = 3,
        DezVezes = 10,
    }
    /// <summary>Ciclos</summary>
    public LoopsOption Loops
    {
        get => (LoopsOption)Component.GetEnum("loops");
        set => Component.SetEnum("loops", (uint)value);
    }
    public enum FinishedActionOption : uint
    {
        Desconectado = 0,
        AtivarObjeto = 1,
        DesativarObjeto = 2,
        AlternarObjeto = 3,
    }
    /// <summary>Ao concluir. Executa depois da pose final de todos os ciclos finitos; cancelar ou ciclo infinito não dispara</summary>
    public FinishedActionOption FinishedAction
    {
        get => (FinishedActionOption)Component.GetEnum("finished_action");
        set => Component.SetEnum("finished_action", (uint)value);
    }
    /// <summary>Receptor. Referência persistente remapeada ao clonar; controla activeSelf do receptor</summary>
    public ObjectReference FinishedTarget
    {
        get => Component.GetReference("finished_target");
        set => Component.SetReference("finished_target", value);
    }
    /// <summary>Reiniciar. Recomeça da pose atual, conservando a pausa</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Restart() => Component.Invoke("restart");
    /// <summary>Cancelar. Interrompe sem voltar à pose inicial</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Cancel() => Component.Invoke("cancel");
    /// <summary>Pausar. Congela o progresso</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Pause() => Component.Invoke("pause");
    /// <summary>Retomar. Continua o progresso pausado</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Resume() => Component.Invoke("resume");
    /// <summary>Decorrido. Segundos desde o início, incluindo o atraso</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double Elapsed() => Component.Invoke("elapsed").AsNumber();
    /// <summary>Concluiu. Emitido uma vez quando as repetições finitas terminam, depois da pose final</summary>
    public ComponentSubscription OnCompleted(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "completed", handler);
}

/// <summary>Tween de propriedade: Interpola uma propriedade numérica interpolável de um componente. Família Lógica · Tempo.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_tween.html#class-tween-method-tween-property</remarks>
public readonly struct PropertyTween : IComponentFacade<PropertyTween>
{
    public static string TypeId => "astra.tween.property";
    public static PropertyTween Wrap(Component component) => new(component);
    public Component Component { get; }
    public PropertyTween(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Destino. Valor final; com "Destino relativo", soma ao valor do início</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float Destination
    {
        get => Component.GetFloat("destination");
        set => Component.SetFloat("destination", value);
    }
    /// <summary>Duração (s)</summary>
    /// <remarks>Faixa válida: 0.001 a 36000.</remarks>
    public float Duration
    {
        get => Component.GetFloat("duration");
        set => Component.SetFloat("duration", value);
    }
    /// <summary>Espera (s)</summary>
    /// <remarks>Faixa válida: 0 a 36000.</remarks>
    public float Delay
    {
        get => Component.GetFloat("delay");
        set => Component.SetFloat("delay", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Iniciar no Play</summary>
    public bool Autoplay
    {
        get => Component.GetBool("autoplay");
        set => Component.SetBool("autoplay", value);
    }
    /// <summary>Ida e volta</summary>
    public bool Pingpong
    {
        get => Component.GetBool("pingpong");
        set => Component.SetBool("pingpong", value);
    }
    /// <summary>Destino relativo</summary>
    public bool Relative
    {
        get => Component.GetBool("relative");
        set => Component.SetBool("relative", value);
    }
    /// <summary>Ignorar escala de tempo</summary>
    public bool IgnoreTimeScale
    {
        get => Component.GetBool("ignore_time_scale");
        set => Component.SetBool("ignore_time_scale", value);
    }
    public enum EasingOption : uint
    {
        Linear = 0,
        Smoothstep = 1,
        QuadraticoEntrada = 2,
        QuadraticoSaida = 3,
    }
    /// <summary>Curva</summary>
    public EasingOption Easing
    {
        get => (EasingOption)Component.GetEnum("easing");
        set => Component.SetEnum("easing", (uint)value);
    }
    public enum LoopsOption : uint
    {
        Infinito = 0,
        UmaVez = 1,
        DuasVezes = 2,
        TresVezes = 3,
        DezVezes = 10,
    }
    /// <summary>Repetição</summary>
    public LoopsOption Loops
    {
        get => (LoopsOption)Component.GetEnum("loops");
        set => Component.SetEnum("loops", (uint)value);
    }
    /// <summary>Alvo. Objeto dono do componente animado; vazio usa este objeto</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
    /// <summary>Reiniciar. Recomeça do valor atual da propriedade, conservando a pausa</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Restart() => Component.Invoke("restart");
    /// <summary>Cancelar. Interrompe sem voltar ao valor inicial</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Cancel() => Component.Invoke("cancel");
    /// <summary>Pausar. Congela o progresso</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Pause() => Component.Invoke("pause");
    /// <summary>Retomar. Continua o progresso pausado</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Resume() => Component.Invoke("resume");
    /// <summary>Decorrido. Segundos desde o início, incluindo a espera</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double Elapsed() => Component.Invoke("elapsed").AsNumber();
    /// <summary>Concluiu. Emitido uma vez quando as repetições finitas terminam, depois do valor final</summary>
    public ComponentSubscription OnCompleted(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "completed", handler);
}

/// <summary>Sequência de tweens: Encadeia Transform Tweens em etapas sequenciais ou paralelas. Família Lógica · Tempo.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_tween.html</remarks>
public readonly struct TweenSequence : IComponentFacade<TweenSequence>
{
    public static string TypeId => "astra.tween.sequence";
    public static TweenSequence Wrap(Component component) => new(component);
    public Component Component { get; }
    public TweenSequence(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Intervalo da etapa 1 (s). Espera antes de a etapa começar, somada à espera do próprio tween</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Interval0
    {
        get => Component.GetFloat("interval_0");
        set => Component.SetFloat("interval_0", value);
    }
    /// <summary>Intervalo da etapa 2 (s). Espera antes de a etapa começar, somada à espera do próprio tween</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Interval1
    {
        get => Component.GetFloat("interval_1");
        set => Component.SetFloat("interval_1", value);
    }
    /// <summary>Intervalo da etapa 3 (s). Espera antes de a etapa começar, somada à espera do próprio tween</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Interval2
    {
        get => Component.GetFloat("interval_2");
        set => Component.SetFloat("interval_2", value);
    }
    /// <summary>Intervalo da etapa 4 (s). Espera antes de a etapa começar, somada à espera do próprio tween</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Interval3
    {
        get => Component.GetFloat("interval_3");
        set => Component.SetFloat("interval_3", value);
    }
    /// <summary>Intervalo da etapa 5 (s). Espera antes de a etapa começar, somada à espera do próprio tween</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Interval4
    {
        get => Component.GetFloat("interval_4");
        set => Component.SetFloat("interval_4", value);
    }
    /// <summary>Intervalo da etapa 6 (s). Espera antes de a etapa começar, somada à espera do próprio tween</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Interval5
    {
        get => Component.GetFloat("interval_5");
        set => Component.SetFloat("interval_5", value);
    }
    /// <summary>Intervalo da etapa 7 (s). Espera antes de a etapa começar, somada à espera do próprio tween</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Interval6
    {
        get => Component.GetFloat("interval_6");
        set => Component.SetFloat("interval_6", value);
    }
    /// <summary>Intervalo da etapa 8 (s). Espera antes de a etapa começar, somada à espera do próprio tween</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Interval7
    {
        get => Component.GetFloat("interval_7");
        set => Component.SetFloat("interval_7", value);
    }
    /// <summary>Ativa. Desligada não reinicia etapas; a configuração é preservada</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Iniciar no Play. Sem isto a sequência espera o método Tocar (script ou Conexão de evento)</summary>
    public bool Autoplay
    {
        get => Component.GetBool("autoplay");
        set => Component.SetBool("autoplay", value);
    }
    /// <summary>Ignorar escala de tempo. Vale para os intervalos da sequência; cada tween segue a própria opção</summary>
    public bool IgnoreTimeScale
    {
        get => Component.GetBool("ignore_time_scale");
        set => Component.SetBool("ignore_time_scale", value);
    }
    /// <summary>Etapa 2 junto da anterior. Começa com a etapa anterior em vez de esperar ela terminar</summary>
    public bool Join1
    {
        get => Component.GetBool("join_1");
        set => Component.SetBool("join_1", value);
    }
    /// <summary>Etapa 3 junto da anterior. Começa com a etapa anterior em vez de esperar ela terminar</summary>
    public bool Join2
    {
        get => Component.GetBool("join_2");
        set => Component.SetBool("join_2", value);
    }
    /// <summary>Etapa 4 junto da anterior. Começa com a etapa anterior em vez de esperar ela terminar</summary>
    public bool Join3
    {
        get => Component.GetBool("join_3");
        set => Component.SetBool("join_3", value);
    }
    /// <summary>Etapa 5 junto da anterior. Começa com a etapa anterior em vez de esperar ela terminar</summary>
    public bool Join4
    {
        get => Component.GetBool("join_4");
        set => Component.SetBool("join_4", value);
    }
    /// <summary>Etapa 6 junto da anterior. Começa com a etapa anterior em vez de esperar ela terminar</summary>
    public bool Join5
    {
        get => Component.GetBool("join_5");
        set => Component.SetBool("join_5", value);
    }
    /// <summary>Etapa 7 junto da anterior. Começa com a etapa anterior em vez de esperar ela terminar</summary>
    public bool Join6
    {
        get => Component.GetBool("join_6");
        set => Component.SetBool("join_6", value);
    }
    /// <summary>Etapa 8 junto da anterior. Começa com a etapa anterior em vez de esperar ela terminar</summary>
    public bool Join7
    {
        get => Component.GetBool("join_7");
        set => Component.SetBool("join_7", value);
    }
    public enum LoopsOption : uint
    {
        Infinito = 0,
        UmaVez = 1,
        DuasVezes = 2,
        TresVezes = 3,
        DezVezes = 10,
    }
    /// <summary>Repetição. Quantas vezes a lista inteira de etapas é percorrida</summary>
    public LoopsOption Loops
    {
        get => (LoopsOption)Component.GetEnum("loops");
        set => Component.SetEnum("loops", (uint)value);
    }
    /// <summary>Etapa 1. Objeto com Transform Tween ou Tween de propriedade; a sequência reinicia esses tweens quando a etapa começa</summary>
    public ObjectReference Step0
    {
        get => Component.GetReference("step_0");
        set => Component.SetReference("step_0", value);
    }
    /// <summary>Etapa 2. Objeto com Transform Tween ou Tween de propriedade; a sequência reinicia esses tweens quando a etapa começa</summary>
    public ObjectReference Step1
    {
        get => Component.GetReference("step_1");
        set => Component.SetReference("step_1", value);
    }
    /// <summary>Etapa 3. Objeto com Transform Tween ou Tween de propriedade; a sequência reinicia esses tweens quando a etapa começa</summary>
    public ObjectReference Step2
    {
        get => Component.GetReference("step_2");
        set => Component.SetReference("step_2", value);
    }
    /// <summary>Etapa 4. Objeto com Transform Tween ou Tween de propriedade; a sequência reinicia esses tweens quando a etapa começa</summary>
    public ObjectReference Step3
    {
        get => Component.GetReference("step_3");
        set => Component.SetReference("step_3", value);
    }
    /// <summary>Etapa 5. Objeto com Transform Tween ou Tween de propriedade; a sequência reinicia esses tweens quando a etapa começa</summary>
    public ObjectReference Step4
    {
        get => Component.GetReference("step_4");
        set => Component.SetReference("step_4", value);
    }
    /// <summary>Etapa 6. Objeto com Transform Tween ou Tween de propriedade; a sequência reinicia esses tweens quando a etapa começa</summary>
    public ObjectReference Step5
    {
        get => Component.GetReference("step_5");
        set => Component.SetReference("step_5", value);
    }
    /// <summary>Etapa 7. Objeto com Transform Tween ou Tween de propriedade; a sequência reinicia esses tweens quando a etapa começa</summary>
    public ObjectReference Step6
    {
        get => Component.GetReference("step_6");
        set => Component.SetReference("step_6", value);
    }
    /// <summary>Etapa 8. Objeto com Transform Tween ou Tween de propriedade; a sequência reinicia esses tweens quando a etapa começa</summary>
    public ObjectReference Step7
    {
        get => Component.GetReference("step_7");
        set => Component.SetReference("step_7", value);
    }
    /// <summary>Tocar. Recomeça da etapa 1, cancelando os tweens das etapas em andamento</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Play() => Component.Invoke("play");
    /// <summary>Cancelar. Interrompe a sequência e os tweens da etapa em andamento, sem voltar poses</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Cancel() => Component.Invoke("cancel");
    /// <summary>Pausar. Congela intervalo e tweens da etapa em andamento</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Pause() => Component.Invoke("pause");
    /// <summary>Retomar. Continua de onde pausou</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Resume() => Component.Invoke("resume");
    /// <summary>Etapa atual. Número da etapa em andamento (1 a 8); zero parada</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public long Step() => Component.Invoke("step").AsInteger();
    /// <summary>Etapa começou. Emitido quando cada etapa reinicia o seu tween; carrega o número da etapa. Payload: Etapa: inteiro</summary>
    public ComponentSubscription OnStepStarted(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "step_started", handler);
    /// <summary>Concluiu. Emitido uma vez quando as repetições finitas terminam</summary>
    public ComponentSubscription OnCompleted(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "completed", handler);
}

/// <summary>Timer: Dispara eventos temporizados e ações persistentes de ativação de objetos. Família Lógica · Tempo.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_timer.html</remarks>
public readonly struct GameTimer : IComponentFacade<GameTimer>
{
    public static string TypeId => "astra.time.timer";
    public static GameTimer Wrap(Component component) => new(component);
    public Component Component { get; }
    public GameTimer(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Intervalo (s). Tempo entre disparos; uma vez quando Repetir está desligado</summary>
    /// <remarks>Faixa válida: 0.05 a 3600.</remarks>
    public float IntervalSeconds
    {
        get => Component.GetFloat("interval_seconds");
        set => Component.SetFloat("interval_seconds", value);
    }
    /// <summary>Iniciar automaticamente. Inicia ao entrar em Play ou criar a instância; desligado exige Start. Alteração não reinicia timer já criado</summary>
    public bool AutoStart
    {
        get => Component.GetBool("auto_start");
        set => Component.SetBool("auto_start", value);
    }
    /// <summary>Repetir</summary>
    public bool Repeat
    {
        get => Component.GetBool("repeat");
        set => Component.SetBool("repeat", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Ignorar escala de tempo. Usa o intervalo não escalado aceito do quadro; a pausa do editor interrompe ambos os relógios</summary>
    public bool IgnoreTimeScale
    {
        get => Component.GetBool("ignore_time_scale");
        set => Component.SetBool("ignore_time_scale", value);
    }
    public enum ElapsedActionOption : uint
    {
        Desconectado = 0,
        AtivarObjeto = 1,
        DesativarObjeto = 2,
        AlternarObjeto = 3,
    }
    /// <summary>Ao disparar. Executa uma ação no alvo antes de entregar TimerElapsed; alternar preserva a paridade dos disparos agregados</summary>
    public ElapsedActionOption ElapsedAction
    {
        get => (ElapsedActionOption)Component.GetEnum("elapsed_action");
        set => Component.SetEnum("elapsed_action", (uint)value);
    }
    /// <summary>Receptor. Alvo da ação; usa referência persistente remapeada por clone/prefab</summary>
    public ObjectReference ElapsedTarget
    {
        get => Component.GetReference("elapsed_target");
        set => Component.SetReference("elapsed_target", value);
    }
    /// <summary>Iniciar. Reinicia a contagem; intervalo zero usa o autorado</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Start(double interval) => Component.Invoke("start", ComponentValue.Number(interval));
    /// <summary>Parar. Interrompe e zera a contagem</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Stop() => Component.Invoke("stop");
    /// <summary>Pausar. Congela a contagem sem perder o restante</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Pause() => Component.Invoke("pause");
    /// <summary>Retomar. Continua a contagem pausada</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Resume() => Component.Invoke("resume");
    /// <summary>Restante. Segundos até o próximo disparo; zero quando parado</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double Remaining() => Component.Invoke("remaining").AsNumber();
    /// <summary>Em execução. Verdadeiro enquanto conta, inclusive pausado</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool Running() => Component.Invoke("running").AsBoolean();
    /// <summary>Disparou. Um evento por quadro; Disparos conta intervalos vencidos no quadro. Payload: Disparos: inteiro</summary>
    public ComponentSubscription OnElapsed(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "elapsed", handler);
}

/// <summary>Conexão de evento: Evento deste objeto aciona objetos ou métodos, sem script. Família Lógica · Eventos.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Events.UnityEvent.html</remarks>
public readonly struct EventConnection : IComponentFacade<EventConnection>
{
    public static string TypeId => "astra.logic.event_connection";
    public static EventConnection Wrap(Component component) => new(component);
    public Component Component { get; }
    public EventConnection(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Valor (s). Timer: intervalo (zero usa o autorado). Áudio: posição do cursor</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Argument
    {
        get => Component.GetFloat("argument");
        set => Component.SetFloat("argument", value);
    }
    /// <summary>Código do clipe. -1 aceita qualquer código; 0..16777215 seleciona o evento do clipe</summary>
    /// <remarks>Faixa válida: -1 a 16777215.</remarks>
    public float ClipTag
    {
        get => Component.GetFloat("clip_tag");
        set => Component.SetFloat("clip_tag", value);
    }
    /// <summary>Ativa. Desligada não reage a eventos; a configuração é preservada</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Uma vez. Reage só ao primeiro evento de cada execução de Play</summary>
    public bool Once
    {
        get => Component.GetBool("once");
        set => Component.SetBool("once", value);
    }
    public enum EventOption : uint
    {
        Nenhum = 0,
        TimerDisparou = 1,
        TweenConcluiu = 2,
        Sensor3DEntrou = 3,
        Sensor3DSaiu = 4,
        Colisao3DComecou = 5,
        Colisao3DTerminou = 6,
        Sensor2DEntrou = 7,
        Sensor2DSaiu = 8,
        Colisao2DComecou = 9,
        Colisao2DTerminou = 10,
        SequenciaEtapaComecou = 11,
        SequenciaConcluiu = 12,
        TweenDePropriedadeConcluiu = 13,
        JuntaQuebrou = 14,
        PersonagemBateuNumColisor = 15,
        Sensor3DDentro = 16,
        CerebroCameraAtivada = 17,
        CerebroCorteDeCamera = 18,
        CerebroTransicaoConcluida = 19,
        CameraVirtualEntrouAoVivo = 20,
        CameraVirtualSaiuDoAr = 21,
        AnimatorEntrouNumEstado = 22,
        AnimatorEventoDoEstado = 23,
        AnimatorEntrouNumGrupo = 24,
        AnimatorSaiuDeUmGrupo = 25,
        AnimatorMisturaInterrompida = 26,
        AgenteChegou = 27,
        AgenteCaminhoFalhou = 28,
        AgenteEntrouNumLink = 29,
        AnimationEventoDoClipe = 30,
        AnimationLimiteDeEventos = 31,
        AnimatorEventoDoClipe = 32,
        AnimatorLimiteDeEventos = 33,
        InteracaoMaoAlcancou = 34,
        InteracaoMaoSoltou = 35,
        InteracaoObjetoLevantado = 36,
        InteracaoEntrouNoGatilho = 37,
        InteracaoSaiuDoGatilho = 38,
        ParticulasParou = 39,
    }
    /// <summary>Evento. Emitido por um componente deste objeto; sem o componente, a conexão não dispara</summary>
    public EventOption Event
    {
        get => (EventOption)Component.GetEnum("event");
        set => Component.SetEnum("event", (uint)value);
    }
    public enum ActionOption : uint
    {
        Desconectado = 0,
        AtivarObjeto = 1,
        DesativarObjeto = 2,
        AlternarObjeto = 3,
        ChamarMetodo = 4,
    }
    /// <summary>Ação. Executada no ponto seguro seguinte ao evento, antes do próximo despacho de scripts</summary>
    public ActionOption Action
    {
        get => (ActionOption)Component.GetEnum("action");
        set => Component.SetEnum("action", (uint)value);
    }
    public enum MethodOption : uint
    {
        Nenhum = 0,
        AudioTocar = 1,
        AudioParar = 2,
        AudioPausar = 3,
        AudioRetomar = 4,
        AudioPosicionar = 5,
        TimerIniciar = 6,
        TimerParar = 7,
        TimerPausar = 8,
        TimerRetomar = 9,
        TweenReiniciar = 10,
        TweenCancelar = 11,
        TweenPausar = 12,
        TweenRetomar = 13,
        PercursoReiniciar = 14,
        PercursoParar = 15,
        SequenciaTocar = 16,
        SequenciaCancelar = 17,
        SequenciaPausar = 18,
        SequenciaRetomar = 19,
        TweenDePropriedadeReiniciar = 20,
        TweenDePropriedadeCancelar = 21,
        TweenDePropriedadePausar = 22,
        TweenDePropriedadeRetomar = 23,
        RaioAtualizarAgora = 24,
        VarreduraAtualizarAgora = 25,
        CameraVirtualPriorizar = 26,
        CameraVirtualEncaixar = 27,
        SnapshotTransicionar = 28,
        SnapshotAplicar = 29,
        AgenteParar = 30,
        AgenteRetomar = 31,
        RecuoDisparar = 32,
        ImpactoGolpearDeFrente = 33,
        PersonagemSubirDescer = 34,
        ParticulasTocar = 35,
        ParticulasParar = 36,
        ParticulasPararELimpar = 37,
        ParticulasPausar = 38,
        ParticulasLimpar = 39,
    }
    /// <summary>Método. Chamado no primeiro componente do tipo correspondente no receptor</summary>
    public MethodOption Method
    {
        get => (MethodOption)Component.GetEnum("method");
        set => Component.SetEnum("method", (uint)value);
    }
    /// <summary>Receptor. Objeto ativado ou dono do componente chamado; vazio usa o próprio emissor</summary>
    public ObjectReference Receiver
    {
        get => Component.GetReference("receiver");
        set => Component.SetReference("receiver", value);
    }
    /// <summary>Outro objeto. Opcional: só contatos com este outro objeto</summary>
    public ObjectReference OtherFilter
    {
        get => Component.GetReference("other_filter");
        set => Component.SetReference("other_filter", value);
    }
}

/// <summary>IK de dois ossos: Raiz e meio da cadeia levam a ponta ao alvo, com direção da dobra. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/TwoBoneIKConstraint.html</remarks>
public readonly struct TwoBoneIk : IComponentFacade<TwoBoneIk>
{
    public static string TypeId => "astra.ik.two_bone";
    public static TwoBoneIk Wrap(Component component) => new(component);
    public Component Component { get; }
    public TwoBoneIk(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Prioridade</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float Priority
    {
        get => Component.GetFloat("priority");
        set => Component.SetFloat("priority", value);
    }
    /// <summary>Ângulo do plano (°). Gira o plano da dobra em torno do eixo raiz→ponta; o zero é perpendicular ao osso do meio no repouso.</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float PlaneAngle
    {
        get => Component.GetFloat("plane_angle");
        set => Component.SetFloat("plane_angle", value);
    }
    /// <summary>Rotação do polo (°). Gira a cadeia resolvida em torno do eixo raiz→alvo (sem Direção da dobra).</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float PoleRotation
    {
        get => Component.GetFloat("pole_rotation");
        set => Component.SetFloat("pole_rotation", value);
    }
    /// <summary>Zero do plano X. Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float PlaneZeroX
    {
        get => Component.GetFloat("plane_zero_x");
        set => Component.SetFloat("plane_zero_x", value);
    }
    /// <summary>Zero do plano Y. Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float PlaneZeroY
    {
        get => Component.GetFloat("plane_zero_y");
        set => Component.SetFloat("plane_zero_y", value);
    }
    /// <summary>Zero do plano Z. Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float PlaneZeroZ
    {
        get => Component.GetFloat("plane_zero_z");
        set => Component.SetFloat("plane_zero_z", value);
    }
    /// <summary>Desvio da rotação do alvo X (°). Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float TargetOffsetX
    {
        get => Component.GetFloat("target_offset_x");
        set => Component.SetFloat("target_offset_x", value);
    }
    /// <summary>Desvio da rotação do alvo Y (°). Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float TargetOffsetY
    {
        get => Component.GetFloat("target_offset_y");
        set => Component.SetFloat("target_offset_y", value);
    }
    /// <summary>Desvio da rotação do alvo Z (°). Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float TargetOffsetZ
    {
        get => Component.GetFloat("target_offset_z");
        set => Component.SetFloat("target_offset_z", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Ponta segue rotação do alvo</summary>
    public bool MatchTargetRotation
    {
        get => Component.GetBool("match_target_rotation");
        set => Component.SetBool("match_target_rotation", value);
    }
    public enum SolverOption : uint
    {
        Pose = 0,
        PlanoRotativo = 1,
    }
    /// <summary>Solver</summary>
    public SolverOption Solver
    {
        get => (SolverOption)Component.GetEnum("solver");
        set => Component.SetEnum("solver", (uint)value);
    }
    public enum ReferenceOption : uint
    {
        Animacao = 0,
        PoseDeRepouso = 1,
    }
    /// <summary>Referência. De onde a cadeia parte antes de resolver.</summary>
    public ReferenceOption Reference
    {
        get => (ReferenceOption)Component.GetEnum("reference");
        set => Component.SetEnum("reference", (uint)value);
    }
    /// <summary>Alvo</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
    /// <summary>Direção da dobra</summary>
    public ObjectReference Pole
    {
        get => Component.GetReference("pole");
        set => Component.SetReference("pole", value);
    }
}

/// <summary>IK de cadeia: CCD ou FABRIK em N ossos, com iterações e tolerância. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/ChainIKConstraint.html</remarks>
public readonly struct ChainIk : IComponentFacade<ChainIk>
{
    public static string TypeId => "astra.ik.chain";
    public static ChainIk Wrap(Component component) => new(component);
    public Component Component { get; }
    public ChainIk(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Ângulo do plano (°). Gira o plano da dobra em torno do eixo raiz→ponta.</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float PlaneAngle
    {
        get => Component.GetFloat("plane_angle");
        set => Component.SetFloat("plane_angle", value);
    }
    /// <summary>Rotação do polo (°). Gira a cadeia resolvida em torno do eixo raiz→alvo.</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float PoleRotation
    {
        get => Component.GetFloat("pole_rotation");
        set => Component.SetFloat("pole_rotation", value);
    }
    /// <summary>Prioridade</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float Priority
    {
        get => Component.GetFloat("priority");
        set => Component.SetFloat("priority", value);
    }
    /// <summary>Ossos na cadeia (ossos)</summary>
    /// <remarks>Faixa válida: 2 a 32.</remarks>
    public float ChainLength
    {
        get => Component.GetFloat("chain_length");
        set => Component.SetFloat("chain_length", value);
    }
    /// <summary>Iterações máximas</summary>
    /// <remarks>Faixa válida: 1 a 128.</remarks>
    public float Iterations
    {
        get => Component.GetFloat("iterations");
        set => Component.SetFloat("iterations", value);
    }
    /// <summary>Tolerância (m)</summary>
    /// <remarks>Faixa válida: 0.00001 a 1.</remarks>
    public float Tolerance
    {
        get => Component.GetFloat("tolerance");
        set => Component.SetFloat("tolerance", value);
    }
    /// <summary>Zero do plano X. Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float PlaneZeroX
    {
        get => Component.GetFloat("plane_zero_x");
        set => Component.SetFloat("plane_zero_x", value);
    }
    /// <summary>Zero do plano Y. Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float PlaneZeroY
    {
        get => Component.GetFloat("plane_zero_y");
        set => Component.SetFloat("plane_zero_y", value);
    }
    /// <summary>Zero do plano Z. Direção do zero do ângulo no espaço do osso raiz; zerado usa a perpendicular do osso do meio.</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float PlaneZeroZ
    {
        get => Component.GetFloat("plane_zero_z");
        set => Component.SetFloat("plane_zero_z", value);
    }
    /// <summary>Desvio da rotação do alvo X (°). Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float TargetOffsetX
    {
        get => Component.GetFloat("target_offset_x");
        set => Component.SetFloat("target_offset_x", value);
    }
    /// <summary>Desvio da rotação do alvo Y (°). Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float TargetOffsetY
    {
        get => Component.GetFloat("target_offset_y");
        set => Component.SetFloat("target_offset_y", value);
    }
    /// <summary>Desvio da rotação do alvo Z (°). Rotação somada à do alvo quando a ponta segue a rotação do alvo (eixos do alvo).</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float TargetOffsetZ
    {
        get => Component.GetFloat("target_offset_z");
        set => Component.SetFloat("target_offset_z", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Ponta segue rotação do alvo</summary>
    public bool MatchTargetRotation
    {
        get => Component.GetBool("match_target_rotation");
        set => Component.SetBool("match_target_rotation", value);
    }
    public enum SolverOption : uint
    {
        CCD = 0,
        FABRIK = 1,
        PlanoRotativo = 2,
    }
    /// <summary>Solver</summary>
    public SolverOption Solver
    {
        get => (SolverOption)Component.GetEnum("solver");
        set => Component.SetEnum("solver", (uint)value);
    }
    public enum ReferenceOption : uint
    {
        Animacao = 0,
        PoseDeRepouso = 1,
    }
    /// <summary>Referência. De onde a cadeia parte antes de resolver.</summary>
    public ReferenceOption Reference
    {
        get => (ReferenceOption)Component.GetEnum("reference");
        set => Component.SetEnum("reference", (uint)value);
    }
    /// <summary>Alvo</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
    /// <summary>Direção da dobra</summary>
    public ObjectReference Pole
    {
        get => Component.GetReference("pole");
        set => Component.SetReference("pole", value);
    }
}

/// <summary>Apoio do pé: Raio pela física apoia o pé na superfície e resolve a perna. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_skeletonmodifier3d.html</remarks>
public readonly struct FootGrounding : IComponentFacade<FootGrounding>
{
    public static string TypeId => "astra.ik.foot_grounding";
    public static FootGrounding Wrap(Component component) => new(component);
    public Component Component { get; }
    public FootGrounding(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Início do raio acima do pé (m)</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float RayHeight
    {
        get => Component.GetFloat("ray_height");
        set => Component.SetFloat("ray_height", value);
    }
    /// <summary>Alcance abaixo do pé (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 20.</remarks>
    public float RayDistance
    {
        get => Component.GetFloat("ray_distance");
        set => Component.SetFloat("ray_distance", value);
    }
    /// <summary>Altura do pé sobre a superfície (m)</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float FootOffset
    {
        get => Component.GetFloat("foot_offset");
        set => Component.SetFloat("foot_offset", value);
    }
    /// <summary>Correção máxima (m)</summary>
    /// <remarks>Faixa válida: 0 a 5.</remarks>
    public float MaxCorrection
    {
        get => Component.GetFloat("max_correction");
        set => Component.SetFloat("max_correction", value);
    }
    /// <summary>Altura de contato (m)</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float LockHeight
    {
        get => Component.GetFloat("lock_height");
        set => Component.SetFloat("lock_height", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Alinhar à superfície</summary>
    public bool AlignToSurface
    {
        get => Component.GetBool("align_to_surface");
        set => Component.SetBool("align_to_surface", value);
    }
    /// <summary>Travar pé no apoio</summary>
    public bool LockFoot
    {
        get => Component.GetBool("lock_foot");
        set => Component.SetBool("lock_foot", value);
    }
}

/// <summary>Look At IK: Coluna e cabeça olham para o alvo com pesos e ângulo máximo. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/MultiAimConstraint.html</remarks>
public readonly struct LookAtIk : IComponentFacade<LookAtIk>
{
    public static string TypeId => "astra.ik.look_at";
    public static LookAtIk Wrap(Component component) => new(component);
    public Component Component { get; }
    public LookAtIk(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Prioridade</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float Priority
    {
        get => Component.GetFloat("priority");
        set => Component.SetFloat("priority", value);
    }
    /// <summary>Peso do corpo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float BodyWeight
    {
        get => Component.GetFloat("body_weight");
        set => Component.SetFloat("body_weight", value);
    }
    /// <summary>Peso da cabeça</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float HeadWeight
    {
        get => Component.GetFloat("head_weight");
        set => Component.SetFloat("head_weight", value);
    }
    /// <summary>Ossos na cadeia (ossos)</summary>
    /// <remarks>Faixa válida: 1 a 16.</remarks>
    public float ChainLength
    {
        get => Component.GetFloat("chain_length");
        set => Component.SetFloat("chain_length", value);
    }
    /// <summary>Ângulo máximo (graus)</summary>
    /// <remarks>Faixa válida: 0 a 180.</remarks>
    public float ClampAngle
    {
        get => Component.GetFloat("clamp_angle");
        set => Component.SetFloat("clamp_angle", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum AxisOption : uint
    {
        X = 0,
        Y = 1,
        Z = 2,
        X3 = 3,
        Y4 = 4,
        Z5 = 5,
    }
    /// <summary>Eixo frontal</summary>
    public AxisOption Axis
    {
        get => (AxisOption)Component.GetEnum("axis");
        set => Component.SetEnum("axis", (uint)value);
    }
    /// <summary>Alvo</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Aim IK: Cadeia gira para o eixo da ponta mirar o alvo. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/MultiAimConstraint.html</remarks>
public readonly struct AimIk : IComponentFacade<AimIk>
{
    public static string TypeId => "astra.ik.aim";
    public static AimIk Wrap(Component component) => new(component);
    public Component Component { get; }
    public AimIk(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Prioridade</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float Priority
    {
        get => Component.GetFloat("priority");
        set => Component.SetFloat("priority", value);
    }
    /// <summary>Ossos na cadeia (ossos)</summary>
    /// <remarks>Faixa válida: 1 a 16.</remarks>
    public float ChainLength
    {
        get => Component.GetFloat("chain_length");
        set => Component.SetFloat("chain_length", value);
    }
    /// <summary>Iterações</summary>
    /// <remarks>Faixa válida: 1 a 32.</remarks>
    public float Iterations
    {
        get => Component.GetFloat("iterations");
        set => Component.SetFloat("iterations", value);
    }
    /// <summary>Queda do peso para a base</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Falloff
    {
        get => Component.GetFloat("falloff");
        set => Component.SetFloat("falloff", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum AxisOption : uint
    {
        X = 0,
        Y = 1,
        Z = 2,
        X3 = 3,
        Y4 = 4,
        Z5 = 5,
    }
    /// <summary>Eixo de mira</summary>
    public AxisOption Axis
    {
        get => (AxisOption)Component.GetEnum("axis");
        set => Component.SetEnum("axis", (uint)value);
    }
    /// <summary>Alvo</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Limite de rotação: Dobradiça ou cone aplicados ao osso depois do IK. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/TwoBoneIKConstraint.html</remarks>
public readonly struct RotationLimit : IComponentFacade<RotationLimit>
{
    public static string TypeId => "astra.ik.rotation_limit";
    public static RotationLimit Wrap(Component component) => new(component);
    public Component Component { get; }
    public RotationLimit(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Mínimo (graus)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float Minimum
    {
        get => Component.GetFloat("minimum");
        set => Component.SetFloat("minimum", value);
    }
    /// <summary>Máximo (graus)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float Maximum
    {
        get => Component.GetFloat("maximum");
        set => Component.SetFloat("maximum", value);
    }
    /// <summary>Cone (graus)</summary>
    /// <remarks>Faixa válida: 0 a 180.</remarks>
    public float Cone
    {
        get => Component.GetFloat("cone");
        set => Component.SetFloat("cone", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum KindOption : uint
    {
        Livre = 0,
        Dobradica = 1,
        ConeETorcao = 2,
    }
    /// <summary>Tipo</summary>
    public KindOption Kind
    {
        get => (KindOption)Component.GetEnum("kind");
        set => Component.SetEnum("kind", (uint)value);
    }
    public enum AxisOption : uint
    {
        X = 0,
        Y = 1,
        Z = 2,
    }
    /// <summary>Eixo</summary>
    public AxisOption Axis
    {
        get => (AxisOption)Component.GetEnum("axis");
        set => Component.SetEnum("axis", (uint)value);
    }
}

/// <summary>Grounder do corpo: Desce a pelve para os pés alcançarem o chão (bípede ou quadrúpede). Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_skeletonmodifier3d.html</remarks>
public readonly struct GrounderBody : IComponentFacade<GrounderBody>
{
    public static string TypeId => "astra.ik.grounder";
    public static GrounderBody Wrap(Component component) => new(component);
    public Component Component { get; }
    public GrounderBody(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Descida máxima da pelve (m)</summary>
    /// <remarks>Faixa válida: 0 a 5.</remarks>
    public float MaxPelvisOffset
    {
        get => Component.GetFloat("max_pelvis_offset");
        set => Component.SetFloat("max_pelvis_offset", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Pelve</summary>
    public ObjectReference Pelvis
    {
        get => Component.GetReference("pelvis");
        set => Component.SetReference("pelvis", value);
    }
}

/// <summary>Curvatura de dedo: Fecha ou abre a cadeia do dedo no eixo local. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/ChainIKConstraint.html</remarks>
public readonly struct FingerCurl : IComponentFacade<FingerCurl>
{
    public static string TypeId => "astra.ik.finger_curl";
    public static FingerCurl Wrap(Component component) => new(component);
    public Component Component { get; }
    public FingerCurl(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Fechamento</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float Curl
    {
        get => Component.GetFloat("curl");
        set => Component.SetFloat("curl", value);
    }
    /// <summary>Ângulo por osso (graus)</summary>
    /// <remarks>Faixa válida: 0 a 180.</remarks>
    public float MaxAngle
    {
        get => Component.GetFloat("max_angle");
        set => Component.SetFloat("max_angle", value);
    }
    /// <summary>Ossos no dedo (ossos)</summary>
    /// <remarks>Faixa válida: 1 a 6.</remarks>
    public float ChainLength
    {
        get => Component.GetFloat("chain_length");
        set => Component.SetFloat("chain_length", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum AxisOption : uint
    {
        X = 0,
        Y = 1,
        Z = 2,
        X3 = 3,
        Y4 = 4,
        Z5 = 5,
    }
    /// <summary>Eixo de dobra</summary>
    public AxisOption Axis
    {
        get => (AxisOption)Component.GetEnum("axis");
        set => Component.SetEnum("axis", (uint)value);
    }
}

/// <summary>Offset de efetor: Desloca o alvo do IK desta ponta. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/TwoBoneIKConstraint.html</remarks>
public readonly struct IkOffset : IComponentFacade<IkOffset>
{
    public static string TypeId => "astra.ik.offset";
    public static IkOffset Wrap(Component component) => new(component);
    public Component Component { get; }
    public IkOffset(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Offset do alvo</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Relaxar torção: Divide a torção do filho com este osso. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/TwistCorrection.html</remarks>
public readonly struct TwistRelaxer : IComponentFacade<TwistRelaxer>
{
    public static string TypeId => "astra.ik.twist_relaxer";
    public static TwistRelaxer Wrap(Component component) => new(component);
    public Component Component { get; }
    public TwistRelaxer(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Parte da torção</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Reação a impacto: Gira a cadeia para longe do golpe e volta. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/MultiRotationConstraint.html</remarks>
public readonly struct HitReaction : IComponentFacade<HitReaction>
{
    public static string TypeId => "astra.ik.hit_reaction";
    public static HitReaction Wrap(Component component) => new(component);
    public Component Component { get; }
    public HitReaction(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Força do golpe (graus)</summary>
    /// <remarks>Faixa válida: 0 a 90.</remarks>
    public float Strength
    {
        get => Component.GetFloat("strength");
        set => Component.SetFloat("strength", value);
    }
    /// <summary>Recuperação (s)</summary>
    /// <remarks>Faixa válida: 0.05 a 5.</remarks>
    public float Duration
    {
        get => Component.GetFloat("duration");
        set => Component.SetFloat("duration", value);
    }
    /// <summary>Ossos que reagem (ossos)</summary>
    /// <remarks>Faixa válida: 1 a 8.</remarks>
    public float ChainLength
    {
        get => Component.GetFloat("chain_length");
        set => Component.SetFloat("chain_length", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Reagir a colisões</summary>
    public bool ReactToCollisions
    {
        get => Component.GetBool("react_to_collisions");
        set => Component.SetBool("react_to_collisions", value);
    }
    public enum AxisOption : uint
    {
        X = 0,
        Y = 1,
        Z = 2,
        X3 = 3,
        Y4 = 4,
        Z5 = 5,
    }
    /// <summary>Frente do osso</summary>
    public AxisOption Axis
    {
        get => (AxisOption)Component.GetEnum("axis");
        set => Component.SetEnum("axis", (uint)value);
    }
    /// <summary>Golpear. Gira a cadeia para longe da direção; força 1 usa a força autorada</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Hit(Vector3 direction, double force) => Component.Invoke("hit", ComponentValue.Vector(direction), ComponentValue.Number(force));
    /// <summary>Golpear de frente. Golpe contra a Frente do osso (empurra para trás); usado pelas Conexões de evento</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void HitFront(double force) => Component.Invoke("hit_front", ComponentValue.Number(force));
}

/// <summary>Recuo: Levanta e recua a cadeia a cada disparo. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/MultiAimConstraint.html</remarks>
public readonly struct Recoil : IComponentFacade<Recoil>
{
    public static string TypeId => "astra.ik.recoil";
    public static Recoil Wrap(Component component) => new(component);
    public Component Component { get; }
    public Recoil(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Levantar (graus)</summary>
    /// <remarks>Faixa válida: 0 a 60.</remarks>
    public float KickAngle
    {
        get => Component.GetFloat("kick_angle");
        set => Component.SetFloat("kick_angle", value);
    }
    /// <summary>Recuar (m)</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float KickBack
    {
        get => Component.GetFloat("kick_back");
        set => Component.SetFloat("kick_back", value);
    }
    /// <summary>Recuperação (s)</summary>
    /// <remarks>Faixa válida: 0.02 a 3.</remarks>
    public float Duration
    {
        get => Component.GetFloat("duration");
        set => Component.SetFloat("duration", value);
    }
    /// <summary>Ossos no recuo (ossos)</summary>
    /// <remarks>Faixa válida: 1 a 8.</remarks>
    public float ChainLength
    {
        get => Component.GetFloat("chain_length");
        set => Component.SetFloat("chain_length", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum AxisOption : uint
    {
        X = 0,
        Y = 1,
        Z = 2,
        X3 = 3,
        Y4 = 4,
        Z5 = 5,
    }
    /// <summary>Eixo do cano</summary>
    public AxisOption Axis
    {
        get => (AxisOption)Component.GetEnum("axis");
        set => Component.SetEnum("axis", (uint)value);
    }
    /// <summary>Disparar. Aplica um recuo; intensidade 1 usa os valores autorados</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Fire(double magnitude) => Component.Invoke("fire", ComponentValue.Number(magnitude));
}

/// <summary>Inércia: A ponta do osso segue a pose por uma mola. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/DampedTransform.html</remarks>
public readonly struct Inertia : IComponentFacade<Inertia>
{
    public static string TypeId => "astra.ik.inertia";
    public static Inertia Wrap(Component component) => new(component);
    public Component Component { get; }
    public Inertia(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Rigidez</summary>
    /// <remarks>Faixa válida: 1 a 400.</remarks>
    public float Stiffness
    {
        get => Component.GetFloat("stiffness");
        set => Component.SetFloat("stiffness", value);
    }
    /// <summary>Amortecimento</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Damping
    {
        get => Component.GetFloat("damping");
        set => Component.SetFloat("damping", value);
    }
    /// <summary>Desvio máximo (graus)</summary>
    /// <remarks>Faixa válida: 0 a 120.</remarks>
    public float MaxAngle
    {
        get => Component.GetFloat("max_angle");
        set => Component.SetFloat("max_angle", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Amplificador: Exagera a rotação animada do osso. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/MultiRotationConstraint.html</remarks>
public readonly struct Amplifier : IComponentFacade<Amplifier>
{
    public static string TypeId => "astra.ik.amplifier";
    public static Amplifier Wrap(Component component) => new(component);
    public Component Component { get; }
    public Amplifier(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Exagero</summary>
    /// <remarks>Faixa válida: 0 a 3.</remarks>
    public float Amount
    {
        get => Component.GetFloat("amount");
        set => Component.SetFloat("amount", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Inclinação do corpo: Inclina o osso para dentro da curva. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/OverrideTransform.html</remarks>
public readonly struct BodyTilt : IComponentFacade<BodyTilt>
{
    public static string TypeId => "astra.ik.body_tilt";
    public static BodyTilt Wrap(Component component) => new(component);
    public Component Component { get; }
    public BodyTilt(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Inclinação por giro (graus por grau/s)</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float TiltPerTurn
    {
        get => Component.GetFloat("tilt_per_turn");
        set => Component.SetFloat("tilt_per_turn", value);
    }
    /// <summary>Inclinação máxima (graus)</summary>
    /// <remarks>Faixa válida: 0 a 60.</remarks>
    public float MaxAngle
    {
        get => Component.GetFloat("max_angle");
        set => Component.SetFloat("max_angle", value);
    }
    /// <summary>Suavização (s)</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float Smoothing
    {
        get => Component.GetFloat("smoothing");
        set => Component.SetFloat("smoothing", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum AxisOption : uint
    {
        X = 0,
        Y = 1,
        Z = 2,
        X3 = 3,
        Y4 = 4,
        Z5 = 5,
    }
    /// <summary>Eixo frontal do personagem</summary>
    public AxisOption Axis
    {
        get => (AxisOption)Component.GetEnum("axis");
        set => Component.SetEnum("axis", (uint)value);
    }
}

/// <summary>Poser de mão: Copia a pose de uma hierarquia de referência. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/OverrideTransform.html</remarks>
public readonly struct HandPoser : IComponentFacade<HandPoser>
{
    public static string TypeId => "astra.ik.hand_poser";
    public static HandPoser Wrap(Component component) => new(component);
    public Component Component { get; }
    public HandPoser(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Incluir este osso</summary>
    public bool IncludeSelf
    {
        get => Component.GetBool("include_self");
        set => Component.SetBool("include_self", value);
    }
    /// <summary>Pose de referência</summary>
    public ObjectReference Reference
    {
        get => Component.GetReference("reference");
        set => Component.SetReference("reference", value);
    }
}

/// <summary>Evitar penetração: Para o alvo do IK na superfície do colisor. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_skeletonmodifier3d.html</remarks>
public readonly struct PenetrationAvoidance : IComponentFacade<PenetrationAvoidance>
{
    public static string TypeId => "astra.ik.penetration_avoidance";
    public static PenetrationAvoidance Wrap(Component component) => new(component);
    public Component Component { get; }
    public PenetrationAvoidance(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Folga da superfície (m)</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Objeto interagível: Ponto de pegada, pose da mão e tempos. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/TwoBoneIKConstraint.html</remarks>
public readonly struct InteractionObject : IComponentFacade<InteractionObject>
{
    public static string TypeId => "astra.ik.interaction_object";
    public static InteractionObject Wrap(Component component) => new(component);
    public Component Component { get; }
    public InteractionObject(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Tempo para alcançar (s)</summary>
    /// <remarks>Faixa válida: 0.05 a 5.</remarks>
    public float ReachTime
    {
        get => Component.GetFloat("reach_time");
        set => Component.SetFloat("reach_time", value);
    }
    /// <summary>Tempo para soltar (s)</summary>
    /// <remarks>Faixa válida: 0.05 a 5.</remarks>
    public float ReleaseTime
    {
        get => Component.GetFloat("release_time");
        set => Component.SetFloat("release_time", value);
    }
    /// <summary>Tempo para levantar (s)</summary>
    /// <remarks>Faixa válida: 0.05 a 5.</remarks>
    public float LiftTime
    {
        get => Component.GetFloat("lift_time");
        set => Component.SetFloat("lift_time", value);
    }
    /// <summary>Pegar ao alcançar</summary>
    public bool PickUp
    {
        get => Component.GetBool("pick_up");
        set => Component.SetBool("pick_up", value);
    }
    /// <summary>Ponto de pegada</summary>
    public ObjectReference Grip
    {
        get => Component.GetReference("grip");
        set => Component.SetReference("grip", value);
    }
    /// <summary>Pegada da 2ª mão</summary>
    public ObjectReference SecondGrip
    {
        get => Component.GetReference("second_grip");
        set => Component.SetReference("second_grip", value);
    }
    /// <summary>Pose da mão</summary>
    public ObjectReference HandPose
    {
        get => Component.GetReference("hand_pose");
        set => Component.SetReference("hand_pose", value);
    }
    /// <summary>Ponto de carregar</summary>
    public ObjectReference HoldPoint
    {
        get => Component.GetReference("hold_point");
        set => Component.SetReference("hold_point", value);
    }
}

/// <summary>Sistema de interação: Leva a mão até objetos interagíveis. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/TwoBoneIKConstraint.html</remarks>
public readonly struct InteractionSystem : IComponentFacade<InteractionSystem>
{
    public static string TypeId => "astra.ik.interaction_system";
    public static InteractionSystem Wrap(Component component) => new(component);
    public Component Component { get; }
    public InteractionSystem(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Velocidade</summary>
    /// <remarks>Faixa válida: 0.1 a 5.</remarks>
    public float Speed
    {
        get => Component.GetFloat("speed");
        set => Component.SetFloat("speed", value);
    }
    /// <summary>Ossos do braço (ossos)</summary>
    /// <remarks>Faixa válida: 2 a 8.</remarks>
    public float LimbBones
    {
        get => Component.GetFloat("limb_bones");
        set => Component.SetFloat("limb_bones", value);
    }
    /// <summary>Agachar até (m)</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float Crouch
    {
        get => Component.GetFloat("crouch");
        set => Component.SetFloat("crouch", value);
    }
    /// <summary>Inclinar o tronco</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Lean
    {
        get => Component.GetFloat("lean");
        set => Component.SetFloat("lean", value);
    }
    /// <summary>Entrada do olhar (/s)</summary>
    /// <remarks>Faixa válida: 0.1 a 20.</remarks>
    public float LookSpeed
    {
        get => Component.GetFloat("look_speed");
        set => Component.SetFloat("look_speed", value);
    }
    /// <summary>Giro do olhar (/s)</summary>
    /// <remarks>Faixa válida: 0.5 a 60.</remarks>
    public float LookTurn
    {
        get => Component.GetFloat("look_turn");
        set => Component.SetFloat("look_turn", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Mão ao iniciar</summary>
    public ObjectReference AutoEffector
    {
        get => Component.GetReference("auto_effector");
        set => Component.SetReference("auto_effector", value);
    }
    /// <summary>Objeto ao iniciar</summary>
    public ObjectReference AutoObject
    {
        get => Component.GetReference("auto_object");
        set => Component.SetReference("auto_object", value);
    }
    /// <summary>Segunda mão ao iniciar</summary>
    public ObjectReference AutoSecond
    {
        get => Component.GetReference("auto_second");
        set => Component.SetReference("auto_second", value);
    }
    /// <summary>Pelve</summary>
    public ObjectReference Pelvis
    {
        get => Component.GetReference("pelvis");
        set => Component.SetReference("pelvis", value);
    }
    /// <summary>Peito</summary>
    public ObjectReference Chest
    {
        get => Component.GetReference("chest");
        set => Component.SetReference("chest", value);
    }
    /// <summary>Olhar com</summary>
    public ObjectReference LookAt
    {
        get => Component.GetReference("look_at");
        set => Component.SetReference("look_at", value);
    }
    /// <summary>Interagir. Leva a mão até o Objeto interagível; falso se a mão ou o objeto não servem</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool Start(ObjectReference effector, ObjectReference @object) => Component.Invoke("start", ComponentValue.Object(effector), ComponentValue.Object(@object)).AsBoolean();
    /// <summary>Soltar. Devolve a mão à animação e larga o objeto pego</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Stop(ObjectReference effector) => Component.Invoke("stop", ComponentValue.Object(effector));
    /// <summary>Interagir pelo gatilho. Começa a interação do Gatilho de interação mais próximo em que o personagem está; falso se não há nenhum</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool Trigger() => Component.Invoke("trigger").AsBoolean();
    /// <summary>Alcançou. A mão chegou ao ponto de pegada. Payload: Mão: objeto, Objeto: objeto</summary>
    public ComponentSubscription OnReached(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "reached", handler);
    /// <summary>Levantou. A última mão chegou e o objeto sobe ao ponto de carregar. Payload: Mão: objeto, Objeto: objeto</summary>
    public ComponentSubscription OnPicked(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "picked", handler);
    /// <summary>Soltou. A mão voltou à animação. Payload: Mão: objeto, Objeto: objeto</summary>
    public ComponentSubscription OnReleased(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "released", handler);
    /// <summary>Entrou no gatilho. O personagem entrou na faixa de um Gatilho de interação (Objeto = o gatilho). Payload: Mão: objeto, Objeto: objeto</summary>
    public ComponentSubscription OnEntered(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "entered", handler);
    /// <summary>Saiu do gatilho. O personagem saiu da faixa do gatilho. Payload: Mão: objeto, Objeto: objeto</summary>
    public ComponentSubscription OnLeft(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "left", handler);
}

/// <summary>Pivô de pegada: Gira o objeto sobre este pivô para a pegada encarar a mão. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.xr.interaction.toolkit@3.0/manual/xr-grab-interactable.html</remarks>
public readonly struct InteractionGrip : IComponentFacade<InteractionGrip>
{
    public static string TypeId => "astra.ik.interaction_grip";
    public static InteractionGrip Wrap(Component component) => new(component);
    public Component Component { get; }
    public InteractionGrip(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso da torção</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float TwistWeight
    {
        get => Component.GetFloat("twist_weight");
        set => Component.SetFloat("twist_weight", value);
    }
    /// <summary>Peso do balanço</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float SwingWeight
    {
        get => Component.GetFloat("swing_weight");
        set => Component.SetFloat("swing_weight", value);
    }
    /// <summary>Peso da rotação</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FullWeight
    {
        get => Component.GetFloat("full_weight");
        set => Component.SetFloat("full_weight", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Girar uma vez</summary>
    public bool RotateOnce
    {
        get => Component.GetBool("rotate_once");
        set => Component.SetBool("rotate_once", value);
    }
    public enum ModeOption : uint
    {
        TorcaoEBalanco = 0,
        RotacaoCompleta = 1,
    }
    /// <summary>Modo</summary>
    public ModeOption Mode
    {
        get => (ModeOption)Component.GetEnum("mode");
        set => Component.SetEnum("mode", (uint)value);
    }
    public enum AxisOption : uint
    {
        X = 0,
        Y = 1,
        Z = 2,
        X3 = 3,
        Y4 = 4,
        Z5 = 5,
    }
    /// <summary>Eixo da torção</summary>
    public AxisOption Axis
    {
        get => (AxisOption)Component.GetEnum("axis");
        set => Component.SetEnum("axis", (uint)value);
    }
    /// <summary>Pegada</summary>
    public ObjectReference Grip
    {
        get => Component.GetReference("grip");
        set => Component.SetReference("grip", value);
    }
}

/// <summary>Gatilho de interação: Faixa à frente do objeto: o personagem dentro dela interage. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.xr.interaction.toolkit@3.0/manual/xr-direct-interactor.html</remarks>
public readonly struct InteractionTrigger : IComponentFacade<InteractionTrigger>
{
    public static string TypeId => "astra.ik.interaction_trigger";
    public static InteractionTrigger Wrap(Component component) => new(component);
    public Component Component { get; }
    public InteractionTrigger(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.05 a 10.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Ângulo máximo (graus)</summary>
    /// <remarks>Faixa válida: 1 a 180.</remarks>
    public float MaxAngle
    {
        get => Component.GetFloat("max_angle");
        set => Component.SetFloat("max_angle", value);
    }
    /// <summary>Deslocamento lateral (m)</summary>
    /// <remarks>Faixa válida: -5 a 5.</remarks>
    public float OffsetX
    {
        get => Component.GetFloat("offset_x");
        set => Component.SetFloat("offset_x", value);
    }
    /// <summary>Deslocamento à frente (m)</summary>
    /// <remarks>Faixa válida: -5 a 5.</remarks>
    public float OffsetZ
    {
        get => Component.GetFloat("offset_z");
        set => Component.SetFloat("offset_z", value);
    }
    /// <summary>Giro da direção (graus)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float AngleOffset
    {
        get => Component.GetFloat("angle_offset");
        set => Component.SetFloat("angle_offset", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Automático</summary>
    public bool Automatic
    {
        get => Component.GetBool("automatic");
        set => Component.SetBool("automatic", value);
    }
    /// <summary>Em volta do objeto</summary>
    public bool Orbit
    {
        get => Component.GetBool("orbit");
        set => Component.SetBool("orbit", value);
    }
    /// <summary>Cima fixo</summary>
    public bool FixUp
    {
        get => Component.GetBool("fix_up");
        set => Component.SetBool("fix_up", value);
    }
    public enum HandsOption : uint
    {
        MaoAoIniciar = 0,
        SegundaMaoAoIniciar = 1,
        AsDuas = 2,
    }
    /// <summary>Mãos</summary>
    public HandsOption Hands
    {
        get => (HandsOption)Component.GetEnum("hands");
        set => Component.SetEnum("hands", (uint)value);
    }
    /// <summary>Objeto interagível</summary>
    public ObjectReference Interactable
    {
        get => Component.GetReference("interactable");
        set => Component.SetReference("interactable", value);
    }
}

/// <summary>IK de corpo inteiro: Puxa a pelve para alcançar alvos e resolve os membros. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_skeletonmodifier3d.html</remarks>
public readonly struct FullBodyIk : IComponentFacade<FullBodyIk>
{
    public static string TypeId => "astra.ik.full_body";
    public static FullBodyIk Wrap(Component component) => new(component);
    public Component Component { get; }
    public FullBodyIk(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Puxar o corpo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float BodyPull
    {
        get => Component.GetFloat("body_pull");
        set => Component.SetFloat("body_pull", value);
    }
    /// <summary>Deslocamento máximo da pelve (m)</summary>
    /// <remarks>Faixa válida: 0 a 3.</remarks>
    public float MaxBodyOffset
    {
        get => Component.GetFloat("max_body_offset");
        set => Component.SetFloat("max_body_offset", value);
    }
    /// <summary>Curvar o tronco</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float SpineBend
    {
        get => Component.GetFloat("spine_bend");
        set => Component.SetFloat("spine_bend", value);
    }
    /// <summary>Ossos por membro (ossos)</summary>
    /// <remarks>Faixa válida: 2 a 8.</remarks>
    public float LimbBones
    {
        get => Component.GetFloat("limb_bones");
        set => Component.SetFloat("limb_bones", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Pelve</summary>
    public ObjectReference Pelvis
    {
        get => Component.GetReference("pelvis");
        set => Component.SetReference("pelvis", value);
    }
    /// <summary>Peito</summary>
    public ObjectReference Chest
    {
        get => Component.GetReference("chest");
        set => Component.SetReference("chest", value);
    }
    /// <summary>Ponta 1 (mão esq.)</summary>
    public ObjectReference Tip1
    {
        get => Component.GetReference("tip_1");
        set => Component.SetReference("tip_1", value);
    }
    /// <summary>Alvo 1</summary>
    public ObjectReference Target1
    {
        get => Component.GetReference("target_1");
        set => Component.SetReference("target_1", value);
    }
    /// <summary>Ponta 2 (mão dir.)</summary>
    public ObjectReference Tip2
    {
        get => Component.GetReference("tip_2");
        set => Component.SetReference("tip_2", value);
    }
    /// <summary>Alvo 2</summary>
    public ObjectReference Target2
    {
        get => Component.GetReference("target_2");
        set => Component.SetReference("target_2", value);
    }
    /// <summary>Ponta 3 (pé esq.)</summary>
    public ObjectReference Tip3
    {
        get => Component.GetReference("tip_3");
        set => Component.SetReference("tip_3", value);
    }
    /// <summary>Alvo 3</summary>
    public ObjectReference Target3
    {
        get => Component.GetReference("target_3");
        set => Component.SetReference("target_3", value);
    }
    /// <summary>Ponta 4 (pé dir.)</summary>
    public ObjectReference Tip4
    {
        get => Component.GetReference("tip_4");
        set => Component.SetReference("tip_4", value);
    }
    /// <summary>Alvo 4</summary>
    public ObjectReference Target4
    {
        get => Component.GetReference("target_4");
        set => Component.SetReference("target_4", value);
    }
}

/// <summary>FABRIK em árvore: Várias cadeias com raiz comum resolvidas juntas. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_skeletonmodifier3d.html</remarks>
public readonly struct FabrikRoot : IComponentFacade<FabrikRoot>
{
    public static string TypeId => "astra.ik.fabrik_root";
    public static FabrikRoot Wrap(Component component) => new(component);
    public Component Component { get; }
    public FabrikRoot(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Fixar raiz</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float RootPin
    {
        get => Component.GetFloat("root_pin");
        set => Component.SetFloat("root_pin", value);
    }
    /// <summary>Iterações</summary>
    /// <remarks>Faixa válida: 1 a 64.</remarks>
    public float Iterations
    {
        get => Component.GetFloat("iterations");
        set => Component.SetFloat("iterations", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Ponta 1</summary>
    public ObjectReference Tip1
    {
        get => Component.GetReference("tip_1");
        set => Component.SetReference("tip_1", value);
    }
    /// <summary>Alvo 1</summary>
    public ObjectReference Target1
    {
        get => Component.GetReference("target_1");
        set => Component.SetReference("target_1", value);
    }
    /// <summary>Ponta 2</summary>
    public ObjectReference Tip2
    {
        get => Component.GetReference("tip_2");
        set => Component.SetReference("tip_2", value);
    }
    /// <summary>Alvo 2</summary>
    public ObjectReference Target2
    {
        get => Component.GetReference("target_2");
        set => Component.SetReference("target_2", value);
    }
    /// <summary>Ponta 3</summary>
    public ObjectReference Tip3
    {
        get => Component.GetReference("tip_3");
        set => Component.SetReference("tip_3", value);
    }
    /// <summary>Alvo 3</summary>
    public ObjectReference Target3
    {
        get => Component.GetReference("target_3");
        set => Component.SetReference("target_3", value);
    }
    /// <summary>Ponta 4</summary>
    public ObjectReference Tip4
    {
        get => Component.GetReference("tip_4");
        set => Component.SetReference("tip_4", value);
    }
    /// <summary>Alvo 4</summary>
    public ObjectReference Target4
    {
        get => Component.GetReference("target_4");
        set => Component.SetReference("target_4", value);
    }
}

/// <summary>Corpo por alvos (VRIK): Cabeça e mãos seguem alvos; pelve e tronco acompanham. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.animation.rigging@1.2/manual/constraints/TwoBoneIKConstraint.html</remarks>
public readonly struct BodyByTargets : IComponentFacade<BodyByTargets>
{
    public static string TypeId => "astra.ik.body_by_targets";
    public static BodyByTargets Wrap(Component component) => new(component);
    public Component Component { get; }
    public BodyByTargets(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Corpo segue a cabeça</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float BodyFollow
    {
        get => Component.GetFloat("body_follow");
        set => Component.SetFloat("body_follow", value);
    }
    /// <summary>Curvar o tronco</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float SpineBend
    {
        get => Component.GetFloat("spine_bend");
        set => Component.SetFloat("spine_bend", value);
    }
    /// <summary>Deslocamento máximo da pelve (m)</summary>
    /// <remarks>Faixa válida: 0 a 5.</remarks>
    public float MaxBodyOffset
    {
        get => Component.GetFloat("max_body_offset");
        set => Component.SetFloat("max_body_offset", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Cabeça copia a rotação do alvo</summary>
    public bool HeadRotation
    {
        get => Component.GetBool("head_rotation");
        set => Component.SetBool("head_rotation", value);
    }
    /// <summary>Pelve</summary>
    public ObjectReference Pelvis
    {
        get => Component.GetReference("pelvis");
        set => Component.SetReference("pelvis", value);
    }
    /// <summary>Peito</summary>
    public ObjectReference Chest
    {
        get => Component.GetReference("chest");
        set => Component.SetReference("chest", value);
    }
    /// <summary>Cabeça</summary>
    public ObjectReference Head
    {
        get => Component.GetReference("head");
        set => Component.SetReference("head", value);
    }
    /// <summary>Alvo da cabeça</summary>
    public ObjectReference HeadTarget
    {
        get => Component.GetReference("head_target");
        set => Component.SetReference("head_target", value);
    }
    /// <summary>Mão esquerda</summary>
    public ObjectReference LeftHand
    {
        get => Component.GetReference("left_hand");
        set => Component.SetReference("left_hand", value);
    }
    /// <summary>Alvo esquerdo</summary>
    public ObjectReference LeftTarget
    {
        get => Component.GetReference("left_target");
        set => Component.SetReference("left_target", value);
    }
    /// <summary>Mão direita</summary>
    public ObjectReference RightHand
    {
        get => Component.GetReference("right_hand");
        set => Component.SetReference("right_hand", value);
    }
    /// <summary>Alvo direito</summary>
    public ObjectReference RightTarget
    {
        get => Component.GetReference("right_target");
        set => Component.SetReference("right_target", value);
    }
}

/// <summary>Encaixe: Acessório preso a um osso com deslocamento; segue a pose final. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_boneattachment3d.html</remarks>
public readonly struct Socket : IComponentFacade<Socket>
{
    public static string TypeId => "astra.ik.socket";
    public static Socket Wrap(Component component) => new(component);
    public Component Component { get; }
    public Socket(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Posição</summary>
    public Vector3 Position
    {
        get => new(Component.GetFloat("position_x"), Component.GetFloat("position_y"), Component.GetFloat("position_z"));
        set => Component.SetVector3("position", value);
    }
    /// <summary>Rotação</summary>
    public Vector3 Rotation
    {
        get => new(Component.GetFloat("rotation_x"), Component.GetFloat("rotation_y"), Component.GetFloat("rotation_z"));
        set => Component.SetVector3("rotation", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Seguir escala do osso</summary>
    public bool FollowScale
    {
        get => Component.GetBool("follow_scale");
        set => Component.SetBool("follow_scale", value);
    }
    /// <summary>Osso</summary>
    public ObjectReference Bone
    {
        get => Component.GetReference("bone");
        set => Component.SetReference("bone", value);
    }
}

/// <summary>Filho de: Troca o pai do objeto durante a animação; os canais locais valem no espaço do pai escolhido. Família Lógica · IK.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-ParentConstraint.html</remarks>
public readonly struct ChildOf : IComponentFacade<ChildOf>
{
    public static string TypeId => "astra.constraint.child_of";
    public static ChildOf Wrap(Component component) => new(component);
    public Component Component { get; }
    public ChildOf(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Fixo no personagem. Os canais valem no espaço da raiz do personagem (IK pinning).</summary>
    public bool Pinned
    {
        get => Component.GetBool("pinned");
        set => Component.SetBool("pinned", value);
    }
    /// <summary>Pai. Os canais locais do objeto valem no espaço deste pai; troque com uma chave.</summary>
    public ObjectReference Parent
    {
        get => Component.GetReference("parent");
        set => Component.SetReference("parent", value);
    }
}

/// <summary>Canvas UI: Instância independente de documento de UI, na tela ou no objeto. Família Renderização · Interface.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/class-Canvas.html</remarks>
public readonly struct UiCanvas : IComponentFacade<UiCanvas>
{
    public static string TypeId => "astra.ui.canvas";
    public static UiCanvas Wrap(Component component) => new(component);
    public Component Component { get; }
    public UiCanvas(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Deslocamento local</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Rotação local</summary>
    public Vector3 Rotation
    {
        get => new(Component.GetFloat("rotation_x"), Component.GetFloat("rotation_y"), Component.GetFloat("rotation_z"));
        set => Component.SetVector3("rotation", value);
    }
    /// <summary>Resolução</summary>
    public Vector2 Resolution
    {
        get => new(Component.GetFloat("width"), Component.GetFloat("height"));
        set => Component.SetVector2("resolution", value);
    }
    /// <summary>Unidades por pixel (m/px). Aplica-se ao Canvas no mundo; Tela usa o viewport.</summary>
    /// <remarks>Faixa válida: 0.00001 a 10.</remarks>
    public float UnitsPerPixel
    {
        get => Component.GetFloat("units_per_pixel");
        set => Component.SetFloat("units_per_pixel", value);
    }
    /// <summary>Ordem</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float Order
    {
        get => Component.GetFloat("order");
        set => Component.SetFloat("order", value);
    }
    /// <summary>Habilitado</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Oclusão no mundo. Depth e hit de Canvas no mundo.</summary>
    public bool Occlusion
    {
        get => Component.GetBool("occlusion");
        set => Component.SetBool("occlusion", value);
    }
    public enum ModeOption : uint
    {
        Tela = 0,
        MundoObjeto = 1,
    }
    /// <summary>Apresentação</summary>
    public ModeOption Mode
    {
        get => (ModeOption)Component.GetEnum("mode");
        set => Component.SetEnum("mode", (uint)value);
    }
    public enum MovementSpaceOption : uint
    {
        Mundo = 0,
        LocalDoJogador = 1,
        CameraDeEntrada = 2,
    }
    /// <summary>Espaço do movimento</summary>
    public MovementSpaceOption MovementSpace
    {
        get => (MovementSpaceOption)Component.GetEnum("movement_space");
        set => Component.SetEnum("movement_space", (uint)value);
    }
    /// <summary>Jogador / receptor. Character ou motor de corpo dinâmico ativo.</summary>
    public ObjectReference InputReceiver
    {
        get => Component.GetReference("input_receiver");
        set => Component.SetReference("input_receiver", value);
    }
    /// <summary>Camera de entrada</summary>
    public ObjectReference InputCamera
    {
        get => Component.GetReference("input_camera");
        set => Component.SetReference("input_camera", value);
    }
    /// <summary>Documento UI. Recurso do projeto por slot</summary>
    public AssetGuid GetDocument(uint slot = 0) => Component.GetResource("document", slot);
    public void SetDocument(AssetGuid value, uint slot = 0) => Component.SetResource("document", value, slot);
}

/// <summary>Malha: Geometria e material. Família Renderização · Geometria.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-MeshRenderer.html</remarks>
public readonly struct MeshRenderer : IComponentFacade<MeshRenderer>
{
    public static string TypeId => "astra.render.mesh";
    public static MeshRenderer Wrap(Component component) => new(component);
    public Component Component { get; }
    public MeshRenderer(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Cor R</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float BaseColorR
    {
        get => Component.GetFloat("base_color.r");
        set => Component.SetFloat("base_color.r", value);
    }
    /// <summary>Cor G</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float BaseColorG
    {
        get => Component.GetFloat("base_color.g");
        set => Component.SetFloat("base_color.g", value);
    }
    /// <summary>Cor B</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float BaseColorB
    {
        get => Component.GetFloat("base_color.b");
        set => Component.SetFloat("base_color.b", value);
    }
    /// <summary>Rugosidade</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Roughness
    {
        get => Component.GetFloat("roughness");
        set => Component.SetFloat("roughness", value);
    }
    /// <summary>Metálico</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Metallic
    {
        get => Component.GetFloat("metallic");
        set => Component.SetFloat("metallic", value);
    }
    /// <summary>Intensidade da normal</summary>
    /// <remarks>Faixa válida: 0 a 16.</remarks>
    public float NormalScale
    {
        get => Component.GetFloat("normal_scale");
        set => Component.SetFloat("normal_scale", value);
    }
    /// <summary>Especular</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Specular
    {
        get => Component.GetFloat("specular");
        set => Component.SetFloat("specular", value);
    }
    /// <summary>Emissão R</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float EmissionR
    {
        get => Component.GetFloat("emission.r");
        set => Component.SetFloat("emission.r", value);
    }
    /// <summary>Emissão G</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float EmissionG
    {
        get => Component.GetFloat("emission.g");
        set => Component.SetFloat("emission.g", value);
    }
    /// <summary>Emissão B</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float EmissionB
    {
        get => Component.GetFloat("emission.b");
        set => Component.SetFloat("emission.b", value);
    }
    /// <summary>Potência de emissão</summary>
    /// <remarks>Faixa válida: 0 a 10000.</remarks>
    public float EmissionStrength
    {
        get => Component.GetFloat("emission_strength");
        set => Component.SetFloat("emission_strength", value);
    }
    /// <summary>Renderizar</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Receber sombras. Sombras do sol e das luzes locais escurecem esta malha</summary>
    public bool ReceiveShadows
    {
        get => Component.GetBool("receive_shadows");
        set => Component.SetBool("receive_shadows", value);
    }
    public enum CastShadowsOption : uint
    {
        Desligado = 0,
        Ligado = 1,
        DuplaFace = 2,
        SoSombra = 3,
    }
    /// <summary>Projetar sombras. Ligado respeita a face do material; Dupla face projeta pelas duas; Só sombra some da câmera</summary>
    public CastShadowsOption CastShadows
    {
        get => (CastShadowsOption)Component.GetEnum("cast_shadows");
        set => Component.SetEnum("cast_shadows", (uint)value);
    }
    [System.Flags]
    public enum RenderingLayerMaskOption : uint
    {
        Padrao = 1,
        Camada1 = 2,
        Camada2 = 4,
        Camada3 = 8,
        Camada4 = 16,
        Camada5 = 32,
        Camada6 = 64,
        Camada7 = 128,
    }
    /// <summary>Camadas de renderização. Só luzes com uma camada de renderização em comum iluminam esta malha</summary>
    public RenderingLayerMaskOption RenderingLayerMask
    {
        get => (RenderingLayerMaskOption)Component.GetEnum("rendering_layer_mask");
        set => Component.SetEnum("rendering_layer_mask", (uint)value);
    }
    /// <summary>Malha. Recurso do projeto por slot</summary>
    public AssetGuid GetMesh(uint slot = 0) => Component.GetResource("mesh", slot);
    public void SetMesh(AssetGuid value, uint slot = 0) => Component.SetResource("mesh", value, slot);
    /// <summary>Material. Recurso do projeto por slot</summary>
    public AssetGuid GetMaterial(uint slot = 0) => Component.GetResource("material", slot);
    public void SetMaterial(AssetGuid value, uint slot = 0) => Component.SetResource("material", value, slot);
    /// <summary>Cor base. Recurso do projeto por slot</summary>
    public AssetGuid GetTextureBaseColor(uint slot = 0) => Component.GetResource("texture.base_color", slot);
    public void SetTextureBaseColor(AssetGuid value, uint slot = 0) => Component.SetResource("texture.base_color", value, slot);
    /// <summary>Normal. Recurso do projeto por slot</summary>
    public AssetGuid GetTextureNormal(uint slot = 0) => Component.GetResource("texture.normal", slot);
    public void SetTextureNormal(AssetGuid value, uint slot = 0) => Component.SetResource("texture.normal", value, slot);
    /// <summary>Metal / rugosidade. Recurso do projeto por slot</summary>
    public AssetGuid GetTextureMetallicRoughness(uint slot = 0) => Component.GetResource("texture.metallic_roughness", slot);
    public void SetTextureMetallicRoughness(AssetGuid value, uint slot = 0) => Component.SetResource("texture.metallic_roughness", value, slot);
    /// <summary>Emissão. Recurso do projeto por slot</summary>
    public AssetGuid GetTextureEmissive(uint slot = 0) => Component.GetResource("texture.emissive", slot);
    public void SetTextureEmissive(AssetGuid value, uint slot = 0) => Component.SetResource("texture.emissive", value, slot);
    /// <summary>Lightmap indireto (RGB linear / UV1). Recurso do projeto por slot</summary>
    public AssetGuid GetTextureLightmap(uint slot = 0) => Component.GetResource("texture.lightmap", slot);
    public void SetTextureLightmap(AssetGuid value, uint slot = 0) => Component.SetResource("texture.lightmap", value, slot);
    /// <summary>Oclusão. Recurso do projeto por slot</summary>
    public AssetGuid GetTextureOcclusion(uint slot = 0) => Component.GetResource("texture.occlusion", slot);
    public void SetTextureOcclusion(AssetGuid value, uint slot = 0) => Component.SetResource("texture.occlusion", value, slot);
    /// <summary>Escala U. Valor por slot</summary>
    public float GetLightmapScaleU(uint slot) => Component.GetSlotFloat("lightmap.scale_u", slot);
    public void SetLightmapScaleU(uint slot, float value) => Component.SetSlotFloat("lightmap.scale_u", value, slot);
    /// <summary>Escala V. Valor por slot</summary>
    public float GetLightmapScaleV(uint slot) => Component.GetSlotFloat("lightmap.scale_v", slot);
    public void SetLightmapScaleV(uint slot, float value) => Component.SetSlotFloat("lightmap.scale_v", value, slot);
    /// <summary>Deslocamento U. Valor por slot</summary>
    public float GetLightmapOffsetU(uint slot) => Component.GetSlotFloat("lightmap.offset_u", slot);
    public void SetLightmapOffsetU(uint slot, float value) => Component.SetSlotFloat("lightmap.offset_u", value, slot);
    /// <summary>Deslocamento V. Valor por slot</summary>
    public float GetLightmapOffsetV(uint slot) => Component.GetSlotFloat("lightmap.offset_v", slot);
    public void SetLightmapOffsetV(uint slot, float value) => Component.SetSlotFloat("lightmap.offset_v", value, slot);
    /// <summary>Intensidade. Valor por slot</summary>
    public float GetLightmapIntensity(uint slot) => Component.GetSlotFloat("lightmap.intensity", slot);
    public void SetLightmapIntensity(uint slot, float value) => Component.SetSlotFloat("lightmap.intensity", value, slot);
    /// <summary>Corte do alfa. Valor por slot</summary>
    public float GetSurfaceAlphaCutoff(uint slot) => Component.GetSlotFloat("surface.alpha_cutoff", slot);
    public void SetSurfaceAlphaCutoff(uint slot, float value) => Component.SetSlotFloat("surface.alpha_cutoff", value, slot);
    /// <summary>Força da oclusão. Valor por slot</summary>
    public float GetChannelsOcclusionStrength(uint slot) => Component.GetSlotFloat("channels.occlusion_strength", slot);
    public void SetChannelsOcclusionStrength(uint slot, float value) => Component.SetSlotFloat("channels.occlusion_strength", value, slot);
    /// <summary>Deslocamento U. Valor por slot</summary>
    public float GetSamplingOffsetU(uint slot) => Component.GetSlotFloat("sampling.offset_u", slot);
    public void SetSamplingOffsetU(uint slot, float value) => Component.SetSlotFloat("sampling.offset_u", value, slot);
    /// <summary>Deslocamento V. Valor por slot</summary>
    public float GetSamplingOffsetV(uint slot) => Component.GetSlotFloat("sampling.offset_v", slot);
    public void SetSamplingOffsetV(uint slot, float value) => Component.SetSlotFloat("sampling.offset_v", value, slot);
    /// <summary>Escala U. Valor por slot</summary>
    public float GetSamplingScaleU(uint slot) => Component.GetSlotFloat("sampling.scale_u", slot);
    public void SetSamplingScaleU(uint slot, float value) => Component.SetSlotFloat("sampling.scale_u", value, slot);
    /// <summary>Escala V. Valor por slot</summary>
    public float GetSamplingScaleV(uint slot) => Component.GetSlotFloat("sampling.scale_v", slot);
    public void SetSamplingScaleV(uint slot, float value) => Component.SetSlotFloat("sampling.scale_v", value, slot);
    /// <summary>Rotação da UV. Valor por slot</summary>
    public float GetSamplingRotation(uint slot) => Component.GetSlotFloat("sampling.rotation", slot);
    public void SetSamplingRotation(uint slot, float value) => Component.SetSlotFloat("sampling.rotation", value, slot);
    /// <summary>Cor base / Deslocamento U. Valor por slot</summary>
    public float GetSamplingBaseColorOffsetU(uint slot) => Component.GetSlotFloat("sampling.base_color.offset_u", slot);
    public void SetSamplingBaseColorOffsetU(uint slot, float value) => Component.SetSlotFloat("sampling.base_color.offset_u", value, slot);
    /// <summary>Cor base / Deslocamento V. Valor por slot</summary>
    public float GetSamplingBaseColorOffsetV(uint slot) => Component.GetSlotFloat("sampling.base_color.offset_v", slot);
    public void SetSamplingBaseColorOffsetV(uint slot, float value) => Component.SetSlotFloat("sampling.base_color.offset_v", value, slot);
    /// <summary>Cor base / Escala U. Valor por slot</summary>
    public float GetSamplingBaseColorScaleU(uint slot) => Component.GetSlotFloat("sampling.base_color.scale_u", slot);
    public void SetSamplingBaseColorScaleU(uint slot, float value) => Component.SetSlotFloat("sampling.base_color.scale_u", value, slot);
    /// <summary>Cor base / Escala V. Valor por slot</summary>
    public float GetSamplingBaseColorScaleV(uint slot) => Component.GetSlotFloat("sampling.base_color.scale_v", slot);
    public void SetSamplingBaseColorScaleV(uint slot, float value) => Component.SetSlotFloat("sampling.base_color.scale_v", value, slot);
    /// <summary>Cor base / Rotação. Valor por slot</summary>
    public float GetSamplingBaseColorRotation(uint slot) => Component.GetSlotFloat("sampling.base_color.rotation", slot);
    public void SetSamplingBaseColorRotation(uint slot, float value) => Component.SetSlotFloat("sampling.base_color.rotation", value, slot);
    /// <summary>Normal / Deslocamento U. Valor por slot</summary>
    public float GetSamplingNormalOffsetU(uint slot) => Component.GetSlotFloat("sampling.normal.offset_u", slot);
    public void SetSamplingNormalOffsetU(uint slot, float value) => Component.SetSlotFloat("sampling.normal.offset_u", value, slot);
    /// <summary>Normal / Deslocamento V. Valor por slot</summary>
    public float GetSamplingNormalOffsetV(uint slot) => Component.GetSlotFloat("sampling.normal.offset_v", slot);
    public void SetSamplingNormalOffsetV(uint slot, float value) => Component.SetSlotFloat("sampling.normal.offset_v", value, slot);
    /// <summary>Normal / Escala U. Valor por slot</summary>
    public float GetSamplingNormalScaleU(uint slot) => Component.GetSlotFloat("sampling.normal.scale_u", slot);
    public void SetSamplingNormalScaleU(uint slot, float value) => Component.SetSlotFloat("sampling.normal.scale_u", value, slot);
    /// <summary>Normal / Escala V. Valor por slot</summary>
    public float GetSamplingNormalScaleV(uint slot) => Component.GetSlotFloat("sampling.normal.scale_v", slot);
    public void SetSamplingNormalScaleV(uint slot, float value) => Component.SetSlotFloat("sampling.normal.scale_v", value, slot);
    /// <summary>Normal / Rotação. Valor por slot</summary>
    public float GetSamplingNormalRotation(uint slot) => Component.GetSlotFloat("sampling.normal.rotation", slot);
    public void SetSamplingNormalRotation(uint slot, float value) => Component.SetSlotFloat("sampling.normal.rotation", value, slot);
    /// <summary>Metal / rugosidade / Deslocamento U. Valor por slot</summary>
    public float GetSamplingMetallicRoughnessOffsetU(uint slot) => Component.GetSlotFloat("sampling.metallic_roughness.offset_u", slot);
    public void SetSamplingMetallicRoughnessOffsetU(uint slot, float value) => Component.SetSlotFloat("sampling.metallic_roughness.offset_u", value, slot);
    /// <summary>Metal / rugosidade / Deslocamento V. Valor por slot</summary>
    public float GetSamplingMetallicRoughnessOffsetV(uint slot) => Component.GetSlotFloat("sampling.metallic_roughness.offset_v", slot);
    public void SetSamplingMetallicRoughnessOffsetV(uint slot, float value) => Component.SetSlotFloat("sampling.metallic_roughness.offset_v", value, slot);
    /// <summary>Metal / rugosidade / Escala U. Valor por slot</summary>
    public float GetSamplingMetallicRoughnessScaleU(uint slot) => Component.GetSlotFloat("sampling.metallic_roughness.scale_u", slot);
    public void SetSamplingMetallicRoughnessScaleU(uint slot, float value) => Component.SetSlotFloat("sampling.metallic_roughness.scale_u", value, slot);
    /// <summary>Metal / rugosidade / Escala V. Valor por slot</summary>
    public float GetSamplingMetallicRoughnessScaleV(uint slot) => Component.GetSlotFloat("sampling.metallic_roughness.scale_v", slot);
    public void SetSamplingMetallicRoughnessScaleV(uint slot, float value) => Component.SetSlotFloat("sampling.metallic_roughness.scale_v", value, slot);
    /// <summary>Metal / rugosidade / Rotação. Valor por slot</summary>
    public float GetSamplingMetallicRoughnessRotation(uint slot) => Component.GetSlotFloat("sampling.metallic_roughness.rotation", slot);
    public void SetSamplingMetallicRoughnessRotation(uint slot, float value) => Component.SetSlotFloat("sampling.metallic_roughness.rotation", value, slot);
    /// <summary>Emissão / Deslocamento U. Valor por slot</summary>
    public float GetSamplingEmissiveOffsetU(uint slot) => Component.GetSlotFloat("sampling.emissive.offset_u", slot);
    public void SetSamplingEmissiveOffsetU(uint slot, float value) => Component.SetSlotFloat("sampling.emissive.offset_u", value, slot);
    /// <summary>Emissão / Deslocamento V. Valor por slot</summary>
    public float GetSamplingEmissiveOffsetV(uint slot) => Component.GetSlotFloat("sampling.emissive.offset_v", slot);
    public void SetSamplingEmissiveOffsetV(uint slot, float value) => Component.SetSlotFloat("sampling.emissive.offset_v", value, slot);
    /// <summary>Emissão / Escala U. Valor por slot</summary>
    public float GetSamplingEmissiveScaleU(uint slot) => Component.GetSlotFloat("sampling.emissive.scale_u", slot);
    public void SetSamplingEmissiveScaleU(uint slot, float value) => Component.SetSlotFloat("sampling.emissive.scale_u", value, slot);
    /// <summary>Emissão / Escala V. Valor por slot</summary>
    public float GetSamplingEmissiveScaleV(uint slot) => Component.GetSlotFloat("sampling.emissive.scale_v", slot);
    public void SetSamplingEmissiveScaleV(uint slot, float value) => Component.SetSlotFloat("sampling.emissive.scale_v", value, slot);
    /// <summary>Emissão / Rotação. Valor por slot</summary>
    public float GetSamplingEmissiveRotation(uint slot) => Component.GetSlotFloat("sampling.emissive.rotation", slot);
    public void SetSamplingEmissiveRotation(uint slot, float value) => Component.SetSlotFloat("sampling.emissive.rotation", value, slot);
    /// <summary>Cor R. Valor por slot</summary>
    public float GetMaterialBaseColorR(uint slot) => Component.GetSlotFloat("material.base_color.r", slot);
    public void SetMaterialBaseColorR(uint slot, float value) => Component.SetSlotFloat("material.base_color.r", value, slot);
    /// <summary>Cor G. Valor por slot</summary>
    public float GetMaterialBaseColorG(uint slot) => Component.GetSlotFloat("material.base_color.g", slot);
    public void SetMaterialBaseColorG(uint slot, float value) => Component.SetSlotFloat("material.base_color.g", value, slot);
    /// <summary>Cor B. Valor por slot</summary>
    public float GetMaterialBaseColorB(uint slot) => Component.GetSlotFloat("material.base_color.b", slot);
    public void SetMaterialBaseColorB(uint slot, float value) => Component.SetSlotFloat("material.base_color.b", value, slot);
    /// <summary>Rugosidade. Valor por slot</summary>
    public float GetMaterialRoughness(uint slot) => Component.GetSlotFloat("material.roughness", slot);
    public void SetMaterialRoughness(uint slot, float value) => Component.SetSlotFloat("material.roughness", value, slot);
    /// <summary>Metálico. Valor por slot</summary>
    public float GetMaterialMetallic(uint slot) => Component.GetSlotFloat("material.metallic", slot);
    public void SetMaterialMetallic(uint slot, float value) => Component.SetSlotFloat("material.metallic", value, slot);
    /// <summary>Intensidade da normal. Valor por slot</summary>
    public float GetMaterialNormalScale(uint slot) => Component.GetSlotFloat("material.normal_scale", slot);
    public void SetMaterialNormalScale(uint slot, float value) => Component.SetSlotFloat("material.normal_scale", value, slot);
    /// <summary>Especular. Valor por slot</summary>
    public float GetMaterialSpecular(uint slot) => Component.GetSlotFloat("material.specular", slot);
    public void SetMaterialSpecular(uint slot, float value) => Component.SetSlotFloat("material.specular", value, slot);
    /// <summary>Emissão R. Valor por slot</summary>
    public float GetMaterialEmissionR(uint slot) => Component.GetSlotFloat("material.emission.r", slot);
    public void SetMaterialEmissionR(uint slot, float value) => Component.SetSlotFloat("material.emission.r", value, slot);
    /// <summary>Emissão G. Valor por slot</summary>
    public float GetMaterialEmissionG(uint slot) => Component.GetSlotFloat("material.emission.g", slot);
    public void SetMaterialEmissionG(uint slot, float value) => Component.SetSlotFloat("material.emission.g", value, slot);
    /// <summary>Emissão B. Valor por slot</summary>
    public float GetMaterialEmissionB(uint slot) => Component.GetSlotFloat("material.emission.b", slot);
    public void SetMaterialEmissionB(uint slot, float value) => Component.SetSlotFloat("material.emission.b", value, slot);
    /// <summary>Potência de emissão. Valor por slot</summary>
    public float GetMaterialEmissionStrength(uint slot) => Component.GetSlotFloat("material.emission_strength", slot);
    public void SetMaterialEmissionStrength(uint slot, float value) => Component.SetSlotFloat("material.emission_strength", value, slot);
    /// <summary>Material. Opção por slot</summary>
    public uint GetMaterialOverride(uint slot) => Component.GetSlotEnum("material.override", slot);
    public void SetMaterialOverride(uint slot, uint value) => Component.SetSlotEnum("material.override", value, slot);
    /// <summary>Receber lightmap indireto. Opção por slot</summary>
    public uint GetLightmapEnabled(uint slot) => Component.GetSlotEnum("lightmap.enabled", slot);
    public void SetLightmapEnabled(uint slot, uint value) => Component.SetSlotEnum("lightmap.enabled", value, slot);
    /// <summary>Tipo de superfície. Opção por slot</summary>
    public uint GetSurfaceAlphaMode(uint slot) => Component.GetSlotEnum("surface.alpha_mode", slot);
    public void SetSurfaceAlphaMode(uint slot, uint value) => Component.SetSlotEnum("surface.alpha_mode", value, slot);
    /// <summary>Faces. Opção por slot</summary>
    public uint GetSurfaceSides(uint slot) => Component.GetSlotEnum("surface.sides", slot);
    public void SetSurfaceSides(uint slot, uint value) => Component.SetSlotEnum("surface.sides", value, slot);
    /// <summary>Canal da rugosidade. Opção por slot</summary>
    public uint GetChannelsRoughness(uint slot) => Component.GetSlotEnum("channels.roughness", slot);
    public void SetChannelsRoughness(uint slot, uint value) => Component.SetSlotEnum("channels.roughness", value, slot);
    /// <summary>Canal do metálico. Opção por slot</summary>
    public uint GetChannelsMetallic(uint slot) => Component.GetSlotEnum("channels.metallic", slot);
    public void SetChannelsMetallic(uint slot, uint value) => Component.SetSlotEnum("channels.metallic", value, slot);
    /// <summary>Canal da oclusão. Opção por slot</summary>
    public uint GetChannelsOcclusion(uint slot) => Component.GetSlotEnum("channels.occlusion", slot);
    public void SetChannelsOcclusion(uint slot, uint value) => Component.SetSlotEnum("channels.occlusion", value, slot);
    /// <summary>Origem da oclusão. Opção por slot</summary>
    public uint GetChannelsOcclusionSource(uint slot) => Component.GetSlotEnum("channels.occlusion_source", slot);
    public void SetChannelsOcclusionSource(uint slot, uint value) => Component.SetSlotEnum("channels.occlusion_source", value, slot);
    /// <summary>Inverter Y da normal. Opção por slot</summary>
    public uint GetChannelsNormalFlipY(uint slot) => Component.GetSlotEnum("channels.normal_flip_y", slot);
    public void SetChannelsNormalFlipY(uint slot, uint value) => Component.SetSlotEnum("channels.normal_flip_y", value, slot);
    /// <summary>Origem do alfa. Opção por slot</summary>
    public uint GetChannelsAlphaSource(uint slot) => Component.GetSlotEnum("channels.alpha_source", slot);
    public void SetChannelsAlphaSource(uint slot, uint value) => Component.SetSlotEnum("channels.alpha_source", value, slot);
    /// <summary>Origem da rugosidade. Opção por slot</summary>
    public uint GetChannelsRoughnessSource(uint slot) => Component.GetSlotEnum("channels.roughness_source", slot);
    public void SetChannelsRoughnessSource(uint slot, uint value) => Component.SetSlotEnum("channels.roughness_source", value, slot);
    /// <summary>Conjunto de UV. Opção por slot</summary>
    public uint GetSamplingUvSet(uint slot) => Component.GetSlotEnum("sampling.uv_set", slot);
    public void SetSamplingUvSet(uint slot, uint value) => Component.SetSlotEnum("sampling.uv_set", value, slot);
    /// <summary>Repetição. Opção por slot</summary>
    public uint GetSamplingWrap(uint slot) => Component.GetSlotEnum("sampling.wrap", slot);
    public void SetSamplingWrap(uint slot, uint value) => Component.SetSlotEnum("sampling.wrap", value, slot);
    /// <summary>Filtro. Opção por slot</summary>
    public uint GetSamplingFilter(uint slot) => Component.GetSlotEnum("sampling.filter", slot);
    public void SetSamplingFilter(uint slot, uint value) => Component.SetSlotEnum("sampling.filter", value, slot);
    /// <summary>Cor base / UV. Opção por slot</summary>
    public uint GetSamplingBaseColorUvSet(uint slot) => Component.GetSlotEnum("sampling.base_color.uv_set", slot);
    public void SetSamplingBaseColorUvSet(uint slot, uint value) => Component.SetSlotEnum("sampling.base_color.uv_set", value, slot);
    /// <summary>Cor base / Repetição. Opção por slot</summary>
    public uint GetSamplingBaseColorWrap(uint slot) => Component.GetSlotEnum("sampling.base_color.wrap", slot);
    public void SetSamplingBaseColorWrap(uint slot, uint value) => Component.SetSlotEnum("sampling.base_color.wrap", value, slot);
    /// <summary>Cor base / Filtro. Opção por slot</summary>
    public uint GetSamplingBaseColorFilter(uint slot) => Component.GetSlotEnum("sampling.base_color.filter", slot);
    public void SetSamplingBaseColorFilter(uint slot, uint value) => Component.SetSlotEnum("sampling.base_color.filter", value, slot);
    /// <summary>Normal / UV. Opção por slot</summary>
    public uint GetSamplingNormalUvSet(uint slot) => Component.GetSlotEnum("sampling.normal.uv_set", slot);
    public void SetSamplingNormalUvSet(uint slot, uint value) => Component.SetSlotEnum("sampling.normal.uv_set", value, slot);
    /// <summary>Normal / Repetição. Opção por slot</summary>
    public uint GetSamplingNormalWrap(uint slot) => Component.GetSlotEnum("sampling.normal.wrap", slot);
    public void SetSamplingNormalWrap(uint slot, uint value) => Component.SetSlotEnum("sampling.normal.wrap", value, slot);
    /// <summary>Normal / Filtro. Opção por slot</summary>
    public uint GetSamplingNormalFilter(uint slot) => Component.GetSlotEnum("sampling.normal.filter", slot);
    public void SetSamplingNormalFilter(uint slot, uint value) => Component.SetSlotEnum("sampling.normal.filter", value, slot);
    /// <summary>Metal / rugosidade / UV. Opção por slot</summary>
    public uint GetSamplingMetallicRoughnessUvSet(uint slot) => Component.GetSlotEnum("sampling.metallic_roughness.uv_set", slot);
    public void SetSamplingMetallicRoughnessUvSet(uint slot, uint value) => Component.SetSlotEnum("sampling.metallic_roughness.uv_set", value, slot);
    /// <summary>Metal / rugosidade / Repetição. Opção por slot</summary>
    public uint GetSamplingMetallicRoughnessWrap(uint slot) => Component.GetSlotEnum("sampling.metallic_roughness.wrap", slot);
    public void SetSamplingMetallicRoughnessWrap(uint slot, uint value) => Component.SetSlotEnum("sampling.metallic_roughness.wrap", value, slot);
    /// <summary>Metal / rugosidade / Filtro. Opção por slot</summary>
    public uint GetSamplingMetallicRoughnessFilter(uint slot) => Component.GetSlotEnum("sampling.metallic_roughness.filter", slot);
    public void SetSamplingMetallicRoughnessFilter(uint slot, uint value) => Component.SetSlotEnum("sampling.metallic_roughness.filter", value, slot);
    /// <summary>Emissão / UV. Opção por slot</summary>
    public uint GetSamplingEmissiveUvSet(uint slot) => Component.GetSlotEnum("sampling.emissive.uv_set", slot);
    public void SetSamplingEmissiveUvSet(uint slot, uint value) => Component.SetSlotEnum("sampling.emissive.uv_set", value, slot);
    /// <summary>Emissão / Repetição. Opção por slot</summary>
    public uint GetSamplingEmissiveWrap(uint slot) => Component.GetSlotEnum("sampling.emissive.wrap", slot);
    public void SetSamplingEmissiveWrap(uint slot, uint value) => Component.SetSlotEnum("sampling.emissive.wrap", value, slot);
    /// <summary>Emissão / Filtro. Opção por slot</summary>
    public uint GetSamplingEmissiveFilter(uint slot) => Component.GetSlotEnum("sampling.emissive.filter", slot);
    public void SetSamplingEmissiveFilter(uint slot, uint value) => Component.SetSlotEnum("sampling.emissive.filter", value, slot);
}

/// <summary>Malha deformável: Esqueleto e blend shapes da Malha. Família Renderização · Geometria.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-SkinnedMeshRenderer.html</remarks>
public readonly struct SkinnedMesh : IComponentFacade<SkinnedMesh>
{
    public static string TypeId => "astra.render.skinned_mesh";
    public static SkinnedMesh Wrap(Component component) => new(component);
    public Component Component { get; }
    public SkinnedMesh(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Vetor de movimento da deformação. A reprojeção temporal usa a pose anterior dos ossos e dos blend shapes, não só a do objeto</summary>
    public bool SkinnedMotionVectors
    {
        get => Component.GetBool("skinned_motion_vectors");
        set => Component.SetBool("skinned_motion_vectors", value);
    }
    public enum QualityOption : uint
    {
        Automatica = 0,
        V1Osso = 1,
        V2Ossos = 2,
        V4Ossos = 4,
    }
    /// <summary>Qualidade. Influências por vértice usadas na deformação (Skin Weights)</summary>
    public QualityOption Quality
    {
        get => (QualityOption)Component.GetEnum("quality");
        set => Component.SetEnum("quality", (uint)value);
    }
    /// <summary>Peso do blend shape (%). Valor por slot</summary>
    public float GetBlendShapeWeight(uint slot) => Component.GetSlotFloat("blend_shape_weight", slot);
    public void SetBlendShapeWeight(uint slot, float value) => Component.SetSlotFloat("blend_shape_weight", value, slot);
}

/// <summary>LOD Group: Nível de detalhe pela altura na tela. Família Renderização · Desempenho.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-LODGroup.html</remarks>
public readonly struct LodGroup : IComponentFacade<LodGroup>
{
    public static string TypeId => "astra.render.lod_group";
    public static LodGroup Wrap(Component component) => new(component);
    public Component Component { get; }
    public LodGroup(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Transição LOD 0 (%). Altura na tela abaixo da qual este nível deixa de ser usado</summary>
    /// <remarks>Faixa válida: 0.1 a 100.</remarks>
    public float Transition0
    {
        get => Component.GetFloat("transition_0");
        set => Component.SetFloat("transition_0", value);
    }
    /// <summary>Transição LOD 1 (%). Altura na tela abaixo da qual este nível deixa de ser usado</summary>
    /// <remarks>Faixa válida: 0.1 a 100.</remarks>
    public float Transition1
    {
        get => Component.GetFloat("transition_1");
        set => Component.SetFloat("transition_1", value);
    }
    /// <summary>Transição LOD 2 (%). Altura na tela abaixo da qual este nível deixa de ser usado</summary>
    /// <remarks>Faixa válida: 0.1 a 100.</remarks>
    public float Transition2
    {
        get => Component.GetFloat("transition_2");
        set => Component.SetFloat("transition_2", value);
    }
    /// <summary>Transição LOD 3 (%). Altura na tela abaixo da qual este nível deixa de ser usado</summary>
    /// <remarks>Faixa válida: 0.1 a 100.</remarks>
    public float Transition3
    {
        get => Component.GetFloat("transition_3");
        set => Component.SetFloat("transition_3", value);
    }
    /// <summary>Tamanho. Maior extensão do nível mais detalhado; Recalcular mede a malha</summary>
    /// <remarks>Faixa válida: 0.001 a 1000000.</remarks>
    public float Size
    {
        get => Component.GetFloat("size");
        set => Component.SetFloat("size", value);
    }
    /// <summary>Largura do fade LOD 0. Proporção do nível em que ele cruza com o próximo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FadeWidth0
    {
        get => Component.GetFloat("fade_width_0");
        set => Component.SetFloat("fade_width_0", value);
    }
    /// <summary>Largura do fade LOD 1. Proporção do nível em que ele cruza com o próximo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FadeWidth1
    {
        get => Component.GetFloat("fade_width_1");
        set => Component.SetFloat("fade_width_1", value);
    }
    /// <summary>Largura do fade LOD 2. Proporção do nível em que ele cruza com o próximo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FadeWidth2
    {
        get => Component.GetFloat("fade_width_2");
        set => Component.SetFloat("fade_width_2", value);
    }
    /// <summary>Largura do fade LOD 3. Proporção do nível em que ele cruza com o próximo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FadeWidth3
    {
        get => Component.GetFloat("fade_width_3");
        set => Component.SetFloat("fade_width_3", value);
    }
    /// <summary>Ativo. Desligar libera os renderizadores do controle deste grupo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Animate Cross-fading. Troca por tempo em vez da faixa de largura</summary>
    public bool AnimateCrossFading
    {
        get => Component.GetBool("animate_cross_fading");
        set => Component.SetBool("animate_cross_fading", value);
    }
    public enum LevelCountOption : uint
    {
        V1 = 1,
        V2 = 2,
        V3 = 3,
        V4 = 4,
    }
    /// <summary>Níveis</summary>
    public LevelCountOption LevelCount
    {
        get => (LevelCountOption)Component.GetEnum("level_count");
        set => Component.SetEnum("level_count", (uint)value);
    }
    public enum FadeModeOption : uint
    {
        Nenhum = 0,
        CrossFade = 1,
    }
    /// <summary>Fade Mode</summary>
    public FadeModeOption FadeMode
    {
        get => (FadeModeOption)Component.GetEnum("fade_mode");
        set => Component.SetEnum("fade_mode", (uint)value);
    }
    public enum ForceLevelOption : uint
    {
        Automatico = 0,
        LOD0 = 1,
        LOD1 = 2,
        LOD2 = 3,
        LOD3 = 4,
    }
    /// <summary>Forçar nível. Estado de execução: 0 automático, n força o LOD n-1</summary>
    public ForceLevelOption ForceLevel
    {
        get => (ForceLevelOption)Component.GetEnum("force_level");
        set => Component.SetEnum("force_level", (uint)value);
    }
    /// <summary>Objetos LOD 0</summary>
    public ObjectReference Level0
    {
        get => Component.GetReference("level_0");
        set => Component.SetReference("level_0", value);
    }
    /// <summary>Objetos LOD 1</summary>
    public ObjectReference Level1
    {
        get => Component.GetReference("level_1");
        set => Component.SetReference("level_1", value);
    }
    /// <summary>Objetos LOD 2</summary>
    public ObjectReference Level2
    {
        get => Component.GetReference("level_2");
        set => Component.SetReference("level_2", value);
    }
    /// <summary>Objetos LOD 3</summary>
    public ObjectReference Level3
    {
        get => Component.GetReference("level_3");
        set => Component.SetReference("level_3", value);
    }
}

/// <summary>Ambiente: Céu, atmosfera, neblina e pós globais ou por volume. Família Renderização · Ambiente.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.render-pipelines.universal@17.0/manual/Volumes.html</remarks>
public readonly struct EnvironmentVolume : IComponentFacade<EnvironmentVolume>
{
    public static string TypeId => "astra.render.environment";
    public static EnvironmentVolume Wrap(Component component) => new(component);
    public Component Component { get; }
    public EnvironmentVolume(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Tom do céu. Cor linear RGB</summary>
    public Vector3 SkyZenith
    {
        get => new(Component.GetFloat("sky_zenith.r"), Component.GetFloat("sky_zenith.g"), Component.GetFloat("sky_zenith.b"));
        set => Component.SetVector3("sky_zenith", value);
    }
    /// <summary>Cor do horizonte. Cor linear RGB</summary>
    public Vector3 SkyHorizon
    {
        get => new(Component.GetFloat("sky_horizon.r"), Component.GetFloat("sky_horizon.g"), Component.GetFloat("sky_horizon.b"));
        set => Component.SetVector3("sky_horizon", value);
    }
    /// <summary>Cor do chão. Cor linear RGB</summary>
    public Vector3 Ground
    {
        get => new(Component.GetFloat("ground.r"), Component.GetFloat("ground.g"), Component.GetFloat("ground.b"));
        set => Component.SetVector3("ground", value);
    }
    /// <summary>Cor da neblina. Cor linear RGB</summary>
    public Vector3 FogColor
    {
        get => new(Component.GetFloat("fog_color.r"), Component.GetFloat("fog_color.g"), Component.GetFloat("fog_color.b"));
        set => Component.SetVector3("fog_color", value);
    }
    /// <summary>Tamanho da caixa</summary>
    public Vector3 BoxSize
    {
        get => new(Component.GetFloat("box_size.x"), Component.GetFloat("box_size.y"), Component.GetFloat("box_size.z"));
        set => Component.SetVector3("box_size", value);
    }
    /// <summary>Lift. Cor linear RGB</summary>
    public Vector3 GradingLiftColor
    {
        get => new(Component.GetFloat("grading_lift_color.r"), Component.GetFloat("grading_lift_color.g"), Component.GetFloat("grading_lift_color.b"));
        set => Component.SetVector3("grading_lift_color", value);
    }
    /// <summary>Gama. Cor linear RGB</summary>
    public Vector3 GradingGammaColor
    {
        get => new(Component.GetFloat("grading_gamma_color.r"), Component.GetFloat("grading_gamma_color.g"), Component.GetFloat("grading_gamma_color.b"));
        set => Component.SetVector3("grading_gamma_color", value);
    }
    /// <summary>Ganho. Cor linear RGB</summary>
    public Vector3 GradingGainColor
    {
        get => new(Component.GetFloat("grading_gain_color.r"), Component.GetFloat("grading_gain_color.g"), Component.GetFloat("grading_gain_color.b"));
        set => Component.SetVector3("grading_gain_color", value);
    }
    /// <summary>Sombras. Cor linear RGB</summary>
    public Vector3 GradingShadowsColor
    {
        get => new(Component.GetFloat("grading_shadows_color.r"), Component.GetFloat("grading_shadows_color.g"), Component.GetFloat("grading_shadows_color.b"));
        set => Component.SetVector3("grading_shadows_color", value);
    }
    /// <summary>Médios. Cor linear RGB</summary>
    public Vector3 GradingMidtonesColor
    {
        get => new(Component.GetFloat("grading_midtones_color.r"), Component.GetFloat("grading_midtones_color.g"), Component.GetFloat("grading_midtones_color.b"));
        set => Component.SetVector3("grading_midtones_color", value);
    }
    /// <summary>Realces. Cor linear RGB</summary>
    public Vector3 GradingHighlightsColor
    {
        get => new(Component.GetFloat("grading_highlights_color.r"), Component.GetFloat("grading_highlights_color.g"), Component.GetFloat("grading_highlights_color.b"));
        set => Component.SetVector3("grading_highlights_color", value);
    }
    /// <summary>Tom. Cor linear RGB</summary>
    public Vector3 CloudTint
    {
        get => new(Component.GetFloat("cloud_tint.r"), Component.GetFloat("cloud_tint.g"), Component.GetFloat("cloud_tint.b"));
        set => Component.SetVector3("cloud_tint", value);
    }
    /// <summary>Prioridade</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float Priority
    {
        get => Component.GetFloat("priority");
        set => Component.SetFloat("priority", value);
    }
    /// <summary>Força atmosférica</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Atmosphere
    {
        get => Component.GetFloat("atmosphere");
        set => Component.SetFloat("atmosphere", value);
    }
    /// <summary>Diâmetro do sol (°)</summary>
    /// <remarks>Faixa válida: 0.05 a 10.</remarks>
    public float SunDiskDegrees
    {
        get => Component.GetFloat("sun_disk_degrees");
        set => Component.SetFloat("sun_disk_degrees", value);
    }
    /// <summary>Brilho do disco solar</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float SunDiskIntensity
    {
        get => Component.GetFloat("sun_disk_intensity");
        set => Component.SetFloat("sun_disk_intensity", value);
    }
    /// <summary>Energia da neblina (×)</summary>
    /// <remarks>Faixa válida: 0 a 65504.</remarks>
    public float FogLightEnergy
    {
        get => Component.GetFloat("fog_light_energy");
        set => Component.SetFloat("fog_light_energy", value);
    }
    /// <summary>Densidade (1/m)</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FogDensity
    {
        get => Component.GetFloat("fog_density");
        set => Component.SetFloat("fog_density", value);
    }
    /// <summary>Início (m)</summary>
    /// <remarks>Faixa válida: 0 a 10000.</remarks>
    public float FogStart
    {
        get => Component.GetFloat("fog_start");
        set => Component.SetFloat("fog_start", value);
    }
    /// <summary>Altura base (m)</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float FogBaseHeight
    {
        get => Component.GetFloat("fog_base_height");
        set => Component.SetFloat("fog_base_height", value);
    }
    /// <summary>Decaimento por altura (1/m)</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float FogHeightFalloff
    {
        get => Component.GetFloat("fog_height_falloff");
        set => Component.SetFloat("fog_height_falloff", value);
    }
    /// <summary>Compensação (EV)</summary>
    /// <remarks>Faixa válida: -16 a 16.</remarks>
    public float ExposureEv
    {
        get => Component.GetFloat("exposure_ev");
        set => Component.SetFloat("exposure_ev", value);
    }
    /// <summary>Limiar do bloom</summary>
    /// <remarks>Faixa válida: 0 a 64.</remarks>
    public float BloomThreshold
    {
        get => Component.GetFloat("bloom_threshold");
        set => Component.SetFloat("bloom_threshold", value);
    }
    /// <summary>Intensidade do bloom</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float BloomIntensity
    {
        get => Component.GetFloat("bloom_intensity");
        set => Component.SetFloat("bloom_intensity", value);
    }
    /// <summary>Contraste</summary>
    /// <remarks>Faixa válida: 0.5 a 2.</remarks>
    public float Contrast
    {
        get => Component.GetFloat("contrast");
        set => Component.SetFloat("contrast", value);
    }
    /// <summary>Saturação</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float Saturation
    {
        get => Component.GetFloat("saturation");
        set => Component.SetFloat("saturation", value);
    }
    /// <summary>Intensidade da vinheta</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float VignetteIntensity
    {
        get => Component.GetFloat("vignette_intensity");
        set => Component.SetFloat("vignette_intensity", value);
    }
    /// <summary>Intensidade do grão</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float FilmGrainIntensity
    {
        get => Component.GetFloat("film_grain_intensity");
        set => Component.SetFloat("film_grain_intensity", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.05 a 10.</remarks>
    public float AmbientOcclusionRadius
    {
        get => Component.GetFloat("ambient_occlusion_radius");
        set => Component.SetFloat("ambient_occlusion_radius", value);
    }
    /// <summary>Intensidade</summary>
    /// <remarks>Faixa válida: 0 a 4.</remarks>
    public float AmbientOcclusionIntensity
    {
        get => Component.GetFloat("ambient_occlusion_intensity");
        set => Component.SetFloat("ambient_occlusion_intensity", value);
    }
    /// <summary>Potência</summary>
    /// <remarks>Faixa válida: 0.1 a 4.</remarks>
    public float AmbientOcclusionPower
    {
        get => Component.GetFloat("ambient_occlusion_power");
        set => Component.SetFloat("ambient_occlusion_power", value);
    }
    /// <summary>Viés (m)</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float AmbientOcclusionBias
    {
        get => Component.GetFloat("ambient_occlusion_bias");
        set => Component.SetFloat("ambient_occlusion_bias", value);
    }
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Distância de mistura (m)</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float BlendDistance
    {
        get => Component.GetFloat("blend_distance");
        set => Component.SetFloat("blend_distance", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 100000.</remarks>
    public float SphereRadius
    {
        get => Component.GetFloat("sphere_radius");
        set => Component.SetFloat("sphere_radius", value);
    }
    /// <summary>Difuso indireto (×)</summary>
    /// <remarks>Faixa válida: 0 a 4.</remarks>
    public float IndirectDiffuse
    {
        get => Component.GetFloat("indirect_diffuse");
        set => Component.SetFloat("indirect_diffuse", value);
    }
    /// <summary>Reflexo indireto (×)</summary>
    /// <remarks>Faixa válida: 0 a 4.</remarks>
    public float IndirectSpecular
    {
        get => Component.GetFloat("indirect_specular");
        set => Component.SetFloat("indirect_specular", value);
    }
    /// <summary>Intensidade (×)</summary>
    /// <remarks>Faixa válida: 0 a 16.</remarks>
    public float PhysicalSkyIntensity
    {
        get => Component.GetFloat("physical_sky_intensity");
        set => Component.SetFloat("physical_sky_intensity", value);
    }
    /// <summary>Densidade do ar (×)</summary>
    /// <remarks>Faixa válida: 0 a 8.</remarks>
    public float AirDensity
    {
        get => Component.GetFloat("air_density");
        set => Component.SetFloat("air_density", value);
    }
    /// <summary>Densidade de aerossóis (×)</summary>
    /// <remarks>Faixa válida: 0 a 8.</remarks>
    public float AerosolDensity
    {
        get => Component.GetFloat("aerosol_density");
        set => Component.SetFloat("aerosol_density", value);
    }
    /// <summary>Anisotropia dos aerossóis (g)</summary>
    /// <remarks>Faixa válida: 0 a 0.95.</remarks>
    public float AerosolAnisotropy
    {
        get => Component.GetFloat("aerosol_anisotropy");
        set => Component.SetFloat("aerosol_anisotropy", value);
    }
    /// <summary>Raio do planeta (km)</summary>
    /// <remarks>Faixa válida: 1 a 100000.</remarks>
    public float PlanetRadiusKm
    {
        get => Component.GetFloat("planet_radius_km");
        set => Component.SetFloat("planet_radius_km", value);
    }
    /// <summary>Altura do observador (km)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ObserverHeightKm
    {
        get => Component.GetFloat("observer_height_km");
        set => Component.SetFloat("observer_height_km", value);
    }
    /// <summary>Escala Rayleigh (km)</summary>
    /// <remarks>Faixa válida: 0.1 a 100.</remarks>
    public float RayleighScaleHeightKm
    {
        get => Component.GetFloat("rayleigh_scale_height_km");
        set => Component.SetFloat("rayleigh_scale_height_km", value);
    }
    /// <summary>Escala de aerossóis (km)</summary>
    /// <remarks>Faixa válida: 0.05 a 50.</remarks>
    public float AerosolScaleHeightKm
    {
        get => Component.GetFloat("aerosol_scale_height_km");
        set => Component.SetFloat("aerosol_scale_height_km", value);
    }
    /// <summary>Altura da atmosfera (km)</summary>
    /// <remarks>Faixa válida: 1 a 1000.</remarks>
    public float AtmosphereHeightKm
    {
        get => Component.GetFloat("atmosphere_height_km");
        set => Component.SetFloat("atmosphere_height_km", value);
    }
    /// <summary>Albedo médio do solo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float GroundAlbedo
    {
        get => Component.GetFloat("ground_albedo");
        set => Component.SetFloat("ground_albedo", value);
    }
    /// <summary>EV mínimo (EV)</summary>
    /// <remarks>Faixa válida: -16 a 16.</remarks>
    public float AutoExposureMinEv
    {
        get => Component.GetFloat("auto_exposure_min_ev");
        set => Component.SetFloat("auto_exposure_min_ev", value);
    }
    /// <summary>EV máximo (EV)</summary>
    /// <remarks>Faixa válida: -16 a 16.</remarks>
    public float AutoExposureMaxEv
    {
        get => Component.GetFloat("auto_exposure_max_ev");
        set => Component.SetFloat("auto_exposure_max_ev", value);
    }
    /// <summary>Corte baixo</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float AutoExposureLowPercent
    {
        get => Component.GetFloat("auto_exposure_low_percent");
        set => Component.SetFloat("auto_exposure_low_percent", value);
    }
    /// <summary>Corte alto</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float AutoExposureHighPercent
    {
        get => Component.GetFloat("auto_exposure_high_percent");
        set => Component.SetFloat("auto_exposure_high_percent", value);
    }
    /// <summary>Cinza alvo</summary>
    /// <remarks>Faixa válida: 0.01 a 1.</remarks>
    public float AutoExposureTargetGrey
    {
        get => Component.GetFloat("auto_exposure_target_grey");
        set => Component.SetFloat("auto_exposure_target_grey", value);
    }
    /// <summary>Velocidade ao escurecer (EV/s)</summary>
    /// <remarks>Faixa válida: 0.01 a 20.</remarks>
    public float AutoExposureSpeedUp
    {
        get => Component.GetFloat("auto_exposure_speed_up");
        set => Component.SetFloat("auto_exposure_speed_up", value);
    }
    /// <summary>Velocidade ao clarear (EV/s)</summary>
    /// <remarks>Faixa válida: 0.01 a 20.</remarks>
    public float AutoExposureSpeedDown
    {
        get => Component.GetFloat("auto_exposure_speed_down");
        set => Component.SetFloat("auto_exposure_speed_down", value);
    }
    /// <summary>Rotação HDRI (°)</summary>
    /// <remarks>Faixa válida: -360 a 360.</remarks>
    public float HdriRotationDegrees
    {
        get => Component.GetFloat("hdri_rotation_degrees");
        set => Component.SetFloat("hdri_rotation_degrees", value);
    }
    /// <summary>Exposição HDRI (EV)</summary>
    /// <remarks>Faixa válida: -16 a 16.</remarks>
    public float HdriExposureEv
    {
        get => Component.GetFloat("hdri_exposure_ev");
        set => Component.SetFloat("hdri_exposure_ev", value);
    }
    /// <summary>Temperatura</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float GradingTemperature
    {
        get => Component.GetFloat("grading_temperature");
        set => Component.SetFloat("grading_temperature", value);
    }
    /// <summary>Matiz</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float GradingTint
    {
        get => Component.GetFloat("grading_tint");
        set => Component.SetFloat("grading_tint", value);
    }
    /// <summary>Vibração</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float GradingVibrance
    {
        get => Component.GetFloat("grading_vibrance");
        set => Component.SetFloat("grading_vibrance", value);
    }
    /// <summary>Brilho do lift</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float GradingLift
    {
        get => Component.GetFloat("grading_lift");
        set => Component.SetFloat("grading_lift", value);
    }
    /// <summary>Brilho do gama</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float GradingGamma
    {
        get => Component.GetFloat("grading_gamma");
        set => Component.SetFloat("grading_gamma", value);
    }
    /// <summary>Brilho do ganho</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float GradingGain
    {
        get => Component.GetFloat("grading_gain");
        set => Component.SetFloat("grading_gain", value);
    }
    /// <summary>Brilho das sombras</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float GradingShadows
    {
        get => Component.GetFloat("grading_shadows");
        set => Component.SetFloat("grading_shadows", value);
    }
    /// <summary>Brilho dos médios</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float GradingMidtones
    {
        get => Component.GetFloat("grading_midtones");
        set => Component.SetFloat("grading_midtones", value);
    }
    /// <summary>Brilho dos realces</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float GradingHighlights
    {
        get => Component.GetFloat("grading_highlights");
        set => Component.SetFloat("grading_highlights", value);
    }
    /// <summary>Início das sombras</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float GradingShadowsStart
    {
        get => Component.GetFloat("grading_shadows_start");
        set => Component.SetFloat("grading_shadows_start", value);
    }
    /// <summary>Fim das sombras</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float GradingShadowsEnd
    {
        get => Component.GetFloat("grading_shadows_end");
        set => Component.SetFloat("grading_shadows_end", value);
    }
    /// <summary>Início dos realces</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float GradingHighlightsStart
    {
        get => Component.GetFloat("grading_highlights_start");
        set => Component.SetFloat("grading_highlights_start", value);
    }
    /// <summary>Fim dos realces</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float GradingHighlightsEnd
    {
        get => Component.GetFloat("grading_highlights_end");
        set => Component.SetFloat("grading_highlights_end", value);
    }
    /// <summary>Auréola do sol</summary>
    /// <remarks>Faixa válida: 0 a 4.</remarks>
    public float SunGlare
    {
        get => Component.GetFloat("sun_glare");
        set => Component.SetFloat("sun_glare", value);
    }
    /// <summary>Fase da lua</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float MoonPhase
    {
        get => Component.GetFloat("moon_phase");
        set => Component.SetFloat("moon_phase", value);
    }
    /// <summary>Diâmetro da lua (°)</summary>
    /// <remarks>Faixa válida: 0.1 a 10.</remarks>
    public float MoonSize
    {
        get => Component.GetFloat("moon_size");
        set => Component.SetFloat("moon_size", value);
    }
    /// <summary>Brilho da lua</summary>
    /// <remarks>Faixa válida: 0 a 8.</remarks>
    public float MoonIntensity
    {
        get => Component.GetFloat("moon_intensity");
        set => Component.SetFloat("moon_intensity", value);
    }
    /// <summary>Brilho das estrelas</summary>
    /// <remarks>Faixa válida: 0 a 8.</remarks>
    public float StarIntensity
    {
        get => Component.GetFloat("star_intensity");
        set => Component.SetFloat("star_intensity", value);
    }
    /// <summary>Opacidade. Opacity: 0 deixa a camada transparente</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float CloudOpacity
    {
        get => Component.GetFloat("cloud_opacity");
        set => Component.SetFloat("cloud_opacity", value);
    }
    /// <summary>Cobertura. Parte do céu com cúmulos; 1 é céu encoberto</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float CloudCoverage
    {
        get => Component.GetFloat("cloud_coverage");
        set => Component.SetFloat("cloud_coverage", value);
    }
    /// <summary>Cirros. Véu fino e alto, em faixas</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float CloudCirrus
    {
        get => Component.GetFloat("cloud_cirrus");
        set => Component.SetFloat("cloud_cirrus", value);
    }
    /// <summary>Altitude (m). Altura da camada: perspectiva no céu e onde a sombra cai</summary>
    /// <remarks>Faixa válida: 100 a 20000.</remarks>
    public float CloudAltitude
    {
        get => Component.GetFloat("cloud_altitude");
        set => Component.SetFloat("cloud_altitude", value);
    }
    /// <summary>Tamanho (m). Largura de uma repetição do mapa de nuvens</summary>
    /// <remarks>Faixa válida: 200 a 100000.</remarks>
    public float CloudSize
    {
        get => Component.GetFloat("cloud_size");
        set => Component.SetFloat("cloud_size", value);
    }
    /// <summary>Compensação de exposição (EV). Brilho das nuvens relativo ao sol</summary>
    /// <remarks>Faixa válida: -8 a 8.</remarks>
    public float CloudExposure
    {
        get => Component.GetFloat("cloud_exposure");
        set => Component.SetFloat("cloud_exposure", value);
    }
    /// <summary>Direção do vento (°). 0° sopra para +X do mundo</summary>
    /// <remarks>Faixa válida: 0 a 360.</remarks>
    public float CloudWindOrientation
    {
        get => Component.GetFloat("cloud_wind_orientation");
        set => Component.SetFloat("cloud_wind_orientation", value);
    }
    /// <summary>Velocidade do vento (km/h). Deslocamento das nuvens e das sombras</summary>
    /// <remarks>Faixa válida: 0 a 400.</remarks>
    public float CloudWindSpeed
    {
        get => Component.GetFloat("cloud_wind_speed");
        set => Component.SetFloat("cloud_wind_speed", value);
    }
    /// <summary>Densidade. Autossombra: nuvens densas escurecem por dentro</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float CloudDensity
    {
        get => Component.GetFloat("cloud_density");
        set => Component.SetFloat("cloud_density", value);
    }
    /// <summary>Luz do céu. Ambient Probe Dimmer: quanto do céu ilumina a nuvem</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float CloudAmbientDimmer
    {
        get => Component.GetFloat("cloud_ambient_dimmer");
        set => Component.SetFloat("cloud_ambient_dimmer", value);
    }
    /// <summary>Força da sombra. Shadow Multiplier: escuridão da sombra no chão</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float CloudShadowMultiplier
    {
        get => Component.GetFloat("cloud_shadow_multiplier");
        set => Component.SetFloat("cloud_shadow_multiplier", value);
    }
    /// <summary>Ativo. Participa da seleção por prioridade</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Neblina. Aplica neblina exponencial uniforme ou com decaimento por altura</summary>
    public bool Fog
    {
        get => Component.GetBool("fog");
        set => Component.SetBool("fog", value);
    }
    /// <summary>Pós-processamento. Ativa os overrides autorais desta cena</summary>
    public bool Post
    {
        get => Component.GetBool("post");
        set => Component.SetBool("post", value);
    }
    /// <summary>Exposição automática. Mede luminância HDR e adapta a exposição por câmera</summary>
    public bool AutoExposure
    {
        get => Component.GetBool("auto_exposure");
        set => Component.SetBool("auto_exposure", value);
    }
    /// <summary>Peso central. Prioriza o centro da imagem na medição</summary>
    public bool AutoExposureCenterWeighted
    {
        get => Component.GetBool("auto_exposure_center_weighted");
        set => Component.SetBool("auto_exposure_center_weighted", value);
    }
    /// <summary>Bloom. Espalha altas luzes antes do tonemap</summary>
    public bool Bloom
    {
        get => Component.GetBool("bloom");
        set => Component.SetBool("bloom", value);
    }
    /// <summary>Vinheta. Escurece gradualmente as bordas</summary>
    public bool Vignette
    {
        get => Component.GetBool("vignette");
        set => Component.SetBool("vignette", value);
    }
    /// <summary>Grão de filme. Adiciona grão sensível à luminância depois da resolução temporal</summary>
    public bool FilmGrain
    {
        get => Component.GetBool("film_grain");
        set => Component.SetBool("film_grain", value);
    }
    /// <summary>Oclusão ambiente. Escurece contatos e concavidades a partir da profundidade da câmera</summary>
    public bool AmbientOcclusion
    {
        get => Component.GetBool("ambient_occlusion");
        set => Component.SetBool("ambient_occlusion", value);
    }
    /// <summary>Sobrescrever céu. Participa da mistura de céu e atmosfera</summary>
    public bool OverrideSky
    {
        get => Component.GetBool("override_sky");
        set => Component.SetBool("override_sky", value);
    }
    /// <summary>Sobrescrever neblina. Participa da mistura de neblina</summary>
    public bool OverrideFog
    {
        get => Component.GetBool("override_fog");
        set => Component.SetBool("override_fog", value);
    }
    /// <summary>Sobrescrever pós. Participa da mistura de exposição e pós</summary>
    public bool OverridePost
    {
        get => Component.GetBool("override_post");
        set => Component.SetBool("override_post", value);
    }
    /// <summary>Sobrescrever luz indireta. Participa da mistura de difuso e reflexo indiretos</summary>
    public bool OverrideIndirect
    {
        get => Component.GetBool("override_indirect");
        set => Component.SetBool("override_indirect", value);
    }
    /// <summary>Sobrescrever nuvens. Participa da mistura do Cloud Layer, separado do céu</summary>
    public bool OverrideClouds
    {
        get => Component.GetBool("override_clouds");
        set => Component.SetBool("override_clouds", value);
    }
    /// <summary>Alta qualidade. Aumenta as amostras de vista e luz; custa mais GPU</summary>
    public bool PhysicalAtmosphereHighQuality
    {
        get => Component.GetBool("physical_atmosphere_high_quality");
        set => Component.SetBool("physical_atmosphere_high_quality", value);
    }
    /// <summary>Nuvens. Cloud Layer: camada 2D iluminada pelo sol, com vento e sombra</summary>
    public bool Clouds
    {
        get => Component.GetBool("clouds");
        set => Component.SetBool("clouds", value);
    }
    /// <summary>Projetar sombras. A luz direcional principal passa pelas nuvens antes de chegar ao chão</summary>
    public bool CloudShadows
    {
        get => Component.GetBool("cloud_shadows");
        set => Component.SetBool("cloud_shadows", value);
    }
    public enum SkyOption : uint
    {
        HDRI = 0,
        Atmosfera = 1,
        AtmosferaFisica = 2,
    }
    /// <summary>Céu. Fonte visual do céu</summary>
    public SkyOption Sky
    {
        get => (SkyOption)Component.GetEnum("sky");
        set => Component.SetEnum("sky", (uint)value);
    }
    public enum ToneMapperOption : uint
    {
        Reinhard = 0,
        ACES = 1,
        AgX = 2,
    }
    /// <summary>Tonemapping. Curva aplicada ao HDR antes da saída</summary>
    public ToneMapperOption ToneMapper
    {
        get => (ToneMapperOption)Component.GetEnum("tone_mapper");
        set => Component.SetEnum("tone_mapper", (uint)value);
    }
    public enum VolumeShapeOption : uint
    {
        Global = 0,
        Caixa = 1,
        Esfera = 2,
    }
    /// <summary>Modo. Global afeta toda vista; formas usam o Transform do objeto</summary>
    public VolumeShapeOption VolumeShape
    {
        get => (VolumeShapeOption)Component.GetEnum("volume_shape");
        set => Component.SetEnum("volume_shape", (uint)value);
    }
    public enum VolumeLayerOption : uint
    {
        Ambiente0 = 0,
        Ambiente1 = 1,
        Ambiente2 = 2,
        Ambiente3 = 3,
        Ambiente4 = 4,
        Ambiente5 = 5,
        Ambiente6 = 6,
        Ambiente7 = 7,
    }
    /// <summary>Camada. Filtro consultado pela câmera</summary>
    public VolumeLayerOption VolumeLayer
    {
        get => (VolumeLayerOption)Component.GetEnum("volume_layer");
        set => Component.SetEnum("volume_layer", (uint)value);
    }
    /// <summary>Perfil. Recurso do projeto por slot</summary>
    public AssetGuid GetProfile(uint slot = 0) => Component.GetResource("profile", slot);
    public void SetProfile(AssetGuid value, uint slot = 0) => Component.SetResource("profile", value, slot);
    /// <summary>Mapa HDRI. Recurso do projeto por slot</summary>
    public AssetGuid GetEnvironmentMap(uint slot = 0) => Component.GetResource("environment_map", slot);
    public void SetEnvironmentMap(AssetGuid value, uint slot = 0) => Component.SetResource("environment_map", value, slot);
}

/// <summary>Clima: Chuva, neve e tempestade: dirige as nuvens do Ambiente e as partículas. Família Renderização · Ambiente.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.render-pipelines.high-definition@17.0/manual/create-simple-clouds-cloud-layer.html</remarks>
public readonly struct Weather : IComponentFacade<Weather>
{
    public static string TypeId => "astra.environment.weather";
    public static Weather Wrap(Component component) => new(component);
    public Component Component { get; }
    public Weather(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Intensidade. Quanto chove ou neva e quão fechado fica o céu</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Intensity
    {
        get => Component.GetFloat("intensity");
        set => Component.SetFloat("intensity", value);
    }
    /// <summary>Transição (s). Em Play, tempo para chegar ao clima novo quando tipo ou intensidade mudam</summary>
    /// <remarks>Faixa válida: 0 a 120.</remarks>
    public float TransitionSeconds
    {
        get => Component.GetFloat("transition_seconds");
        set => Component.SetFloat("transition_seconds", value);
    }
    /// <summary>Gotas por segundo (/s). Emissão da Chuva na intensidade máxima</summary>
    /// <remarks>Faixa válida: 0 a 20000.</remarks>
    public float RainRate
    {
        get => Component.GetFloat("rain_rate");
        set => Component.SetFloat("rain_rate", value);
    }
    /// <summary>Flocos por segundo (/s). Emissão da Neve na intensidade máxima</summary>
    /// <remarks>Faixa válida: 0 a 20000.</remarks>
    public float SnowRate
    {
        get => Component.GetFloat("snow_rate");
        set => Component.SetFloat("snow_rate", value);
    }
    /// <summary>Altura sobre a câmera (m). Onde ficam os emissores, acima da câmera da cena</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float HeightAboveCamera
    {
        get => Component.GetFloat("height_above_camera");
        set => Component.SetFloat("height_above_camera", value);
    }
    /// <summary>Vento na precipitação (×). Quanto o vento das nuvens deste Ambiente inclina a chuva e arrasta a neve</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float WindInfluence
    {
        get => Component.GetFloat("wind_influence");
        set => Component.SetFloat("wind_influence", value);
    }
    /// <summary>Neblina máxima (1/m). Densidade da neblina do Ambiente com chuva ou neve no máximo; vale com Sobrescrever neblina</summary>
    /// <remarks>Faixa válida: 0 a 0.2.</remarks>
    public float FogDensity
    {
        get => Component.GetFloat("fog_density");
        set => Component.SetFloat("fog_density", value);
    }
    /// <summary>Intervalo médio (s). Tempo médio entre relâmpagos em Play</summary>
    /// <remarks>Faixa válida: 1 a 120.</remarks>
    public float LightningInterval
    {
        get => Component.GetFloat("lightning_interval");
        set => Component.SetFloat("lightning_interval", value);
    }
    /// <summary>Clarão (EV). Quanto o clarão acende as nuvens e a neblina</summary>
    /// <remarks>Faixa válida: 0 a 8.</remarks>
    public float LightningBrightness
    {
        get => Component.GetFloat("lightning_brightness");
        set => Component.SetFloat("lightning_brightness", value);
    }
    /// <summary>Ativo. Desligado, o clima não escreve mais nada; os valores ficam como estão</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Seguir a câmera. Leva Chuva e Neve para cima da câmera da cena, como filhos da Main Camera</summary>
    public bool FollowCamera
    {
        get => Component.GetBool("follow_camera");
        set => Component.SetBool("follow_camera", value);
    }
    /// <summary>Relâmpagos. Clarões aleatórios nas nuvens durante a Tempestade</summary>
    public bool Lightning
    {
        get => Component.GetBool("lightning");
        set => Component.SetBool("lightning", value);
    }
    public enum KindOption : uint
    {
        Limpo = 0,
        Nublado = 1,
        Chuva = 2,
        Tempestade = 3,
        Neve = 4,
    }
    /// <summary>Tipo. Limpo, nublado, chuva, tempestade ou neve: muda nuvens, neblina e precipitação</summary>
    public KindOption Kind
    {
        get => (KindOption)Component.GetEnum("kind");
        set => Component.SetEnum("kind", (uint)value);
    }
    /// <summary>Chuva. Partículas de chuva: o clima dosa a emissão e as leva sobre a câmera</summary>
    public ObjectReference Rain
    {
        get => Component.GetReference("rain");
        set => Component.SetReference("rain", value);
    }
    /// <summary>Neve. Partículas de neve: o clima dosa a emissão e as leva sobre a câmera</summary>
    public ObjectReference Snow
    {
        get => Component.GetReference("snow");
        set => Component.SetReference("snow", value);
    }
}

/// <summary>Luz: Direcional, pontual ou spot. Família Luz · Luzes.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-Light.html</remarks>
public readonly struct Light : IComponentFacade<Light>
{
    public static string TypeId => "astra.render.light";
    public static Light Wrap(Component component) => new(component);
    public Component Component { get; }
    public Light(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Cor. Cor sRGB, como o seletor; o motor converte para linear</summary>
    public Vector3 Color
    {
        get => new(Component.GetFloat("color.r"), Component.GetFloat("color.g"), Component.GetFloat("color.b"));
        set => Component.SetVector3("color", value);
    }
    /// <summary>Temperatura (K). Multiplicada pela cor linear; D65 é 6500 K</summary>
    /// <remarks>Faixa válida: 1667 a 25000.</remarks>
    public float ColorTemperature
    {
        get => Component.GetFloat("color_temperature");
        set => Component.SetFloat("color_temperature", value);
    }
    /// <summary>Intensidade</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float Intensity
    {
        get => Component.GetFloat("intensity");
        set => Component.SetFloat("intensity", value);
    }
    /// <summary>Alcance (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 1000.</remarks>
    public float Range
    {
        get => Component.GetFloat("range");
        set => Component.SetFloat("range", value);
    }
    /// <summary>Ângulo do spot (°). Abertura total do cone (Spot Angle); a luz zera na borda</summary>
    /// <remarks>Faixa válida: 1 a 179.</remarks>
    public float SpotAngle
    {
        get => Component.GetFloat("spot_angle");
        set => Component.SetFloat("spot_angle", value);
    }
    /// <summary>Ângulo interno do spot (°). Abertura total do miolo com intensidade plena (Inner Spot Angle)</summary>
    /// <remarks>Faixa válida: 0 a 179.</remarks>
    public float InnerSpotAngle
    {
        get => Component.GetFloat("inner_spot_angle");
        set => Component.SetFloat("inner_spot_angle", value);
    }
    /// <summary>Força da sombra. 1 é sombra opaca; abaixo disso a luz vaza pelo oclusor</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float ShadowStrength
    {
        get => Component.GetFloat("shadow_strength");
        set => Component.SetFloat("shadow_strength", value);
    }
    /// <summary>Desvio (texel). Afasta a comparação de profundidade e some com a acne; 0,05 é o desvio da política do aparelho</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float ShadowBias
    {
        get => Component.GetFloat("shadow_bias");
        set => Component.SetFloat("shadow_bias", value);
    }
    /// <summary>Desvio na normal (texel). Desloca a amostra ao longo da normal; 0,4 é o deslocamento da política do aparelho</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float ShadowNormalBias
    {
        get => Component.GetFloat("shadow_normal_bias");
        set => Component.SetFloat("shadow_normal_bias", value);
    }
    /// <summary>Plano próximo da sombra (m). Perto demais perde precisão; longe demais corta o que está junto da lâmpada</summary>
    /// <remarks>Faixa válida: 0.01 a 10.</remarks>
    public float ShadowNearPlane
    {
        get => Component.GetFloat("shadow_near_plane");
        set => Component.SetFloat("shadow_near_plane", value);
    }
    /// <summary>Acesa. Ativa a contribuição desta luz</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Filtro por temperatura. Multiplica a cor pela temperatura de corpo negro</summary>
    public bool UseColorTemperature
    {
        get => Component.GetBool("use_color_temperature");
        set => Component.SetBool("use_color_temperature", value);
    }
    public enum KindOption : uint
    {
        Direcional = 0,
        Pontual = 1,
        Spot = 2,
    }
    /// <summary>Tipo. Direcional ilumina a cena inteira numa direção; pontual e spot partem de um ponto</summary>
    public KindOption Kind
    {
        get => (KindOption)Component.GetEnum("kind");
        set => Component.SetEnum("kind", (uint)value);
    }
    public enum UnitOption : uint
    {
        RelativaSemUnidade = 0,
        LuxCandela = 1,
        LuxLumen = 2,
    }
    /// <summary>Unidade. Relativa é a intensidade da URP; lux, candela e lúmen são as unidades físicas da HDRP</summary>
    public UnitOption Unit
    {
        get => (UnitOption)Component.GetEnum("unit");
        set => Component.SetEnum("unit", (uint)value);
    }
    public enum ShadowModeOption : uint
    {
        SemSombra = 0,
        SombraDura = 1,
        SombraSuave = 2,
    }
    /// <summary>Tipo de sombra. Dura usa uma busca; suave filtra a borda. A política do aparelho pode desligar a sombra</summary>
    public ShadowModeOption ShadowMode
    {
        get => (ShadowModeOption)Component.GetEnum("shadow_mode");
        set => Component.SetEnum("shadow_mode", (uint)value);
    }
    public enum ShadowResolutionOption : uint
    {
        Automatica = 0,
        Baixa = 1,
        Media = 2,
        Alta = 3,
        MuitoAlta = 4,
    }
    /// <summary>Resolução da sombra. Automática escolhe pelo tamanho da luz na tela</summary>
    public ShadowResolutionOption ShadowResolution
    {
        get => (ShadowResolutionOption)Component.GetEnum("shadow_resolution");
        set => Component.SetEnum("shadow_resolution", (uint)value);
    }
    [System.Flags]
    public enum CullingMaskOption : uint
    {
        Padrao = 1,
        Camada1 = 2,
        Camada2 = 4,
        Camada3 = 8,
        Camada4 = 16,
        Camada5 = 32,
        Camada6 = 64,
        Camada7 = 128,
        Camada8 = 256,
        Camada9 = 512,
        Camada10 = 1024,
        Camada11 = 2048,
        Camada12 = 4096,
        Camada13 = 8192,
        Camada14 = 16384,
        Camada15 = 32768,
        Camada16 = 65536,
        Camada17 = 131072,
        Camada18 = 262144,
        Camada19 = 524288,
        Camada20 = 1048576,
        Camada21 = 2097152,
        Camada22 = 4194304,
        Camada23 = 8388608,
        Camada24 = 16777216,
        Camada25 = 33554432,
        Camada26 = 67108864,
        Camada27 = 134217728,
        Camada28 = 268435456,
        Camada29 = 536870912,
        Camada30 = 1073741824,
        Camada31 = 2147483648,
    }
    /// <summary>Máscara de culling. Camadas de objeto que esta luz ilumina</summary>
    public CullingMaskOption CullingMask
    {
        get => (CullingMaskOption)Component.GetEnum("culling_mask");
        set => Component.SetEnum("culling_mask", (uint)value);
    }
    [System.Flags]
    public enum RenderingLayersOption : uint
    {
        Padrao = 1,
        Camada1 = 2,
        Camada2 = 4,
        Camada3 = 8,
        Camada4 = 16,
        Camada5 = 32,
        Camada6 = 64,
        Camada7 = 128,
    }
    /// <summary>Camadas de renderização. Ilumina só renderers com uma camada de renderização em comum</summary>
    public RenderingLayersOption RenderingLayers
    {
        get => (RenderingLayersOption)Component.GetEnum("rendering_layers");
        set => Component.SetEnum("rendering_layers", (uint)value);
    }
}

/// <summary>Hora do dia: Move o sol pela hora, data e latitude; dia, crepúsculo e noite. Família Luz · Luzes.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_directionallight3d.html</remarks>
public readonly struct TimeOfDay : IComponentFacade<TimeOfDay>
{
    public static string TypeId => "astra.render.time_of_day";
    public static TimeOfDay Wrap(Component component) => new(component);
    public Component Component { get; }
    public TimeOfDay(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Hora (h). Hora solar local; com Duração do dia, avança em Play</summary>
    /// <remarks>Faixa válida: 0 a 24.</remarks>
    public float Hour
    {
        get => Component.GetFloat("hour");
        set => Component.SetFloat("hour", value);
    }
    /// <summary>Duração do dia (min). Minutos reais para 24 h de jogo; 0 congela a hora</summary>
    /// <remarks>Faixa válida: 0 a 1440.</remarks>
    public float DayLengthMinutes
    {
        get => Component.GetFloat("day_length_minutes");
        set => Component.SetFloat("day_length_minutes", value);
    }
    /// <summary>Latitude (°). Altura do sol ao meio-dia e duração da noite; negativo no hemisfério sul</summary>
    /// <remarks>Faixa válida: -90 a 90.</remarks>
    public float Latitude
    {
        get => Component.GetFloat("latitude");
        set => Component.SetFloat("latitude", value);
    }
    /// <summary>Dia do ano. Estação e fase da lua; 172 é o solstício de junho</summary>
    /// <remarks>Faixa válida: 1 a 366.</remarks>
    public float DayOfYear
    {
        get => Component.GetFloat("day_of_year");
        set => Component.SetFloat("day_of_year", value);
    }
    /// <summary>Norte (°). Gira o caminho do sol em torno do eixo vertical</summary>
    /// <remarks>Faixa válida: 0 a 360.</remarks>
    public float NorthDegrees
    {
        get => Component.GetFloat("north_degrees");
        set => Component.SetFloat("north_degrees", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Iluminação da cena: Fonte do sol: qual luz direcional é o sol da cena. Família Luz · Luzes.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/lighting-window.html</remarks>
public readonly struct LightingSettings : IComponentFacade<LightingSettings>
{
    public static string TypeId => "astra.render.lighting_settings";
    public static LightingSettings Wrap(Component component) => new(component);
    public Component Component { get; }
    public LightingSettings(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Fonte do sol. A luz direcional que é o sol da cena: projeta as cascatas e aparece no céu</summary>
    public ObjectReference SunSource
    {
        get => Component.GetReference("sun_source");
        set => Component.SetReference("sun_source", value);
    }
}

/// <summary>Câmera: Projeção e enquadramento. Família Câmera · Projeção.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-Camera.html</remarks>
public readonly struct Camera : IComponentFacade<Camera>
{
    public static string TypeId => "astra.camera";
    public static Camera Wrap(Component component) => new(component);
    public Component Component { get; }
    public Camera(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Campo vertical (°)</summary>
    /// <remarks>Faixa válida: 1 a 170.</remarks>
    public float VerticalFov
    {
        get => Component.GetFloat("vertical_fov");
        set => Component.SetFloat("vertical_fov", value);
    }
    /// <summary>Próximo (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 10000.</remarks>
    public float NearPlane
    {
        get => Component.GetFloat("near_plane");
        set => Component.SetFloat("near_plane", value);
    }
    /// <summary>Distante (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 1000000.</remarks>
    public float FarPlane
    {
        get => Component.GetFloat("far_plane");
        set => Component.SetFloat("far_plane", value);
    }
    /// <summary>Prioridade</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float Priority
    {
        get => Component.GetFloat("priority");
        set => Component.SetFloat("priority", value);
    }
    /// <summary>Meia altura (m). Metade da altura visível. Zoom altera esta extensão.</summary>
    /// <remarks>Faixa válida: 0.001 a 100000.</remarks>
    public float OrthographicHalfHeight
    {
        get => Component.GetFloat("orthographic_half_height");
        set => Component.SetFloat("orthographic_half_height", value);
    }
    /// <summary>Usar no Play</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum ProjectionOption : uint
    {
        Perspectiva = 0,
        Ortografica = 1,
    }
    /// <summary>Projeção</summary>
    public ProjectionOption Projection
    {
        get => (ProjectionOption)Component.GetEnum("projection");
        set => Component.SetEnum("projection", (uint)value);
    }
    public enum EnvironmentMaskOption : uint
    {
        TodosOsAmbientes = 4294967295,
        Ambiente0 = 1,
        Ambiente1 = 2,
        Ambiente2 = 4,
        Ambiente3 = 8,
        Ambiente4 = 16,
        Ambiente5 = 32,
        Ambiente6 = 64,
        Ambiente7 = 128,
    }
    /// <summary>Ambientes. Volumes que esta câmera consulta</summary>
    public EnvironmentMaskOption EnvironmentMask
    {
        get => (EnvironmentMaskOption)Component.GetEnum("environment_mask");
        set => Component.SetEnum("environment_mask", (uint)value);
    }
}

/// <summary>Olhar: Rotação local da câmera por entrada ou script. Família Câmera · Controle.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachinePanTilt.html</remarks>
public readonly struct CameraLook : IComponentFacade<CameraLook>
{
    public static string TypeId => "astra.camera.look";
    public static CameraLook Wrap(Component component) => new(component);
    public Component Component { get; }
    public CameraLook(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Sensibilidade horizontal graus/tela (°)</summary>
    /// <remarks>Faixa válida: 0 a 720.</remarks>
    public float YawSensitivity
    {
        get => Component.GetFloat("yaw_sensitivity");
        set => Component.SetFloat("yaw_sensitivity", value);
    }
    /// <summary>Sensibilidade vertical graus/tela (°)</summary>
    /// <remarks>Faixa válida: 0 a 720.</remarks>
    public float PitchSensitivity
    {
        get => Component.GetFloat("pitch_sensitivity");
        set => Component.SetFloat("pitch_sensitivity", value);
    }
    /// <summary>Limite vertical graus (°)</summary>
    /// <remarks>Faixa válida: 1 a 89.</remarks>
    public float PitchLimit
    {
        get => Component.GetFloat("pitch_limit");
        set => Component.SetFloat("pitch_limit", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Acompanhar alvo: Posiciona a câmera após física e animação. Família Câmera · Controle.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineFollow.html</remarks>
public readonly struct CameraFollow : IComponentFacade<CameraFollow>
{
    public static string TypeId => "astra.camera.follow";
    public static CameraFollow Wrap(Component component) => new(component);
    public Component Component { get; }
    public CameraFollow(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Deslocamento</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Amortecimento (s)</summary>
    /// <remarks>Faixa válida: 0 a 30.</remarks>
    public float DampingSeconds
    {
        get => Component.GetFloat("damping_seconds");
        set => Component.SetFloat("damping_seconds", value);
    }
    /// <summary>Altura do pivô (m). Altura em mundo do ponto observado no alvo; usada somente em órbita</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float PivotHeight
    {
        get => Component.GetFloat("pivot_height");
        set => Component.SetFloat("pivot_height", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Orbitar alvo. Gira o deslocamento pela orientação da câmera; CameraLook controla a órbita</summary>
    public bool Orbit
    {
        get => Component.GetBool("orbit");
        set => Component.SetBool("orbit", value);
    }
    /// <summary>Alvo</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Câmera virtual: Pose e lente por prioridade: seguir, órbita, mira, desoclusão e tremor. Família Câmera · Câmera virtual.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineCamera.html</remarks>
public readonly struct VirtualCamera : IComponentFacade<VirtualCamera>
{
    public static string TypeId => "astra.camera.virtual";
    public static VirtualCamera Wrap(Component component) => new(component);
    public Component Component { get; }
    public VirtualCamera(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Deslocamento</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Ajuste da mira</summary>
    public Vector3 AimOffset
    {
        get => new(Component.GetFloat("aim_offset_x"), Component.GetFloat("aim_offset_y"), Component.GetFloat("aim_offset_z"));
        set => Component.SetVector3("aim_offset", value);
    }
    /// <summary>Prioridade. A maior entre as câmeras virtuais ativas fica ao vivo; no empate vence a ativada por último</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float Priority
    {
        get => Component.GetFloat("priority");
        set => Component.SetFloat("priority", value);
    }
    /// <summary>Campo vertical (°). Usado quando a Câmera do Cérebro é perspectiva</summary>
    /// <remarks>Faixa válida: 1 a 170.</remarks>
    public float VerticalFov
    {
        get => Component.GetFloat("vertical_fov");
        set => Component.SetFloat("vertical_fov", value);
    }
    /// <summary>Meia altura (m). Usado quando a Câmera do Cérebro é ortográfica</summary>
    /// <remarks>Faixa válida: 0.001 a 100000.</remarks>
    public float OrthographicHalfHeight
    {
        get => Component.GetFloat("orthographic_half_height");
        set => Component.SetFloat("orthographic_half_height", value);
    }
    /// <summary>Próximo (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 10000.</remarks>
    public float NearPlane
    {
        get => Component.GetFloat("near_plane");
        set => Component.SetFloat("near_plane", value);
    }
    /// <summary>Distante (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 1000000.</remarks>
    public float FarPlane
    {
        get => Component.GetFloat("far_plane");
        set => Component.SetFloat("far_plane", value);
    }
    /// <summary>Inclinação holandesa (°). Giro em torno da direção de visão</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float Dutch
    {
        get => Component.GetFloat("dutch");
        set => Component.SetFloat("dutch", value);
    }
    /// <summary>Raio da órbita (m). Distância ao alvo rastreado</summary>
    /// <remarks>Faixa válida: 0.01 a 1000.</remarks>
    public float OrbitRadius
    {
        get => Component.GetFloat("orbit_radius");
        set => Component.SetFloat("orbit_radius", value);
    }
    /// <summary>Ângulo horizontal (°). Giro em torno do eixo Y do mundo; zero fica atrás do alvo (−Z)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float OrbitYaw
    {
        get => Component.GetFloat("orbit_yaw");
        set => Component.SetFloat("orbit_yaw", value);
    }
    /// <summary>Ângulo vertical (°). Positivo põe a câmera acima do alvo</summary>
    /// <remarks>Faixa válida: -89 a 89.</remarks>
    public float OrbitPitch
    {
        get => Component.GetFloat("orbit_pitch");
        set => Component.SetFloat("orbit_pitch", value);
    }
    /// <summary>Vertical mínimo (°)</summary>
    /// <remarks>Faixa válida: -89 a 89.</remarks>
    public float OrbitPitchMin
    {
        get => Component.GetFloat("orbit_pitch_min");
        set => Component.SetFloat("orbit_pitch_min", value);
    }
    /// <summary>Vertical máximo (°)</summary>
    /// <remarks>Faixa válida: -89 a 89.</remarks>
    public float OrbitPitchMax
    {
        get => Component.GetFloat("orbit_pitch_max");
        set => Component.SetFloat("orbit_pitch_max", value);
    }
    /// <summary>Sensibilidade horizontal (°). Graus por largura de tela arrastada</summary>
    /// <remarks>Faixa válida: 0 a 2000.</remarks>
    public float OrbitYawSensitivity
    {
        get => Component.GetFloat("orbit_yaw_sensitivity");
        set => Component.SetFloat("orbit_yaw_sensitivity", value);
    }
    /// <summary>Sensibilidade vertical (°). Graus por altura de tela arrastada</summary>
    /// <remarks>Faixa válida: 0 a 2000.</remarks>
    public float OrbitPitchSensitivity
    {
        get => Component.GetFloat("orbit_pitch_sensitivity");
        set => Component.SetFloat("orbit_pitch_sensitivity", value);
    }
    /// <summary>Amortecimento da posição (s). Tempo para alcançar o alvo; zero acompanha sem atraso</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float PositionDamping
    {
        get => Component.GetFloat("position_damping");
        set => Component.SetFloat("position_damping", value);
    }
    /// <summary>Amortecimento da rotação (s)</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float RotationDamping
    {
        get => Component.GetFloat("rotation_damping");
        set => Component.SetFloat("rotation_damping", value);
    }
    /// <summary>Raio da câmera (m). Distância mantida dos obstáculos; zero usa um raio de luz</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float CameraRadius
    {
        get => Component.GetFloat("camera_radius");
        set => Component.SetFloat("camera_radius", value);
    }
    /// <summary>Distância mínima do alvo (m). Obstáculos mais perto do alvo que isto são ignorados</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float MinimumDistance
    {
        get => Component.GetFloat("minimum_distance");
        set => Component.SetFloat("minimum_distance", value);
    }
    /// <summary>Amortecimento ao liberar (s). Tempo para voltar à distância livre; a aproximação é imediata</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float CollisionDamping
    {
        get => Component.GetFloat("collision_damping");
        set => Component.SetFloat("collision_damping", value);
    }
    /// <summary>Tremor da rotação (°)</summary>
    /// <remarks>Faixa válida: 0 a 90.</remarks>
    public float NoiseAmplitude
    {
        get => Component.GetFloat("noise_amplitude");
        set => Component.SetFloat("noise_amplitude", value);
    }
    /// <summary>Tremor da posição (m)</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float NoisePositionAmplitude
    {
        get => Component.GetFloat("noise_position_amplitude");
        set => Component.SetFloat("noise_position_amplitude", value);
    }
    /// <summary>Frequência (Hz)</summary>
    /// <remarks>Faixa válida: 0 a 50.</remarks>
    public float NoiseFrequency
    {
        get => Component.GetFloat("noise_frequency");
        set => Component.SetFloat("noise_frequency", value);
    }
    /// <summary>Duração da entrada (s)</summary>
    /// <remarks>Faixa válida: 0 a 60.</remarks>
    public float BlendTime
    {
        get => Component.GetFloat("blend_time");
        set => Component.SetFloat("blend_time", value);
    }
    /// <summary>Ativa. Desligada deixa de concorrer; a configuração é preservada</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Controlar pela entrada de olhar. A ação Olhar do mapa de entrada gira a órbita enquanto esta câmera está ao vivo</summary>
    public bool OrbitInput
    {
        get => Component.GetBool("orbit_input");
        set => Component.SetBool("orbit_input", value);
    }
    /// <summary>Evitar obstáculos. Aproxima a câmera do alvo quando um colisor bloqueia a linha de visão</summary>
    public bool AvoidObstacles
    {
        get => Component.GetBool("avoid_obstacles");
        set => Component.SetBool("avoid_obstacles", value);
    }
    public enum PositionModeOption : uint
    {
        FixaPoseAutorada = 0,
        Seguir = 1,
        Orbita = 2,
    }
    /// <summary>Posição. Seguir e Órbita usam o alvo rastreado; sem alvo a câmera fica na pose autorada</summary>
    public PositionModeOption PositionMode
    {
        get => (PositionModeOption)Component.GetEnum("position_mode");
        set => Component.SetEnum("position_mode", (uint)value);
    }
    public enum BindingOption : uint
    {
        EixosDoMundo = 0,
        GuinadaDoAlvo = 1,
    }
    /// <summary>Referencial do deslocamento. Guinada do alvo gira o deslocamento junto com o alvo (Lock To Target With World Up)</summary>
    public BindingOption Binding
    {
        get => (BindingOption)Component.GetEnum("binding");
        set => Component.SetEnum("binding", (uint)value);
    }
    public enum RotationModeOption : uint
    {
        FixaRotacaoAutorada = 0,
        OlharParaOAlvo = 1,
        RotacaoDoAlvo = 2,
    }
    /// <summary>Rotação. Olhar para o alvo usa o alvo de mira, ou o rastreado quando ele está vazio</summary>
    public RotationModeOption RotationMode
    {
        get => (RotationModeOption)Component.GetEnum("rotation_mode");
        set => Component.SetEnum("rotation_mode", (uint)value);
    }
    public enum CollisionLayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada dos obstáculos. Todas ou só uma camada física do projeto</summary>
    public CollisionLayerOption CollisionLayer
    {
        get => (CollisionLayerOption)Component.GetEnum("collision_layer");
        set => Component.SetEnum("collision_layer", (uint)value);
    }
    public enum BlendStyleOption : uint
    {
        PadraoDoCerebro = 0,
        Corte = 1,
        Suave = 2,
        Linear = 3,
        EntradaSuave = 4,
        SaidaSuave = 5,
        EntradaBrusca = 6,
        SaidaBrusca = 7,
    }
    /// <summary>Entrada. Curva usada quando esta câmera entra ao vivo</summary>
    public BlendStyleOption BlendStyle
    {
        get => (BlendStyleOption)Component.GetEnum("blend_style");
        set => Component.SetEnum("blend_style", (uint)value);
    }
    /// <summary>Alvo rastreado. Objeto que Seguir e Órbita acompanham</summary>
    public ObjectReference TrackingTarget
    {
        get => Component.GetReference("tracking_target");
        set => Component.SetReference("tracking_target", value);
    }
    /// <summary>Alvo de mira. Ponto que Olhar para o alvo e a desoclusão usam</summary>
    public ObjectReference LookAtTarget
    {
        get => Component.GetReference("look_at_target");
        set => Component.SetReference("look_at_target", value);
    }
    /// <summary>Priorizar. Vence o empate com outras câmeras de mesma prioridade, como se tivesse sido ativada agora</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Prioritize() => Component.Invoke("prioritize");
    /// <summary>Encaixar. Descarta o amortecimento no próximo quadro (depois de teletransportar o alvo)</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Snap() => Component.Invoke("snap");
    /// <summary>Ao vivo. Verdadeiro quando algum Cérebro está mostrando esta câmera</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool IsLive() => Component.Invoke("is_live").AsBoolean();
    /// <summary>Entrou ao vivo. Emitido quando um Cérebro passa a mostrar esta câmera; carrega a câmera anterior. Payload: Outra câmera: objeto</summary>
    public ComponentSubscription OnActivated(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "activated", handler);
    /// <summary>Saiu do ar. Emitido quando outra câmera assume o Cérebro; carrega a nova câmera. Payload: Outra câmera: objeto</summary>
    public ComponentSubscription OnDeactivated(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "deactivated", handler);
}

/// <summary>Cérebro de câmera: Mostra a câmera virtual de maior prioridade e faz a transição. Família Câmera · Câmera virtual.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.cinemachine@3.1/manual/CinemachineBrain.html</remarks>
public readonly struct CameraBrain : IComponentFacade<CameraBrain>
{
    public static string TypeId => "astra.camera.brain";
    public static CameraBrain Wrap(Component component) => new(component);
    public Component Component { get; }
    public CameraBrain(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Duração padrão (s). Usada quando a câmera que entra não define a própria transição</summary>
    /// <remarks>Faixa válida: 0 a 60.</remarks>
    public float DefaultBlendTime
    {
        get => Component.GetFloat("default_blend_time");
        set => Component.SetFloat("default_blend_time", value);
    }
    /// <summary>Ativo. Desligado devolve a câmera à autoria; a pose do último quadro permanece</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Ignorar escala de tempo. Câmeras e transições seguem o tempo real mesmo em câmera lenta ou pausa por escala</summary>
    public bool IgnoreTimeScale
    {
        get => Component.GetBool("ignore_time_scale");
        set => Component.SetBool("ignore_time_scale", value);
    }
    public enum DefaultBlendOption : uint
    {
        Corte = 1,
        Suave = 2,
        Linear = 3,
        EntradaSuave = 4,
        SaidaSuave = 5,
        EntradaBrusca = 6,
        SaidaBrusca = 7,
    }
    /// <summary>Transição padrão</summary>
    public DefaultBlendOption DefaultBlend
    {
        get => (DefaultBlendOption)Component.GetEnum("default_blend");
        set => Component.SetEnum("default_blend", (uint)value);
    }
    /// <summary>Câmera ao vivo. Câmera virtual que o Cérebro está mostrando; vazio sem nenhuma</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public ObjectReference LiveCamera() => Component.Reference(Component.Invoke("live_camera"));
    /// <summary>Em transição. Verdadeiro enquanto mistura duas câmeras</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool Blending() => Component.Invoke("blending").AsBoolean();
    /// <summary>Câmera ativada. Emitido quando uma câmera virtual assume, já no primeiro quadro da transição. Payload: Câmera que entrou: objeto, Câmera que saiu: objeto</summary>
    public ComponentSubscription OnCameraActivated(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "camera_activated", handler);
    /// <summary>Corte de câmera. Emitido quando a troca acontece sem transição. Payload: Câmera: objeto</summary>
    public ComponentSubscription OnCameraCut(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "camera_cut", handler);
    /// <summary>Transição concluída. Emitido quando a mistura termina e só a câmera nova aparece. Payload: Câmera: objeto</summary>
    public ComponentSubscription OnBlendFinished(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "blend_finished", handler);
}

/// <summary>Campo de gravidade: Gravidade local em volume sobre corpos dinâmicos. Família Física 3D · Campos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_area3d.html</remarks>
public readonly struct GravityField : IComponentFacade<GravityField>
{
    public static string TypeId => "astra.physics.field.gravity";
    public static GravityField Wrap(Component component) => new(component);
    public Component Component { get; }
    public GravityField(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Meias XYZ</summary>
    public Vector3 HalfExtents
    {
        get => new(Component.GetFloat("half_x"), Component.GetFloat("half_y"), Component.GetFloat("half_z"));
        set => Component.SetVector3("half_extents", value);
    }
    /// <summary>Centro local</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Gravidade local</summary>
    public Vector3 Vector
    {
        get => new(Component.GetFloat("vector_x"), Component.GetFloat("vector_y"), Component.GetFloat("vector_z"));
        set => Component.SetVector3("vector", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 10000.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Acordar corpos. Desligado: corpos em repouso permanecem dormindo</summary>
    public bool WakeBodies
    {
        get => Component.GetBool("wake_bodies");
        set => Component.SetBool("wake_bodies", value);
    }
    /// <summary>Substituir gravidade do mundo. Sobrepostos somam vetores e cancelam a gravidade padrão uma única vez</summary>
    public bool ReplaceWorldGravity
    {
        get => Component.GetBool("replace_world_gravity");
        set => Component.SetBool("replace_world_gravity", value);
    }
    public enum ShapeOption : uint
    {
        Caixa = 0,
        Esfera = 1,
    }
    /// <summary>Forma. Escala e orientação são as do objeto e de seus pais</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    public enum FalloffOption : uint
    {
        Uniforme = 0,
        Linear = 1,
        Suave = 2,
    }
    /// <summary>Queda de influência. Centro vale 1; Linear e Suave chegam a zero na borda</summary>
    public FalloffOption Falloff
    {
        get => (FalloffOption)Component.GetEnum("falloff");
        set => Component.SetEnum("falloff", (uint)value);
    }
    public enum AffectedLayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada afetada. Todas ou uma camada física do projeto; não altera a matriz de colisão</summary>
    public AffectedLayerOption AffectedLayer
    {
        get => (AffectedLayerOption)Component.GetEnum("affected_layer");
        set => Component.SetEnum("affected_layer", (uint)value);
    }
}

/// <summary>Campo de vento: Arrasto para velocidade local do ar, dependente da massa. Família Física 3D · Campos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_area3d.html</remarks>
public readonly struct WindField : IComponentFacade<WindField>
{
    public static string TypeId => "astra.physics.field.wind";
    public static WindField Wrap(Component component) => new(component);
    public Component Component { get; }
    public WindField(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Meias XYZ</summary>
    public Vector3 HalfExtents
    {
        get => new(Component.GetFloat("half_x"), Component.GetFloat("half_y"), Component.GetFloat("half_z"));
        set => Component.SetVector3("half_extents", value);
    }
    /// <summary>Centro local</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Vento local</summary>
    public Vector3 Vector
    {
        get => new(Component.GetFloat("vector_x"), Component.GetFloat("vector_y"), Component.GetFloat("vector_z"));
        set => Component.SetVector3("vector", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 10000.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Acoplamento (kg/s)</summary>
    /// <remarks>Faixa válida: 0 a 10000.</remarks>
    public float Coefficient
    {
        get => Component.GetFloat("coefficient");
        set => Component.SetFloat("coefficient", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Acordar corpos. Desligado: corpos em repouso permanecem dormindo</summary>
    public bool WakeBodies
    {
        get => Component.GetBool("wake_bodies");
        set => Component.SetBool("wake_bodies", value);
    }
    public enum ShapeOption : uint
    {
        Caixa = 0,
        Esfera = 1,
    }
    /// <summary>Forma. Escala e orientação são as do objeto e de seus pais</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    public enum FalloffOption : uint
    {
        Uniforme = 0,
        Linear = 1,
        Suave = 2,
    }
    /// <summary>Queda de influência. Centro vale 1; Linear e Suave chegam a zero na borda</summary>
    public FalloffOption Falloff
    {
        get => (FalloffOption)Component.GetEnum("falloff");
        set => Component.SetEnum("falloff", (uint)value);
    }
    public enum AffectedLayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada afetada. Todas ou uma camada física do projeto; não altera a matriz de colisão</summary>
    public AffectedLayerOption AffectedLayer
    {
        get => (AffectedLayerOption)Component.GetEnum("affected_layer");
        set => Component.SetEnum("affected_layer", (uint)value);
    }
}

/// <summary>Campo de arrasto: Amortecimento linear e angular local. Família Física 3D · Campos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_area3d.html</remarks>
public readonly struct DragField : IComponentFacade<DragField>
{
    public static string TypeId => "astra.physics.field.drag";
    public static DragField Wrap(Component component) => new(component);
    public Component Component { get; }
    public DragField(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Meias XYZ</summary>
    public Vector3 HalfExtents
    {
        get => new(Component.GetFloat("half_x"), Component.GetFloat("half_y"), Component.GetFloat("half_z"));
        set => Component.SetVector3("half_extents", value);
    }
    /// <summary>Centro local</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 10000.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Arrasto linear (1/s)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float LinearDrag
    {
        get => Component.GetFloat("linear_drag");
        set => Component.SetFloat("linear_drag", value);
    }
    /// <summary>Arrasto angular (1/s)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float AngularDrag
    {
        get => Component.GetFloat("angular_drag");
        set => Component.SetFloat("angular_drag", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Acordar corpos. Desligado: corpos em repouso permanecem dormindo</summary>
    public bool WakeBodies
    {
        get => Component.GetBool("wake_bodies");
        set => Component.SetBool("wake_bodies", value);
    }
    public enum ShapeOption : uint
    {
        Caixa = 0,
        Esfera = 1,
    }
    /// <summary>Forma. Escala e orientação são as do objeto e de seus pais</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    public enum FalloffOption : uint
    {
        Uniforme = 0,
        Linear = 1,
        Suave = 2,
    }
    /// <summary>Queda de influência. Centro vale 1; Linear e Suave chegam a zero na borda</summary>
    public FalloffOption Falloff
    {
        get => (FalloffOption)Component.GetEnum("falloff");
        set => Component.SetEnum("falloff", (uint)value);
    }
    public enum AffectedLayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada afetada. Todas ou uma camada física do projeto; não altera a matriz de colisão</summary>
    public AffectedLayerOption AffectedLayer
    {
        get => (AffectedLayerOption)Component.GetEnum("affected_layer");
        set => Component.SetEnum("affected_layer", (uint)value);
    }
}

/// <summary>Campo radial: Atração, repulsão e vórtice ao redor do centro. Família Física 3D · Campos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_area3d.html</remarks>
public readonly struct RadialField : IComponentFacade<RadialField>
{
    public static string TypeId => "astra.physics.field.radial";
    public static RadialField Wrap(Component component) => new(component);
    public Component Component { get; }
    public RadialField(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Meias XYZ</summary>
    public Vector3 HalfExtents
    {
        get => new(Component.GetFloat("half_x"), Component.GetFloat("half_y"), Component.GetFloat("half_z"));
        set => Component.SetVector3("half_extents", value);
    }
    /// <summary>Centro local</summary>
    public Vector3 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"), Component.GetFloat("offset_z"));
        set => Component.SetVector3("offset", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 10000.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Aceleração radial (m/s²)</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float Acceleration
    {
        get => Component.GetFloat("acceleration");
        set => Component.SetFloat("acceleration", value);
    }
    /// <summary>Aceleração tangencial (m/s²)</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float TangentialAcceleration
    {
        get => Component.GetFloat("tangential_acceleration");
        set => Component.SetFloat("tangential_acceleration", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Acordar corpos. Desligado: corpos em repouso permanecem dormindo</summary>
    public bool WakeBodies
    {
        get => Component.GetBool("wake_bodies");
        set => Component.SetBool("wake_bodies", value);
    }
    public enum ShapeOption : uint
    {
        Caixa = 0,
        Esfera = 1,
    }
    /// <summary>Forma. Escala e orientação são as do objeto e de seus pais</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    public enum FalloffOption : uint
    {
        Uniforme = 0,
        Linear = 1,
        Suave = 2,
    }
    /// <summary>Queda de influência. Centro vale 1; Linear e Suave chegam a zero na borda</summary>
    public FalloffOption Falloff
    {
        get => (FalloffOption)Component.GetEnum("falloff");
        set => Component.SetEnum("falloff", (uint)value);
    }
    public enum AffectedLayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada afetada. Todas ou uma camada física do projeto; não altera a matriz de colisão</summary>
    public AffectedLayerOption AffectedLayer
    {
        get => (AffectedLayerOption)Component.GetEnum("affected_layer");
        set => Component.SetEnum("affected_layer", (uint)value);
    }
}

/// <summary>Conexão física 3D: Evento de sensor/contato altera a ativação de um receptor. Família Física 3D · Eventos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_area3d.html</remarks>
public readonly struct PhysicsEventConnection3D : IComponentFacade<PhysicsEventConnection3D>
{
    public static string TypeId => "astra.physics.event_connection";
    public static PhysicsEventConnection3D Wrap(Component component) => new(component);
    public Component Component { get; }
    public PhysicsEventConnection3D(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Ativa</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum EventOption : uint
    {
        EntradaSensor = 0,
        PermSensor = 1,
        SaidaSensor = 2,
        EntradaContato = 3,
        PermContato = 4,
        SaidaContato = 5,
    }
    /// <summary>Evento. Sensor exige Sensor ligado no Corpo físico; contato sólido exige desligado</summary>
    public EventOption Event
    {
        get => (EventOption)Component.GetEnum("event");
        set => Component.SetEnum("event", (uint)value);
    }
    public enum ActionOption : uint
    {
        Desconectado = 0,
        AtivarObjeto = 1,
        DesativarObjeto = 2,
        AlternarObjeto = 3,
    }
    /// <summary>Ação. Executada antes do callback C#; permanência pode disparar a cada passo físico</summary>
    public ActionOption Action
    {
        get => (ActionOption)Component.GetEnum("action");
        set => Component.SetEnum("action", (uint)value);
    }
    /// <summary>Receptor. Objeto cuja ativação será alterada</summary>
    public ObjectReference Receiver
    {
        get => Component.GetReference("receiver");
        set => Component.SetReference("receiver", value);
    }
    /// <summary>Outro objeto. Opcional: somente eventos com este outro objeto; vazio aceita todos</summary>
    public ObjectReference OtherFilter
    {
        get => Component.GetReference("other_filter");
        set => Component.SetReference("other_filter", value);
    }
}

/// <summary>Força constante: Força e torque contínuos sobre corpo dinâmico. Família Física 3D · Forças.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-ConstantForce.html</remarks>
public readonly struct ConstantForce : IComponentFacade<ConstantForce>
{
    public static string TypeId => "astra.physics.constant_force";
    public static ConstantForce Wrap(Component component) => new(component);
    public Component Component { get; }
    public ConstantForce(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Força mundo</summary>
    public Vector3 Force
    {
        get => new(Component.GetFloat("force_x"), Component.GetFloat("force_y"), Component.GetFloat("force_z"));
        set => Component.SetVector3("force", value);
    }
    /// <summary>Força local</summary>
    public Vector3 RelativeForce
    {
        get => new(Component.GetFloat("relative_force_x"), Component.GetFloat("relative_force_y"), Component.GetFloat("relative_force_z"));
        set => Component.SetVector3("relative_force", value);
    }
    /// <summary>Torque mundo</summary>
    public Vector3 Torque
    {
        get => new(Component.GetFloat("torque_x"), Component.GetFloat("torque_y"), Component.GetFloat("torque_z"));
        set => Component.SetVector3("torque", value);
    }
    /// <summary>Torque local</summary>
    public Vector3 RelativeTorque
    {
        get => new(Component.GetFloat("relative_torque_x"), Component.GetFloat("relative_torque_y"), Component.GetFloat("relative_torque_z"));
        set => Component.SetVector3("relative_torque", value);
    }
    /// <summary>Ativo. Requer Corpo físico dinâmico no mesmo objeto; desligado permite configurar sem aplicar.</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Corpo físico: Massa e resposta física. Família Física 3D · Corpos.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-Rigidbody.html</remarks>
public readonly struct PhysicsBody : IComponentFacade<PhysicsBody>
{
    public static string TypeId => "astra.physics.body";
    public static PhysicsBody Wrap(Component component) => new(component);
    public Component Component { get; }
    public PhysicsBody(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Velocidade inicial</summary>
    public Vector3 Velocity
    {
        get => new(Component.GetFloat("velocity_x"), Component.GetFloat("velocity_y"), Component.GetFloat("velocity_z"));
        set => Component.SetVector3("velocity", value);
    }
    /// <summary>Giro inicial</summary>
    public Vector3 AngularVelocity
    {
        get => new(Component.GetFloat("angular_x"), Component.GetFloat("angular_y"), Component.GetFloat("angular_z"));
        set => Component.SetVector3("angular_velocity", value);
    }
    /// <summary>Centro de massa</summary>
    public Vector3 CenterOfMass
    {
        get => new(Component.GetFloat("center_of_mass_x"), Component.GetFloat("center_of_mass_y"), Component.GetFloat("center_of_mass_z"));
        set => Component.SetVector3("center_of_mass", value);
    }
    /// <summary>Inércia</summary>
    public Vector3 InertiaTensor
    {
        get => new(Component.GetFloat("inertia_x"), Component.GetFloat("inertia_y"), Component.GetFloat("inertia_z"));
        set => Component.SetVector3("inertia_tensor", value);
    }
    /// <summary>Massa kg (kg)</summary>
    /// <remarks>Faixa válida: 0.01 a 1000000.</remarks>
    public float Mass
    {
        get => Component.GetFloat("mass");
        set => Component.SetFloat("mass", value);
    }
    /// <summary>Atrito</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Friction
    {
        get => Component.GetFloat("friction");
        set => Component.SetFloat("friction", value);
    }
    /// <summary>Restituição</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Restitution
    {
        get => Component.GetFloat("restitution");
        set => Component.SetFloat("restitution", value);
    }
    /// <summary>Amortecimento linear</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float LinearDamping
    {
        get => Component.GetFloat("linear_damping");
        set => Component.SetFloat("linear_damping", value);
    }
    /// <summary>Amortecimento angular</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float AngularDamping
    {
        get => Component.GetFloat("angular_damping");
        set => Component.SetFloat("angular_damping", value);
    }
    /// <summary>Multiplicador da gravidade</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float GravityFactor
    {
        get => Component.GetFloat("gravity_factor");
        set => Component.SetFloat("gravity_factor", value);
    }
    /// <summary>Limite linear (m/s)</summary>
    /// <remarks>Faixa válida: 0.001 a 100000.</remarks>
    public float MaxLinearVelocity
    {
        get => Component.GetFloat("max_linear_velocity");
        set => Component.SetFloat("max_linear_velocity", value);
    }
    /// <summary>Limite angular (rad/s)</summary>
    /// <remarks>Faixa válida: 0.001 a 100000.</remarks>
    public float MaxAngularVelocity
    {
        get => Component.GetFloat("max_angular_velocity");
        set => Component.SetFloat("max_angular_velocity", value);
    }
    /// <summary>Iterações de velocidade. 0 usa as iterações do mundo; aumenta custo por ilha de contato</summary>
    /// <remarks>Faixa válida: 0 a 255.</remarks>
    public float SolverVelocitySteps
    {
        get => Component.GetFloat("solver_velocity_steps");
        set => Component.SetFloat("solver_velocity_steps", value);
    }
    /// <summary>Sensor sem resposta</summary>
    public bool Sensor
    {
        get => Component.GetBool("sensor");
        set => Component.SetBool("sensor", value);
    }
    /// <summary>Permitir repouso</summary>
    public bool AllowSleep
    {
        get => Component.GetBool("allow_sleep");
        set => Component.SetBool("allow_sleep", value);
    }
    /// <summary>Travar posição X. Eixos do mundo; bloquear todos exige corpo estático</summary>
    public bool FreezePositionX
    {
        get => Component.GetBool("freeze_position_x");
        set => Component.SetBool("freeze_position_x", value);
    }
    /// <summary>Travar posição Y. Eixos do mundo; bloquear todos exige corpo estático</summary>
    public bool FreezePositionY
    {
        get => Component.GetBool("freeze_position_y");
        set => Component.SetBool("freeze_position_y", value);
    }
    /// <summary>Travar posição Z. Eixos do mundo; bloquear todos exige corpo estático</summary>
    public bool FreezePositionZ
    {
        get => Component.GetBool("freeze_position_z");
        set => Component.SetBool("freeze_position_z", value);
    }
    /// <summary>Travar rotação X. Eixos do mundo; bloquear todos exige corpo estático</summary>
    public bool FreezeRotationX
    {
        get => Component.GetBool("freeze_rotation_x");
        set => Component.SetBool("freeze_rotation_x", value);
    }
    /// <summary>Travar rotação Y. Eixos do mundo; bloquear todos exige corpo estático</summary>
    public bool FreezeRotationY
    {
        get => Component.GetBool("freeze_rotation_y");
        set => Component.SetBool("freeze_rotation_y", value);
    }
    /// <summary>Travar rotação Z. Eixos do mundo; bloquear todos exige corpo estático</summary>
    public bool FreezeRotationZ
    {
        get => Component.GetBool("freeze_rotation_z");
        set => Component.SetBool("freeze_rotation_z", value);
    }
    /// <summary>Colisão contínua. Varredura linear Jolt; não detecta varredura apenas angular</summary>
    public bool ContinuousCollision
    {
        get => Component.GetBool("continuous_collision");
        set => Component.SetBool("continuous_collision", value);
    }
    /// <summary>Monitorar. Desligado, o sensor não publica entrada, permanência nem saída (Area3D.monitoring)</summary>
    public bool Monitoring
    {
        get => Component.GetBool("monitoring");
        set => Component.SetBool("monitoring", value);
    }
    /// <summary>Detectável por sensores. Desligado, sensores ignoram este corpo (Area3D.monitorable)</summary>
    public bool Monitorable
    {
        get => Component.GetBool("monitorable");
        set => Component.SetBool("monitorable", value);
    }
    /// <summary>Centro de massa automático. Ligado, o Jolt calcula o centro pela forma (Rigidbody.automaticCenterOfMass)</summary>
    public bool AutomaticCenterOfMass
    {
        get => Component.GetBool("automatic_center_of_mass");
        set => Component.SetBool("automatic_center_of_mass", value);
    }
    /// <summary>Inércia automática. Ligado, a inércia vem da forma e da massa (Rigidbody.automaticInertiaTensor)</summary>
    public bool AutomaticInertia
    {
        get => Component.GetBool("automatic_inertia");
        set => Component.SetBool("automatic_inertia", value);
    }
    public enum MotionOption : uint
    {
        Estatico = 0,
        Cinematico = 1,
        Dinamico = 2,
    }
    /// <summary>Movimento</summary>
    public MotionOption Motion
    {
        get => (MotionOption)Component.GetEnum("motion");
        set => Component.SetEnum("motion", (uint)value);
    }
    public enum FrictionCombineOption : uint
    {
        PadraoDoMotor = 0,
        Media = 1,
        Minimo = 2,
        Multiplicar = 3,
        Maximo = 4,
    }
    /// <summary>Combinar atrito. Num par, vale o modo de maior precedência: máximo &gt; multiplicar &gt; mínimo &gt; média. Padrão: média geométrica</summary>
    public FrictionCombineOption FrictionCombine
    {
        get => (FrictionCombineOption)Component.GetEnum("friction_combine");
        set => Component.SetEnum("friction_combine", (uint)value);
    }
    public enum RestitutionCombineOption : uint
    {
        PadraoDoMotor = 0,
        Media = 1,
        Minimo = 2,
        Multiplicar = 3,
        Maximo = 4,
    }
    /// <summary>Combinar restituição. Num par, vale o modo de maior precedência. Padrão: o maior valor</summary>
    public RestitutionCombineOption RestitutionCombine
    {
        get => (RestitutionCombineOption)Component.GetEnum("restitution_combine");
        set => Component.SetEnum("restitution_combine", (uint)value);
    }
    public enum SurfaceOption : uint
    {
        Padrao = 0,
        Concreto = 1,
        Madeira = 2,
        Metal = 3,
        Grama = 4,
        Terra = 5,
        Areia = 6,
        Agua = 7,
        Gelo = 8,
        Borracha = 9,
        Vidro = 10,
        Tecido = 11,
        Pedra = 12,
    }
    /// <summary>Superfície. Tipo de superfície lido por scripts no acerto e no contato (passos, faíscas, sons)</summary>
    public SurfaceOption Surface
    {
        get => (SurfaceOption)Component.GetEnum("surface");
        set => Component.SetEnum("surface", (uint)value);
    }
    public enum InterpolationOption : uint
    {
        Nenhuma = 0,
        Interpolar = 1,
        Extrapolar = 2,
    }
    /// <summary>Interpolação. Suaviza a pose desenhada entre passos de 60 Hz (Rigidbody.interpolation); a física não muda</summary>
    public InterpolationOption Interpolation
    {
        get => (InterpolationOption)Component.GetEnum("interpolation");
        set => Component.SetEnum("interpolation", (uint)value);
    }
    /// <summary>Material físico. Recurso do projeto por slot</summary>
    public AssetGuid GetMaterial(uint slot = 0) => Component.GetResource("material", slot);
    public void SetMaterial(AssetGuid value, uint slot = 0) => Component.SetResource("material", value, slot);
}

/// <summary>Personagem: Locomoção com cápsula. Família Física 3D · Corpos.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-CharacterController.html</remarks>
public readonly struct Character : IComponentFacade<Character>
{
    public static string TypeId => "astra.physics.character";
    public static Character Wrap(Component component) => new(component);
    public Component Component { get; }
    public Character(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Raio m (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 10.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Meia altura do cilindro m (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 10.</remarks>
    public float HalfHeight
    {
        get => Component.GetFloat("half_height");
        set => Component.SetFloat("half_height", value);
    }
    /// <summary>Altura dos olhos m (m)</summary>
    /// <remarks>Faixa válida: 0.02 a 20.</remarks>
    public float EyeHeight
    {
        get => Component.GetFloat("eye_height");
        set => Component.SetFloat("eye_height", value);
    }
    /// <summary>Velocidade m/s (m/s)</summary>
    /// <remarks>Faixa válida: 0.01 a 100.</remarks>
    public float Speed
    {
        get => Component.GetFloat("speed");
        set => Component.SetFloat("speed", value);
    }
    /// <summary>Inclinação máxima graus (°)</summary>
    /// <remarks>Faixa válida: 1 a 89.</remarks>
    public float SlopeDegrees
    {
        get => Component.GetFloat("slope_degrees");
        set => Component.SetFloat("slope_degrees", value);
    }
    /// <summary>Velocidade do salto m/s (m/s)</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float JumpSpeed
    {
        get => Component.GetFloat("jump_speed");
        set => Component.SetFloat("jump_speed", value);
    }
    /// <summary>Altura do degrau (m)</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float StepHeight
    {
        get => Component.GetFloat("step_height");
        set => Component.SetFloat("step_height", value);
    }
    /// <summary>Aderência ao chão (m)</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float FloorSnapLength
    {
        get => Component.GetFloat("floor_snap_length");
        set => Component.SetFloat("floor_snap_length", value);
    }
    /// <summary>Gravidade (m/s²)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float Gravity
    {
        get => Component.GetFloat("gravity");
        set => Component.SetFloat("gravity", value);
    }
    /// <summary>Prioridade UI. Maior prioridade vence; empate: IA &gt; Script &gt; Gamepad &gt; Teclado &gt; UI. Script/IA zero mantém posse neste quadro.</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ControlPriorityUi
    {
        get => Component.GetFloat("control_priority_ui");
        set => Component.SetFloat("control_priority_ui", value);
    }
    /// <summary>Prioridade Teclado / mouse. Maior prioridade vence; empate: IA &gt; Script &gt; Gamepad &gt; Teclado &gt; UI. Script/IA zero mantém posse neste quadro.</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ControlPriorityKeyboard
    {
        get => Component.GetFloat("control_priority_keyboard");
        set => Component.SetFloat("control_priority_keyboard", value);
    }
    /// <summary>Prioridade Gamepad. Maior prioridade vence; empate: IA &gt; Script &gt; Gamepad &gt; Teclado &gt; UI. Script/IA zero mantém posse neste quadro.</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ControlPriorityGamepad
    {
        get => Component.GetFloat("control_priority_gamepad");
        set => Component.SetFloat("control_priority_gamepad", value);
    }
    /// <summary>Prioridade Script. Maior prioridade vence; empate: IA &gt; Script &gt; Gamepad &gt; Teclado &gt; UI. Script/IA zero mantém posse neste quadro.</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ControlPriorityScript
    {
        get => Component.GetFloat("control_priority_script");
        set => Component.SetFloat("control_priority_script", value);
    }
    /// <summary>Prioridade IA. Maior prioridade vence; empate: IA &gt; Script &gt; Gamepad &gt; Teclado &gt; UI. Script/IA zero mantém posse neste quadro.</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ControlPriorityAi
    {
        get => Component.GetFloat("control_priority_ai");
        set => Component.SetFloat("control_priority_ai", value);
    }
    /// <summary>Aceleração (m/s²). Zero responde na hora (comportamento anterior); maior que zero acelera até a velocidade pedida.</summary>
    /// <remarks>Faixa válida: 0 a 200.</remarks>
    public float Acceleration
    {
        get => Component.GetFloat("acceleration");
        set => Component.SetFloat("acceleration", value);
    }
    /// <summary>Frenagem (m/s²). Zero para na hora; maior que zero desacelera ao soltar o comando ou mudar de direção.</summary>
    /// <remarks>Faixa válida: 0 a 200.</remarks>
    public float Braking
    {
        get => Component.GetFloat("braking");
        set => Component.SetFloat("braking", value);
    }
    /// <summary>Controle no ar. Fração da aceleração disponível sem apoio; zero conserva o impulso do salto.</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float AirControl
    {
        get => Component.GetFloat("air_control");
        set => Component.SetFloat("air_control", value);
    }
    /// <summary>Velocidade de nado. Fração da velocidade usada dentro da água.</summary>
    /// <remarks>Faixa válida: 0.05 a 1.</remarks>
    public float SwimSpeed
    {
        get => Component.GetFloat("swim_speed");
        set => Component.SetFloat("swim_speed", value);
    }
    /// <summary>Giro ao dirigir (°/s). Velocidade de giro no modo Dirigir; cresce com a velocidade e inverte de ré.</summary>
    /// <remarks>Faixa válida: 0 a 720.</remarks>
    public float TurnRate
    {
        get => Component.GetFloat("turn_rate");
        set => Component.SetFloat("turn_rate", value);
    }
    /// <summary>Impulso ao sair. Conserva X/Z da superfície no ar; desligar remove apenas esse impulso, sem recriar a cápsula.</summary>
    public bool InheritPlatformHorizontal
    {
        get => Component.GetBool("inherit_platform_horizontal");
        set => Component.SetBool("inherit_platform_horizontal", value);
    }
    public enum ControlSourceOption : uint
    {
        Automatico = 0,
        UI = 1,
        TecladoMouse = 2,
        Gamepad = 3,
        Script = 4,
        IA = 5,
    }
    /// <summary>Fonte de controle. Automático arbitra prioridades; fonte fixa dá posse exclusiva, sem converter a malha ou a colisão.</summary>
    public ControlSourceOption ControlSource
    {
        get => (ControlSourceOption)Component.GetEnum("control_source");
        set => Component.SetEnum("control_source", (uint)value);
    }
    public enum MovementModeOption : uint
    {
        Solo = 0,
        Voo = 1,
        NadoNaAgua = 2,
        Dirigir = 3,
    }
    /// <summary>Modo de movimento. Solo usa gravidade e apoio; Voo ignora a gravidade e usa o eixo vertical; Nado entra em vigor dentro de um volume de água da cena e volta ao solo fora dele; Dirigir transforma frente/trás em acelerador e lados em giro, sem deslizar de lado.</summary>
    public MovementModeOption MovementMode
    {
        get => (MovementModeOption)Component.GetEnum("movement_mode");
        set => Component.SetEnum("movement_mode", (uint)value);
    }
    /// <summary>Agachar. Troca para a cápsula baixa; levantar falha sem espaço acima</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool Crouch(bool crouching) => Component.Invoke("crouch", ComponentValue.Boolean(crouching)).AsBoolean();
    /// <summary>Subir/descer. Eixo vertical do voo e do nado neste quadro (-1 a 1)</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void SetVertical(double axis) => Component.Invoke("set_vertical", ComponentValue.Number(axis));
    /// <summary>Na água. Verdadeiro enquanto o nado está em vigor</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool InWater() => Component.Invoke("in_water").AsBoolean();
    /// <summary>Bateu num colisor. O movimento do personagem colidiu com um corpo; carrega o objeto, o ponto e a normal. Payload: Objeto tocado: objeto, Ponto: vetor, Normal: vetor</summary>
    public ComponentSubscription OnColliderHit(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "collider_hit", handler);
}

/// <summary>Colisor 3D: Volume de contato. Família Física 3D · Formas.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-BoxCollider.html</remarks>
public readonly struct Collider : IComponentFacade<Collider>
{
    public static string TypeId => "astra.physics.collider";
    public static Collider Wrap(Component component) => new(component);
    public Component Component { get; }
    public Collider(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Meia extensão</summary>
    public Vector3 HalfExtents
    {
        get => new(Component.GetFloat("half_x"), Component.GetFloat("half_y"), Component.GetFloat("half_z"));
        set => Component.SetVector3("half_extents", value);
    }
    /// <summary>Centro</summary>
    public Vector3 Center
    {
        get => new(Component.GetFloat("center_x"), Component.GetFloat("center_y"), Component.GetFloat("center_z"));
        set => Component.SetVector3("center", value);
    }
    /// <summary>Rotação</summary>
    public Vector3 Rotation
    {
        get => new(Component.GetFloat("rotation_x"), Component.GetFloat("rotation_y"), Component.GetFloat("rotation_z"));
        set => Component.SetVector3("rotation", value);
    }
    /// <summary>Raio</summary>
    /// <remarks>Faixa válida: 0.01 a 10000.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Meia altura cilíndrica</summary>
    /// <remarks>Faixa válida: 0.01 a 10000.</remarks>
    public float HalfHeight
    {
        get => Component.GetFloat("half_height");
        set => Component.SetFloat("half_height", value);
    }
    /// <summary>Tolerância do casco (u). Pontos até esta distância podem ficar fora do casco; valores maiores geram cascos mais simples.</summary>
    /// <remarks>Faixa válida: 0.00001 a 1.</remarks>
    public float HullTolerance
    {
        get => Component.GetFloat("hull_tolerance");
        set => Component.SetFloat("hull_tolerance", value);
    }
    /// <summary>Ângulo de aresta ativa (°). Separa arestas de contato em superfícies com mudança de normal acima deste ângulo.</summary>
    /// <remarks>Faixa válida: 0 a 90.</remarks>
    public float ActiveEdgeAngle
    {
        get => Component.GetFloat("active_edge_angle");
        set => Component.SetFloat("active_edge_angle", value);
    }
    /// <summary>Atrito. Usado por esta forma quando Material próprio está ligado</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Friction
    {
        get => Component.GetFloat("friction");
        set => Component.SetFloat("friction", value);
    }
    /// <summary>Restituição</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Restitution
    {
        get => Component.GetFloat("restitution");
        set => Component.SetFloat("restitution", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Convexo. Casco convexo da malha: aceita corpo dinâmico. Desligado usa os triângulos exatos e só vale em corpo estático ou cinemático.</summary>
    public bool Convex
    {
        get => Component.GetBool("convex");
        set => Component.SetBool("convex", value);
    }
    /// <summary>Soldar vértices iguais. Compartilha vértices coincidentes antes de criar a malha física, reduzindo costuras internas.</summary>
    public bool WeldVertices
    {
        get => Component.GetBool("weld_vertices");
        set => Component.SetBool("weld_vertices", value);
    }
    /// <summary>Otimizar para o jogo. Constrói uma árvore de busca mais eficiente; desligue para cozinhar mais rápido durante iterações.</summary>
    public bool OptimizeCooking
    {
        get => Component.GetBool("optimize_cooking");
        set => Component.SetBool("optimize_cooking", value);
    }
    /// <summary>Pose local da malha. Aplica centro e rotação locais à geometria física; não altera a malha visual. Desligado mantém a pose legada do objeto.</summary>
    public bool MeshLocalPose
    {
        get => Component.GetBool("mesh_local_pose");
        set => Component.SetBool("mesh_local_pose", value);
    }
    /// <summary>Material próprio. Esta forma usa atrito, restituição e superfície próprios em vez dos do corpo</summary>
    public bool OwnMaterial
    {
        get => Component.GetBool("own_material");
        set => Component.SetBool("own_material", value);
    }
    public enum ShapeOption : uint
    {
        Caixa = 0,
        Esfera = 1,
        Capsula = 2,
        Malha = 3,
        Cilindro = 4,
    }
    /// <summary>Forma</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    public enum FrictionCombineOption : uint
    {
        PadraoDoMotor = 0,
        Media = 1,
        Minimo = 2,
        Multiplicar = 3,
        Maximo = 4,
    }
    /// <summary>Combinar atrito</summary>
    public FrictionCombineOption FrictionCombine
    {
        get => (FrictionCombineOption)Component.GetEnum("friction_combine");
        set => Component.SetEnum("friction_combine", (uint)value);
    }
    public enum RestitutionCombineOption : uint
    {
        PadraoDoMotor = 0,
        Media = 1,
        Minimo = 2,
        Multiplicar = 3,
        Maximo = 4,
    }
    /// <summary>Combinar restituição</summary>
    public RestitutionCombineOption RestitutionCombine
    {
        get => (RestitutionCombineOption)Component.GetEnum("restitution_combine");
        set => Component.SetEnum("restitution_combine", (uint)value);
    }
    public enum SurfaceOption : uint
    {
        Padrao = 0,
        Concreto = 1,
        Madeira = 2,
        Metal = 3,
        Grama = 4,
        Terra = 5,
        Areia = 6,
        Agua = 7,
        Gelo = 8,
        Borracha = 9,
        Vidro = 10,
        Tecido = 11,
        Pedra = 12,
    }
    /// <summary>Superfície</summary>
    public SurfaceOption Surface
    {
        get => (SurfaceOption)Component.GetEnum("surface");
        set => Component.SetEnum("surface", (uint)value);
    }
    /// <summary>Corpo proprietário</summary>
    public ObjectReference Owner
    {
        get => Component.GetReference("owner");
        set => Component.SetReference("owner", value);
    }
    /// <summary>Malha de colisão. Recurso do projeto por slot</summary>
    public AssetGuid GetCollisionMesh(uint slot = 0) => Component.GetResource("collision_mesh", slot);
    public void SetCollisionMesh(AssetGuid value, uint slot = 0) => Component.SetResource("collision_mesh", value, slot);
    /// <summary>Material físico. Recurso do projeto por slot</summary>
    public AssetGuid GetMaterial(uint slot = 0) => Component.GetResource("material", slot);
    public void SetMaterial(AssetGuid value, uint slot = 0) => Component.SetResource("material", value, slot);
    /// <summary>Sensor: entrou. Outro corpo começou a sobrepor este sensor. Payload: Outro objeto: objeto</summary>
    public ComponentSubscription OnTriggerEnter(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "trigger_enter", handler);
    /// <summary>Sensor: saiu. Outro corpo deixou de sobrepor este sensor. Payload: Outro objeto: objeto</summary>
    public ComponentSubscription OnTriggerExit(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "trigger_exit", handler);
    /// <summary>Colisão: começou. Contato sólido começou; entregue aos dois objetos. Payload: Outro objeto: objeto</summary>
    public ComponentSubscription OnCollisionEnter(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "collision_enter", handler);
    /// <summary>Colisão: terminou. Contato sólido terminou; entregue aos dois objetos. Payload: Outro objeto: objeto</summary>
    public ComponentSubscription OnCollisionExit(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "collision_exit", handler);
    /// <summary>Sensor: dentro. Outro corpo continua sobrepondo este sensor; no máximo uma vez por quadro e par. Payload: Outro objeto: objeto</summary>
    public ComponentSubscription OnTriggerStay(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "trigger_stay", handler);
}

/// <summary>Junta: Nove mecanismos Jolt com limites, referenciais e motores. Família Física 3D · Juntas.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_generic6dofjoint3d.html</remarks>
public readonly struct Joint : IComponentFacade<Joint>
{
    public static string TypeId => "astra.physics.joint";
    public static Joint Wrap(Component component) => new(component);
    public Component Component { get; }
    public Joint(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Âncora A</summary>
    public Vector3 AnchorA
    {
        get => new(Component.GetFloat("anchor_a_x"), Component.GetFloat("anchor_a_y"), Component.GetFloat("anchor_a_z"));
        set => Component.SetVector3("anchor_a", value);
    }
    /// <summary>Âncora B</summary>
    public Vector3 AnchorB
    {
        get => new(Component.GetFloat("anchor_b_x"), Component.GetFloat("anchor_b_y"), Component.GetFloat("anchor_b_z"));
        set => Component.SetVector3("anchor_b", value);
    }
    /// <summary>Eixo A</summary>
    public Vector3 AxisA
    {
        get => new(Component.GetFloat("axis_a_x"), Component.GetFloat("axis_a_y"), Component.GetFloat("axis_a_z"));
        set => Component.SetVector3("axis_a", value);
    }
    /// <summary>Eixo B</summary>
    public Vector3 AxisB
    {
        get => new(Component.GetFloat("axis_b_x"), Component.GetFloat("axis_b_y"), Component.GetFloat("axis_b_z"));
        set => Component.SetVector3("axis_b", value);
    }
    /// <summary>Plano A</summary>
    public Vector3 NormalA
    {
        get => new(Component.GetFloat("normal_a_x"), Component.GetFloat("normal_a_y"), Component.GetFloat("normal_a_z"));
        set => Component.SetVector3("normal_a", value);
    }
    /// <summary>Plano B</summary>
    public Vector3 NormalB
    {
        get => new(Component.GetFloat("normal_b_x"), Component.GetFloat("normal_b_y"), Component.GetFloat("normal_b_z"));
        set => Component.SetVector3("normal_b", value);
    }
    /// <summary>Limite mínimo</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LimitMin
    {
        get => Component.GetFloat("limit_min");
        set => Component.SetFloat("limit_min", value);
    }
    /// <summary>Limite máximo</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LimitMax
    {
        get => Component.GetFloat("limit_max");
        set => Component.SetFloat("limit_max", value);
    }
    /// <summary>Velocidade do motor</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float MotorVelocity
    {
        get => Component.GetFloat("motor_velocity");
        set => Component.SetFloat("motor_velocity", value);
    }
    /// <summary>Alvo do motor</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float MotorPosition
    {
        get => Component.GetFloat("motor_position");
        set => Component.SetFloat("motor_position", value);
    }
    /// <summary>Força / torque máximo</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float MotorForce
    {
        get => Component.GetFloat("motor_force");
        set => Component.SetFloat("motor_force", value);
    }
    /// <summary>Frequência · Hz (Hz)</summary>
    /// <remarks>Faixa válida: 0.001 a 1000.</remarks>
    public float SpringFrequency
    {
        get => Component.GetFloat("spring_frequency");
        set => Component.SetFloat("spring_frequency", value);
    }
    /// <summary>Amortecimento da mola</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float SpringDamping
    {
        get => Component.GetFloat("spring_damping");
        set => Component.SetFloat("spring_damping", value);
    }
    /// <summary>Cone · semiângulo Y (°)</summary>
    /// <remarks>Faixa válida: 0 a 180.</remarks>
    public float SwingY
    {
        get => Component.GetFloat("swing_y");
        set => Component.SetFloat("swing_y", value);
    }
    /// <summary>Cone · semiângulo Z (°)</summary>
    /// <remarks>Faixa válida: 0 a 180.</remarks>
    public float SwingZ
    {
        get => Component.GetFloat("swing_z");
        set => Component.SetFloat("swing_z", value);
    }
    /// <summary>Torção mínima (°)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float TwistMin
    {
        get => Component.GetFloat("twist_min");
        set => Component.SetFloat("twist_min", value);
    }
    /// <summary>Torção máxima (°)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float TwistMax
    {
        get => Component.GetFloat("twist_max");
        set => Component.SetFloat("twist_max", value);
    }
    /// <summary>Limite mínimo (u)</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LinearXMinimum
    {
        get => Component.GetFloat("linear_x_minimum");
        set => Component.SetFloat("linear_x_minimum", value);
    }
    /// <summary>Limite máximo (u)</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LinearXMaximum
    {
        get => Component.GetFloat("linear_x_maximum");
        set => Component.SetFloat("linear_x_maximum", value);
    }
    /// <summary>Atrito máximo (N)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float LinearXFriction
    {
        get => Component.GetFloat("linear_x_friction");
        set => Component.SetFloat("linear_x_friction", value);
    }
    /// <summary>Velocidade alvo (u/s)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float LinearXVelocity
    {
        get => Component.GetFloat("linear_x_velocity");
        set => Component.SetFloat("linear_x_velocity", value);
    }
    /// <summary>Posição alvo (u)</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LinearXPosition
    {
        get => Component.GetFloat("linear_x_position");
        set => Component.SetFloat("linear_x_position", value);
    }
    /// <summary>Força máxima (N)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float LinearXForce
    {
        get => Component.GetFloat("linear_x_force");
        set => Component.SetFloat("linear_x_force", value);
    }
    /// <summary>Frequência (Hz)</summary>
    /// <remarks>Faixa válida: 0.001 a 1000.</remarks>
    public float LinearXFrequency
    {
        get => Component.GetFloat("linear_x_frequency");
        set => Component.SetFloat("linear_x_frequency", value);
    }
    /// <summary>Amortecimento</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float LinearXDamping
    {
        get => Component.GetFloat("linear_x_damping");
        set => Component.SetFloat("linear_x_damping", value);
    }
    /// <summary>Limite mínimo (u)</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LinearYMinimum
    {
        get => Component.GetFloat("linear_y_minimum");
        set => Component.SetFloat("linear_y_minimum", value);
    }
    /// <summary>Limite máximo (u)</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LinearYMaximum
    {
        get => Component.GetFloat("linear_y_maximum");
        set => Component.SetFloat("linear_y_maximum", value);
    }
    /// <summary>Atrito máximo (N)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float LinearYFriction
    {
        get => Component.GetFloat("linear_y_friction");
        set => Component.SetFloat("linear_y_friction", value);
    }
    /// <summary>Velocidade alvo (u/s)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float LinearYVelocity
    {
        get => Component.GetFloat("linear_y_velocity");
        set => Component.SetFloat("linear_y_velocity", value);
    }
    /// <summary>Posição alvo (u)</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LinearYPosition
    {
        get => Component.GetFloat("linear_y_position");
        set => Component.SetFloat("linear_y_position", value);
    }
    /// <summary>Força máxima (N)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float LinearYForce
    {
        get => Component.GetFloat("linear_y_force");
        set => Component.SetFloat("linear_y_force", value);
    }
    /// <summary>Frequência (Hz)</summary>
    /// <remarks>Faixa válida: 0.001 a 1000.</remarks>
    public float LinearYFrequency
    {
        get => Component.GetFloat("linear_y_frequency");
        set => Component.SetFloat("linear_y_frequency", value);
    }
    /// <summary>Amortecimento</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float LinearYDamping
    {
        get => Component.GetFloat("linear_y_damping");
        set => Component.SetFloat("linear_y_damping", value);
    }
    /// <summary>Limite mínimo (u)</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LinearZMinimum
    {
        get => Component.GetFloat("linear_z_minimum");
        set => Component.SetFloat("linear_z_minimum", value);
    }
    /// <summary>Limite máximo (u)</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LinearZMaximum
    {
        get => Component.GetFloat("linear_z_maximum");
        set => Component.SetFloat("linear_z_maximum", value);
    }
    /// <summary>Atrito máximo (N)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float LinearZFriction
    {
        get => Component.GetFloat("linear_z_friction");
        set => Component.SetFloat("linear_z_friction", value);
    }
    /// <summary>Velocidade alvo (u/s)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float LinearZVelocity
    {
        get => Component.GetFloat("linear_z_velocity");
        set => Component.SetFloat("linear_z_velocity", value);
    }
    /// <summary>Posição alvo (u)</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float LinearZPosition
    {
        get => Component.GetFloat("linear_z_position");
        set => Component.SetFloat("linear_z_position", value);
    }
    /// <summary>Força máxima (N)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float LinearZForce
    {
        get => Component.GetFloat("linear_z_force");
        set => Component.SetFloat("linear_z_force", value);
    }
    /// <summary>Frequência (Hz)</summary>
    /// <remarks>Faixa válida: 0.001 a 1000.</remarks>
    public float LinearZFrequency
    {
        get => Component.GetFloat("linear_z_frequency");
        set => Component.SetFloat("linear_z_frequency", value);
    }
    /// <summary>Amortecimento</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float LinearZDamping
    {
        get => Component.GetFloat("linear_z_damping");
        set => Component.SetFloat("linear_z_damping", value);
    }
    /// <summary>Limite mínimo (°)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float AngularXMinimum
    {
        get => Component.GetFloat("angular_x_minimum");
        set => Component.SetFloat("angular_x_minimum", value);
    }
    /// <summary>Limite máximo (°)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float AngularXMaximum
    {
        get => Component.GetFloat("angular_x_maximum");
        set => Component.SetFloat("angular_x_maximum", value);
    }
    /// <summary>Atrito máximo (Nm)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float AngularXFriction
    {
        get => Component.GetFloat("angular_x_friction");
        set => Component.SetFloat("angular_x_friction", value);
    }
    /// <summary>Velocidade alvo (°/s)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float AngularXVelocity
    {
        get => Component.GetFloat("angular_x_velocity");
        set => Component.SetFloat("angular_x_velocity", value);
    }
    /// <summary>Posição alvo (°)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float AngularXPosition
    {
        get => Component.GetFloat("angular_x_position");
        set => Component.SetFloat("angular_x_position", value);
    }
    /// <summary>Força máxima (Nm)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float AngularXForce
    {
        get => Component.GetFloat("angular_x_force");
        set => Component.SetFloat("angular_x_force", value);
    }
    /// <summary>Frequência (Hz)</summary>
    /// <remarks>Faixa válida: 0.001 a 1000.</remarks>
    public float AngularXFrequency
    {
        get => Component.GetFloat("angular_x_frequency");
        set => Component.SetFloat("angular_x_frequency", value);
    }
    /// <summary>Amortecimento</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float AngularXDamping
    {
        get => Component.GetFloat("angular_x_damping");
        set => Component.SetFloat("angular_x_damping", value);
    }
    /// <summary>Limite mínimo (°)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float AngularYMinimum
    {
        get => Component.GetFloat("angular_y_minimum");
        set => Component.SetFloat("angular_y_minimum", value);
    }
    /// <summary>Limite máximo (°)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float AngularYMaximum
    {
        get => Component.GetFloat("angular_y_maximum");
        set => Component.SetFloat("angular_y_maximum", value);
    }
    /// <summary>Atrito máximo (Nm)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float AngularYFriction
    {
        get => Component.GetFloat("angular_y_friction");
        set => Component.SetFloat("angular_y_friction", value);
    }
    /// <summary>Velocidade alvo (°/s)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float AngularYVelocity
    {
        get => Component.GetFloat("angular_y_velocity");
        set => Component.SetFloat("angular_y_velocity", value);
    }
    /// <summary>Posição alvo (°)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float AngularYPosition
    {
        get => Component.GetFloat("angular_y_position");
        set => Component.SetFloat("angular_y_position", value);
    }
    /// <summary>Força máxima (Nm)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float AngularYForce
    {
        get => Component.GetFloat("angular_y_force");
        set => Component.SetFloat("angular_y_force", value);
    }
    /// <summary>Frequência (Hz)</summary>
    /// <remarks>Faixa válida: 0.001 a 1000.</remarks>
    public float AngularYFrequency
    {
        get => Component.GetFloat("angular_y_frequency");
        set => Component.SetFloat("angular_y_frequency", value);
    }
    /// <summary>Amortecimento</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float AngularYDamping
    {
        get => Component.GetFloat("angular_y_damping");
        set => Component.SetFloat("angular_y_damping", value);
    }
    /// <summary>Limite mínimo (°)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float AngularZMinimum
    {
        get => Component.GetFloat("angular_z_minimum");
        set => Component.SetFloat("angular_z_minimum", value);
    }
    /// <summary>Limite máximo (°)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float AngularZMaximum
    {
        get => Component.GetFloat("angular_z_maximum");
        set => Component.SetFloat("angular_z_maximum", value);
    }
    /// <summary>Atrito máximo (Nm)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float AngularZFriction
    {
        get => Component.GetFloat("angular_z_friction");
        set => Component.SetFloat("angular_z_friction", value);
    }
    /// <summary>Velocidade alvo (°/s)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float AngularZVelocity
    {
        get => Component.GetFloat("angular_z_velocity");
        set => Component.SetFloat("angular_z_velocity", value);
    }
    /// <summary>Posição alvo (°)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float AngularZPosition
    {
        get => Component.GetFloat("angular_z_position");
        set => Component.SetFloat("angular_z_position", value);
    }
    /// <summary>Força máxima (Nm)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float AngularZForce
    {
        get => Component.GetFloat("angular_z_force");
        set => Component.SetFloat("angular_z_force", value);
    }
    /// <summary>Frequência (Hz)</summary>
    /// <remarks>Faixa válida: 0.001 a 1000.</remarks>
    public float AngularZFrequency
    {
        get => Component.GetFloat("angular_z_frequency");
        set => Component.SetFloat("angular_z_frequency", value);
    }
    /// <summary>Amortecimento</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float AngularZDamping
    {
        get => Component.GetFloat("angular_z_damping");
        set => Component.SetFloat("angular_z_damping", value);
    }
    /// <summary>Força de quebra (N). Zero: nunca quebra. Acima, a junta sai do solver no passo em que a força de restrição passa do limite</summary>
    /// <remarks>Faixa válida: 0 a 1000000000.</remarks>
    public float BreakForce
    {
        get => Component.GetFloat("break_force");
        set => Component.SetFloat("break_force", value);
    }
    /// <summary>Torque de quebra (N·m). Zero: nunca quebra por torque</summary>
    /// <remarks>Faixa válida: 0 a 1000000000.</remarks>
    public float BreakTorque
    {
        get => Component.GetFloat("break_torque");
        set => Component.SetFloat("break_torque", value);
    }
    /// <summary>Ativa</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum KindOption : uint
    {
        Ponto = 0,
        Dobradica = 1,
        Deslizante = 2,
        Distancia = 3,
        Fixa = 4,
        Cone = 5,
        SwingTwist = 6,
        Configuravel6DOF = 7,
        Mola = 8,
    }
    /// <summary>Tipo</summary>
    public KindOption Kind
    {
        get => (KindOption)Component.GetEnum("kind");
        set => Component.SetEnum("kind", (uint)value);
    }
    public enum MotorOption : uint
    {
        Desligado = 0,
        Velocidade = 1,
        Posicao = 2,
        PosicaoEVelocidade = 3,
    }
    /// <summary>Motor</summary>
    public MotorOption Motor
    {
        get => (MotorOption)Component.GetEnum("motor");
        set => Component.SetEnum("motor", (uint)value);
    }
    public enum LinearXMotionOption : uint
    {
        Travado = 0,
        Limitado = 1,
        Livre = 2,
    }
    /// <summary>Movimento</summary>
    public LinearXMotionOption LinearXMotion
    {
        get => (LinearXMotionOption)Component.GetEnum("linear_x_motion");
        set => Component.SetEnum("linear_x_motion", (uint)value);
    }
    public enum LinearXMotorOption : uint
    {
        Desligado = 0,
        Velocidade = 1,
        Posicao = 2,
        PosicaoEVelocidade = 3,
    }
    /// <summary>Motor</summary>
    public LinearXMotorOption LinearXMotor
    {
        get => (LinearXMotorOption)Component.GetEnum("linear_x_motor");
        set => Component.SetEnum("linear_x_motor", (uint)value);
    }
    public enum LinearYMotionOption : uint
    {
        Travado = 0,
        Limitado = 1,
        Livre = 2,
    }
    /// <summary>Movimento</summary>
    public LinearYMotionOption LinearYMotion
    {
        get => (LinearYMotionOption)Component.GetEnum("linear_y_motion");
        set => Component.SetEnum("linear_y_motion", (uint)value);
    }
    public enum LinearYMotorOption : uint
    {
        Desligado = 0,
        Velocidade = 1,
        Posicao = 2,
        PosicaoEVelocidade = 3,
    }
    /// <summary>Motor</summary>
    public LinearYMotorOption LinearYMotor
    {
        get => (LinearYMotorOption)Component.GetEnum("linear_y_motor");
        set => Component.SetEnum("linear_y_motor", (uint)value);
    }
    public enum LinearZMotionOption : uint
    {
        Travado = 0,
        Limitado = 1,
        Livre = 2,
    }
    /// <summary>Movimento</summary>
    public LinearZMotionOption LinearZMotion
    {
        get => (LinearZMotionOption)Component.GetEnum("linear_z_motion");
        set => Component.SetEnum("linear_z_motion", (uint)value);
    }
    public enum LinearZMotorOption : uint
    {
        Desligado = 0,
        Velocidade = 1,
        Posicao = 2,
        PosicaoEVelocidade = 3,
    }
    /// <summary>Motor</summary>
    public LinearZMotorOption LinearZMotor
    {
        get => (LinearZMotorOption)Component.GetEnum("linear_z_motor");
        set => Component.SetEnum("linear_z_motor", (uint)value);
    }
    public enum AngularXMotionOption : uint
    {
        Travado = 0,
        Limitado = 1,
        Livre = 2,
    }
    /// <summary>Movimento</summary>
    public AngularXMotionOption AngularXMotion
    {
        get => (AngularXMotionOption)Component.GetEnum("angular_x_motion");
        set => Component.SetEnum("angular_x_motion", (uint)value);
    }
    public enum AngularXMotorOption : uint
    {
        Desligado = 0,
        Velocidade = 1,
        Posicao = 2,
        PosicaoEVelocidade = 3,
    }
    /// <summary>Motor</summary>
    public AngularXMotorOption AngularXMotor
    {
        get => (AngularXMotorOption)Component.GetEnum("angular_x_motor");
        set => Component.SetEnum("angular_x_motor", (uint)value);
    }
    public enum AngularYMotionOption : uint
    {
        Travado = 0,
        Limitado = 1,
        Livre = 2,
    }
    /// <summary>Movimento</summary>
    public AngularYMotionOption AngularYMotion
    {
        get => (AngularYMotionOption)Component.GetEnum("angular_y_motion");
        set => Component.SetEnum("angular_y_motion", (uint)value);
    }
    public enum AngularYMotorOption : uint
    {
        Desligado = 0,
        Velocidade = 1,
        Posicao = 2,
        PosicaoEVelocidade = 3,
    }
    /// <summary>Motor</summary>
    public AngularYMotorOption AngularYMotor
    {
        get => (AngularYMotorOption)Component.GetEnum("angular_y_motor");
        set => Component.SetEnum("angular_y_motor", (uint)value);
    }
    public enum AngularZMotionOption : uint
    {
        Travado = 0,
        Limitado = 1,
        Livre = 2,
    }
    /// <summary>Movimento</summary>
    public AngularZMotionOption AngularZMotion
    {
        get => (AngularZMotionOption)Component.GetEnum("angular_z_motion");
        set => Component.SetEnum("angular_z_motion", (uint)value);
    }
    public enum AngularZMotorOption : uint
    {
        Desligado = 0,
        Velocidade = 1,
        Posicao = 2,
        PosicaoEVelocidade = 3,
    }
    /// <summary>Motor</summary>
    public AngularZMotorOption AngularZMotor
    {
        get => (AngularZMotorOption)Component.GetEnum("angular_z_motor");
        set => Component.SetEnum("angular_z_motor", (uint)value);
    }
    /// <summary>Conectar corpo</summary>
    public ObjectReference ConnectedBody
    {
        get => Component.GetReference("connected_body");
        set => Component.SetReference("connected_body", value);
    }
    /// <summary>Quebrou. Força ou torque passou do limite; a junta saiu do solver até o fim do Play (Unity OnJointBreak). Payload: Força ou torque: número</summary>
    public ComponentSubscription OnBroken(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "broken", handler);
}

/// <summary>Motor dinâmico: Locomoção por força e salto com apoio sobre Corpo físico dinâmico. Família Física 3D · Corpos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_rigidbody3d.html</remarks>
public readonly struct DynamicBodyMotor : IComponentFacade<DynamicBodyMotor>
{
    public static string TypeId => "astra.physics.dynamic_motor";
    public static DynamicBodyMotor Wrap(Component component) => new(component);
    public Component Component { get; }
    public DynamicBodyMotor(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Velocidade (m/s)</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float Speed
    {
        get => Component.GetFloat("speed");
        set => Component.SetFloat("speed", value);
    }
    /// <summary>Aceleração (m/s²)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float Acceleration
    {
        get => Component.GetFloat("acceleration");
        set => Component.SetFloat("acceleration", value);
    }
    /// <summary>Frenagem (m/s²)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float Braking
    {
        get => Component.GetFloat("braking");
        set => Component.SetFloat("braking", value);
    }
    /// <summary>Controle no ar</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float AirControl
    {
        get => Component.GetFloat("air_control");
        set => Component.SetFloat("air_control", value);
    }
    /// <summary>Velocidade do salto (m/s)</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float JumpSpeed
    {
        get => Component.GetFloat("jump_speed");
        set => Component.SetFloat("jump_speed", value);
    }
    /// <summary>Centro até os pés (m). Usado somente quando Apoio pela colisão está desligado.</summary>
    /// <remarks>Faixa válida: 0.01 a 100.</remarks>
    public float ProbeHeight
    {
        get => Component.GetFloat("probe_height");
        set => Component.SetFloat("probe_height", value);
    }
    /// <summary>Alcance abaixo dos pés (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 2.</remarks>
    public float ProbeDistance
    {
        get => Component.GetFloat("probe_distance");
        set => Component.SetFloat("probe_distance", value);
    }
    /// <summary>Raio de sondagem (m). Usado somente quando Apoio pela colisão está desligado.</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float SupportRadius
    {
        get => Component.GetFloat("support_radius");
        set => Component.SetFloat("support_radius", value);
    }
    /// <summary>Rampa máxima (°)</summary>
    /// <remarks>Faixa válida: 0 a 89.</remarks>
    public float MaxSlope
    {
        get => Component.GetFloat("max_slope");
        set => Component.SetFloat("max_slope", value);
    }
    /// <summary>Prioridade UI. Maior prioridade vence; empate: IA &gt; Script &gt; Gamepad &gt; Teclado &gt; UI. Script/IA zero mantém posse neste quadro.</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ControlPriorityUi
    {
        get => Component.GetFloat("control_priority_ui");
        set => Component.SetFloat("control_priority_ui", value);
    }
    /// <summary>Prioridade Teclado / mouse. Maior prioridade vence; empate: IA &gt; Script &gt; Gamepad &gt; Teclado &gt; UI. Script/IA zero mantém posse neste quadro.</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ControlPriorityKeyboard
    {
        get => Component.GetFloat("control_priority_keyboard");
        set => Component.SetFloat("control_priority_keyboard", value);
    }
    /// <summary>Prioridade Gamepad. Maior prioridade vence; empate: IA &gt; Script &gt; Gamepad &gt; Teclado &gt; UI. Script/IA zero mantém posse neste quadro.</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ControlPriorityGamepad
    {
        get => Component.GetFloat("control_priority_gamepad");
        set => Component.SetFloat("control_priority_gamepad", value);
    }
    /// <summary>Prioridade Script. Maior prioridade vence; empate: IA &gt; Script &gt; Gamepad &gt; Teclado &gt; UI. Script/IA zero mantém posse neste quadro.</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ControlPriorityScript
    {
        get => Component.GetFloat("control_priority_script");
        set => Component.SetFloat("control_priority_script", value);
    }
    /// <summary>Prioridade IA. Maior prioridade vence; empate: IA &gt; Script &gt; Gamepad &gt; Teclado &gt; UI. Script/IA zero mantém posse neste quadro.</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ControlPriorityAi
    {
        get => Component.GetFloat("control_priority_ai");
        set => Component.SetFloat("control_priority_ai", value);
    }
    /// <summary>Habilitado</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Acompanhar plataforma</summary>
    public bool InheritPlatformVelocity
    {
        get => Component.GetBool("inherit_platform_velocity");
        set => Component.SetBool("inherit_platform_velocity", value);
    }
    /// <summary>Apoio pela colisão. Sonda a superfície real do colisor do solver, com escala, rotação e partes compostas. Cinco amostras; alcance abaixo da forma em metros.</summary>
    public bool AutomaticSupport
    {
        get => Component.GetBool("automatic_support");
        set => Component.SetBool("automatic_support", value);
    }
    public enum ControlSourceOption : uint
    {
        Automatico = 0,
        UI = 1,
        TecladoMouse = 2,
        Gamepad = 3,
        Script = 4,
        IA = 5,
    }
    /// <summary>Fonte de controle. Automático arbitra prioridades; fonte fixa dá posse exclusiva, sem converter a malha ou a colisão.</summary>
    public ControlSourceOption ControlSource
    {
        get => (ControlSourceOption)Component.GetEnum("control_source");
        set => Component.SetEnum("control_source", (uint)value);
    }
}

/// <summary>Raio: Consulta um raio a cada quadro e guarda o primeiro acerto. Família Física 3D · Consultas.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_raycast3d.html</remarks>
public readonly struct RayCast : IComponentFacade<RayCast>
{
    public static string TypeId => "astra.physics.raycast";
    public static RayCast Wrap(Component component) => new(component);
    public Component Component { get; }
    public RayCast(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Alvo local</summary>
    public Vector3 Target
    {
        get => new(Component.GetFloat("target_x"), Component.GetFloat("target_y"), Component.GetFloat("target_z"));
        set => Component.SetVector3("target", value);
    }
    /// <summary>Ativo. Desligado não consulta e mantém o último resultado como vazio</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Ignorar o próprio corpo. Ignora o corpo deste objeto ou do ancestral mais próximo com Corpo físico</summary>
    public bool ExcludeSelf
    {
        get => Component.GetBool("exclude_self");
        set => Component.SetBool("exclude_self", value);
    }
    /// <summary>Incluir sensores. Sensores ficam de fora por padrão (collide_with_areas do Godot)</summary>
    public bool IncludeSensors
    {
        get => Component.GetBool("include_sensors");
        set => Component.SetBool("include_sensors", value);
    }
    /// <summary>Incluir estáticos</summary>
    public bool IncludeStatic
    {
        get => Component.GetBool("include_static");
        set => Component.SetBool("include_static", value);
    }
    /// <summary>Incluir dinâmicos</summary>
    public bool IncludeDynamic
    {
        get => Component.GetBool("include_dynamic");
        set => Component.SetBool("include_dynamic", value);
    }
    public enum LayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada. Todas ou só uma camada física do projeto</summary>
    public LayerOption Layer
    {
        get => (LayerOption)Component.GetEnum("layer");
        set => Component.SetEnum("layer", (uint)value);
    }
    /// <summary>Acertando. Verdadeiro quando a última consulta encontrou algo</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool Colliding() => Component.Invoke("colliding").AsBoolean();
    /// <summary>Objeto atingido. Dono do corpo atingido; vazio sem acerto</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public ObjectReference Collider() => Component.Reference(Component.Invoke("collider"));
    /// <summary>Ponto. Ponto do acerto no mundo</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public Vector3 Point() => Component.Invoke("point").AsVector3();
    /// <summary>Normal. Normal da superfície atingida</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public Vector3 Normal() => Component.Invoke("normal").AsVector3();
    /// <summary>Distância. Distância do acerto ao início, em metros</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double Distance() => Component.Invoke("distance").AsNumber();
    /// <summary>Atualizar agora. Refaz a consulta neste instante (force_raycast_update do Godot)</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Update() => Component.Invoke("update");
}

/// <summary>Varredura de forma: Varre uma forma até o alvo e guarda o primeiro acerto. Família Física 3D · Consultas.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_shapecast3d.html</remarks>
public readonly struct ShapeCast : IComponentFacade<ShapeCast>
{
    public static string TypeId => "astra.physics.shapecast";
    public static ShapeCast Wrap(Component component) => new(component);
    public Component Component { get; }
    public ShapeCast(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Meia extensão</summary>
    public Vector3 HalfExtents
    {
        get => new(Component.GetFloat("half_x"), Component.GetFloat("half_y"), Component.GetFloat("half_z"));
        set => Component.SetVector3("half_extents", value);
    }
    /// <summary>Alvo local</summary>
    public Vector3 Target
    {
        get => new(Component.GetFloat("target_x"), Component.GetFloat("target_y"), Component.GetFloat("target_z"));
        set => Component.SetVector3("target", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 1000.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Meia altura (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 1000.</remarks>
    public float HalfHeight
    {
        get => Component.GetFloat("half_height");
        set => Component.SetFloat("half_height", value);
    }
    /// <summary>Ativo. Desligado não consulta e mantém o último resultado como vazio</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Ignorar o próprio corpo. Ignora o corpo deste objeto ou do ancestral mais próximo com Corpo físico</summary>
    public bool ExcludeSelf
    {
        get => Component.GetBool("exclude_self");
        set => Component.SetBool("exclude_self", value);
    }
    /// <summary>Incluir sensores. Sensores ficam de fora por padrão (collide_with_areas do Godot)</summary>
    public bool IncludeSensors
    {
        get => Component.GetBool("include_sensors");
        set => Component.SetBool("include_sensors", value);
    }
    /// <summary>Incluir estáticos</summary>
    public bool IncludeStatic
    {
        get => Component.GetBool("include_static");
        set => Component.SetBool("include_static", value);
    }
    /// <summary>Incluir dinâmicos</summary>
    public bool IncludeDynamic
    {
        get => Component.GetBool("include_dynamic");
        set => Component.SetBool("include_dynamic", value);
    }
    public enum LayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada. Todas ou só uma camada física do projeto</summary>
    public LayerOption Layer
    {
        get => (LayerOption)Component.GetEnum("layer");
        set => Component.SetEnum("layer", (uint)value);
    }
    public enum ShapeOption : uint
    {
        Esfera = 0,
        Caixa = 1,
        Capsula = 2,
        Cilindro = 3,
    }
    /// <summary>Forma</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    /// <summary>Acertando. Verdadeiro quando a última consulta encontrou algo</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool Colliding() => Component.Invoke("colliding").AsBoolean();
    /// <summary>Objeto atingido. Dono do corpo atingido; vazio sem acerto</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public ObjectReference Collider() => Component.Reference(Component.Invoke("collider"));
    /// <summary>Ponto. Ponto do acerto no mundo</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public Vector3 Point() => Component.Invoke("point").AsVector3();
    /// <summary>Normal. Normal da superfície atingida</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public Vector3 Normal() => Component.Invoke("normal").AsVector3();
    /// <summary>Distância. Distância do acerto ao início, em metros</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double Distance() => Component.Invoke("distance").AsNumber();
    /// <summary>Atualizar agora. Refaz a consulta neste instante (force_raycast_update do Godot)</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Update() => Component.Invoke("update");
}

/// <summary>Braço de mola: Afasta os filhos até a primeira colisão, como câmera em terceira pessoa. Família Física 3D · Consultas.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_springarm3d.html</remarks>
public readonly struct SpringArm : IComponentFacade<SpringArm>
{
    public static string TypeId => "astra.physics.spring_arm";
    public static SpringArm Wrap(Component component) => new(component);
    public Component Component { get; }
    public SpringArm(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Comprimento (m). Distância livre máxima ao longo de +Z local</summary>
    /// <remarks>Faixa válida: 0.001 a 1000.</remarks>
    public float Length
    {
        get => Component.GetFloat("length");
        set => Component.SetFloat("length", value);
    }
    /// <summary>Margem (m). Folga entre o filho e a superfície atingida</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float Margin
    {
        get => Component.GetFloat("margin");
        set => Component.SetFloat("margin", value);
    }
    /// <summary>Raio da esfera (m). Zero usa raio de luz; maior varre uma esfera (evita a câmera atravessar quinas)</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Ativo. Desligado não consulta e mantém o último resultado como vazio</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Ignorar o próprio corpo. Ignora o corpo deste objeto ou do ancestral mais próximo com Corpo físico</summary>
    public bool ExcludeSelf
    {
        get => Component.GetBool("exclude_self");
        set => Component.SetBool("exclude_self", value);
    }
    /// <summary>Incluir sensores. Sensores ficam de fora por padrão (collide_with_areas do Godot)</summary>
    public bool IncludeSensors
    {
        get => Component.GetBool("include_sensors");
        set => Component.SetBool("include_sensors", value);
    }
    /// <summary>Incluir estáticos</summary>
    public bool IncludeStatic
    {
        get => Component.GetBool("include_static");
        set => Component.SetBool("include_static", value);
    }
    /// <summary>Incluir dinâmicos</summary>
    public bool IncludeDynamic
    {
        get => Component.GetBool("include_dynamic");
        set => Component.SetBool("include_dynamic", value);
    }
    public enum LayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada. Todas ou só uma camada física do projeto</summary>
    public LayerOption Layer
    {
        get => (LayerOption)Component.GetEnum("layer");
        set => Component.SetEnum("layer", (uint)value);
    }
    /// <summary>Comprimento atual. Distância livre do último quadro, já descontada a margem</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double HitLength() => Component.Invoke("hit_length").AsNumber();
}

/// <summary>Animação: Clipes tocados e misturados no Play. Família Animação · Clipes.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-Animation.html</remarks>
public readonly struct Animation : IComponentFacade<Animation>
{
    public static string TypeId => "astra.animation";
    public static Animation Wrap(Component component) => new(component);
    public Component Component { get; }
    public Animation(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Velocidade (x). Velocidade inicial de cada clipe (AnimationState.speed); negativo toca de trás para frente</summary>
    /// <remarks>Faixa válida: -10 a 10.</remarks>
    public float Speed
    {
        get => Component.GetFloat("speed");
        set => Component.SetFloat("speed", value);
    }
    /// <summary>Ativa. Desligar suspende tempo e avaliação; religar retoma os estados</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Tocar ao iniciar. Play Automatically: o clipe padrão começa a tocar quando o Play inicia</summary>
    public bool PlayAutomatically
    {
        get => Component.GetBool("play_automatically");
        set => Component.SetBool("play_automatically", value);
    }
    public enum WrapModeOption : uint
    {
        UmaVez = 0,
        Repetir = 1,
        VaiEVolta = 2,
        SegurarNoFim = 3,
    }
    /// <summary>Repetição. Wrap Mode padrão dos clipes: o que acontece depois do fim</summary>
    public WrapModeOption WrapMode
    {
        get => (WrapModeOption)Component.GetEnum("wrap_mode");
        set => Component.SetEnum("wrap_mode", (uint)value);
    }
    public enum ClipCountOption : uint
    {
        V0 = 0,
        V1 = 1,
        V2 = 2,
        V3 = 3,
        V4 = 4,
        V5 = 5,
        V6 = 6,
        V7 = 7,
        V8 = 8,
        V9 = 9,
        V10 = 10,
        V11 = 11,
        V12 = 12,
        V13 = 13,
        V14 = 14,
        V15 = 15,
        V16 = 16,
        V17 = 17,
        V18 = 18,
        V19 = 19,
        V20 = 20,
        V21 = 21,
        V22 = 22,
        V23 = 23,
        V24 = 24,
        V25 = 25,
        V26 = 26,
        V27 = 27,
        V28 = 28,
        V29 = 29,
        V30 = 30,
        V31 = 31,
        V32 = 32,
    }
    /// <summary>Quantidade de clipes</summary>
    public ClipCountOption ClipCount
    {
        get => (ClipCountOption)Component.GetEnum("clip_count");
        set => Component.SetEnum("clip_count", (uint)value);
    }
    /// <summary>Clipe padrão. Recurso do projeto por slot</summary>
    public AssetGuid GetClip(uint slot = 0) => Component.GetResource("clip", slot);
    public void SetClip(AssetGuid value, uint slot = 0) => Component.SetResource("clip", value, slot);
    /// <summary>Clipe. Recurso do projeto por slot</summary>
    public AssetGuid GetClips(uint slot = 0) => Component.GetResource("clips", slot);
    public void SetClips(AssetGuid value, uint slot = 0) => Component.SetResource("clips", value, slot);
    /// <summary>Evento do clipe. Código, valor e identidade local do evento; somente travessia em Play. Payload: Código: inteiro, Valor: número, ID no clipe: inteiro</summary>
    public ComponentSubscription OnClipEvent(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "clip_event", handler);
    /// <summary>Limite de eventos. Travessia excedeu o orçamento; não são repetidos no próximo quadro. Payload: Eventos suprimidos: inteiro</summary>
    public ComponentSubscription OnClipEventsLost(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "clip_events_lost", handler);
}

/// <summary>Animator: Máquina de estados: parâmetros, transições, misturas e camadas. Família Animação · Máquina de estados.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-AnimatorController.html</remarks>
public readonly struct Animator : IComponentFacade<Animator>
{
    public static string TypeId => "astra.animation.animator";
    public static Animator Wrap(Component component) => new(component);
    public Component Component { get; }
    public Animator(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Velocidade (×). Multiplica o tempo de todos os estados</summary>
    /// <remarks>Faixa válida: -10 a 10.</remarks>
    public float Speed
    {
        get => Component.GetFloat("speed");
        set => Component.SetFloat("speed", value);
    }
    /// <summary>Ativo. Desligado congela a pose atual; parâmetros e estados são preservados</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Ignorar escala de tempo. Anima em tempo real mesmo com o jogo pausado por escala (menus, cutscenes)</summary>
    public bool UnscaledTime
    {
        get => Component.GetBool("unscaled_time");
        set => Component.SetBool("unscaled_time", value);
    }
    /// <summary>Raiz animada. Objeto cuja hierarquia os clipes animam (o modelo importado)</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
    /// <summary>Corpo / motor. Fonte dos parâmetros físicos; independe da malha e não move o corpo</summary>
    public ObjectReference MotionSource
    {
        get => Component.GetReference("motion_source");
        set => Component.SetReference("motion_source", value);
    }
    /// <summary>Máscara · Base. Máscara da camada por instância; remapeada em hierarquia/prefab</summary>
    public ObjectReference LayerMask0
    {
        get => Component.GetReference("layer_mask_0");
        set => Component.SetReference("layer_mask_0", value);
    }
    /// <summary>Máscara · Camada 2. Máscara da camada por instância; remapeada em hierarquia/prefab</summary>
    public ObjectReference LayerMask1
    {
        get => Component.GetReference("layer_mask_1");
        set => Component.SetReference("layer_mask_1", value);
    }
    /// <summary>Máscara · Camada 3. Máscara da camada por instância; remapeada em hierarquia/prefab</summary>
    public ObjectReference LayerMask2
    {
        get => Component.GetReference("layer_mask_2");
        set => Component.SetReference("layer_mask_2", value);
    }
    /// <summary>Máscara · Camada 4. Máscara da camada por instância; remapeada em hierarquia/prefab</summary>
    public ObjectReference LayerMask3
    {
        get => Component.GetReference("layer_mask_3");
        set => Component.SetReference("layer_mask_3", value);
    }
    /// <summary>Controller. Recurso do projeto por slot</summary>
    public AssetGuid GetController(uint slot = 0) => Component.GetResource("controller", slot);
    public void SetController(AssetGuid value, uint slot = 0) => Component.SetResource("controller", value, slot);
    /// <summary>Em transição. Verdadeiro enquanto a camada base mistura dois estados</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool InTransition() => Component.Invoke("in_transition").AsBoolean();
    /// <summary>Ler peso. Peso efetivo da instância</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double GetLayerWeight(long layer) => Component.Invoke("get_layer_weight", ComponentValue.Integer(layer)).AsNumber();
    /// <summary>Peso da instância. Não altera o recurso compartilhado</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void SetLayerWeight(long layer, double value) => Component.Invoke("set_layer_weight", ComponentValue.Integer(layer), ComponentValue.Number(value));
    /// <summary>Ler composição. 0 Override / 1 Additive</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public long GetLayerBlend(long layer) => Component.Invoke("get_layer_blend", ComponentValue.Integer(layer)).AsInteger();
    /// <summary>Composição da instância. 0 Override / 1 Additive</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void SetLayerBlend(long layer, long mode) => Component.Invoke("set_layer_blend", ComponentValue.Integer(layer), ComponentValue.Integer(mode));
    /// <summary>Ler tempo de referência. Segundos no clipe</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double GetLayerReferenceTime(long layer) => Component.Invoke("get_layer_reference_time", ComponentValue.Integer(layer)).AsNumber();
    /// <summary>Tempo de referência. Segundos no clipe</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void SetLayerReferenceTime(long layer, double value) => Component.Invoke("set_layer_reference_time", ComponentValue.Integer(layer), ComponentValue.Number(value));
    /// <summary>Referência da instância. GUID zero usa a pose inicial; desconhecido é recusado</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void SetLayerReference(long layer, long high, long low) => Component.Invoke("set_layer_reference", ComponentValue.Integer(layer), ComponentValue.Integer(high), ComponentValue.Integer(low));
    /// <summary>GUID alto. Bits da referência efetiva</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public long GetLayerReferenceHigh(long layer) => Component.Invoke("get_layer_reference_high", ComponentValue.Integer(layer)).AsInteger();
    /// <summary>GUID baixo. Bits da referência efetiva</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public long GetLayerReferenceLow(long layer) => Component.Invoke("get_layer_reference_low", ComponentValue.Integer(layer)).AsInteger();
    /// <summary>Restaurar camada. Restaura a composição autorada, preservando estado e relógio</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void ResetLayerOverrides(long layer) => Component.Invoke("reset_layer_overrides", ComponentValue.Integer(layer));
    /// <summary>Entrou no estado. Emitido quando um estado começa (no início da transição para ele). Payload: Camada: inteiro, Estado: inteiro</summary>
    public ComponentSubscription OnStateEntered(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "state_entered", handler);
    /// <summary>Evento do estado. Emitido quando o tempo do estado passa por um evento marcado nele. Payload: Camada: inteiro, Estado: inteiro, Marca: inteiro</summary>
    public ComponentSubscription OnStateEvent(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "state_event", handler);
    /// <summary>Entrou no grupo. Um grupo tornou-se ativo; da raiz até o grupo interno. Payload: Camada: inteiro, Grupo: inteiro</summary>
    public ComponentSubscription OnMachineEntered(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "machine_entered", handler);
    /// <summary>Saiu do grupo. Um grupo deixou de participar da reprodução; do grupo interno à raiz. Payload: Camada: inteiro, Grupo: inteiro</summary>
    public ComponentSubscription OnMachineExited(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "machine_exited", handler);
    /// <summary>Mistura interrompida. Pose composta preservada; ID zero indica CrossFade solicitado pela API. Payload: Camada: inteiro, Transição cancelada: inteiro, Novo estado: inteiro</summary>
    public ComponentSubscription OnTransitionInterrupted(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "transition_interrupted", handler);
    /// <summary>Evento do clipe. Código, valor e identidade local do evento; somente travessia em Play. Payload: Código: inteiro, Valor: número, ID no clipe: inteiro</summary>
    public ComponentSubscription OnClipEvent(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "clip_event", handler);
    /// <summary>Limite de eventos. Travessia excedeu o orçamento; não são repetidos no próximo quadro. Payload: Eventos suprimidos: inteiro</summary>
    public ComponentSubscription OnClipEventsLost(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "clip_events_lost", handler);
}

/// <summary>Root motion: O passo do osso raiz anima o personagem; o osso fica no lugar. Família Animação · Movimento.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/RootMotion.html</remarks>
public readonly struct RootMotion : IComponentFacade<RootMotion>
{
    public static string TypeId => "astra.animation.root_motion";
    public static RootMotion Wrap(Component component) => new(component);
    public Component Component { get; }
    public RootMotion(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Peso</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Weight
    {
        get => Component.GetFloat("weight");
        set => Component.SetFloat("weight", value);
    }
    /// <summary>Salto de laço (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 10.</remarks>
    public float LoopJump
    {
        get => Component.GetFloat("loop_jump");
        set => Component.SetFloat("loop_jump", value);
    }
    /// <summary>Giro de laço (graus)</summary>
    /// <remarks>Faixa válida: 1 a 180.</remarks>
    public float LoopTurn
    {
        get => Component.GetFloat("loop_turn");
        set => Component.SetFloat("loop_turn", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Andar no plano</summary>
    public bool Horizontal
    {
        get => Component.GetBool("horizontal");
        set => Component.SetBool("horizontal", value);
    }
    /// <summary>Subir e descer</summary>
    public bool Vertical
    {
        get => Component.GetBool("vertical");
        set => Component.SetBool("vertical", value);
    }
    /// <summary>Girar</summary>
    public bool Rotation
    {
        get => Component.GetBool("rotation");
        set => Component.SetBool("rotation", value);
    }
    /// <summary>Osso fica no lugar</summary>
    public bool KeepBoneInPlace
    {
        get => Component.GetBool("keep_bone_in_place");
        set => Component.SetBool("keep_bone_in_place", value);
    }
    public enum AxisOption : uint
    {
        X = 0,
        Y = 1,
        Z = 2,
        X3 = 3,
        Y4 = 4,
        Z5 = 5,
    }
    /// <summary>Frente do osso</summary>
    public AxisOption Axis
    {
        get => (AxisOption)Component.GetEnum("axis");
        set => Component.SetEnum("axis", (uint)value);
    }
    /// <summary>Osso raiz</summary>
    public ObjectReference Bone
    {
        get => Component.GetReference("bone");
        set => Component.SetReference("bone", value);
    }
}

/// <summary>Campo de gravidade 2D: Gravidade XY em área sobre corpos dinâmicos 2D. Família Física 2D · Campos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_area2d.html</remarks>
public readonly struct GravityField2D : IComponentFacade<GravityField2D>
{
    public static string TypeId => "astra.physics2d.field.gravity";
    public static GravityField2D Wrap(Component component) => new(component);
    public Component Component { get; }
    public GravityField2D(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Meias XY</summary>
    public Vector2 HalfExtents
    {
        get => new(Component.GetFloat("half_x"), Component.GetFloat("half_y"));
        set => Component.SetVector2("half_extents", value);
    }
    /// <summary>Centro local</summary>
    public Vector2 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"));
        set => Component.SetVector2("offset", value);
    }
    /// <summary>Gravidade local</summary>
    public Vector2 Vector
    {
        get => new(Component.GetFloat("vector_x"), Component.GetFloat("vector_y"));
        set => Component.SetVector2("vector", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 10000.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Acordar corpos. Desligado: corpos em repouso permanecem dormindo</summary>
    public bool WakeBodies
    {
        get => Component.GetBool("wake_bodies");
        set => Component.SetBool("wake_bodies", value);
    }
    /// <summary>Substituir gravidade do mundo. Sobrepostos somam vetores e cancelam a gravidade padrão uma única vez</summary>
    public bool ReplaceWorldGravity
    {
        get => Component.GetBool("replace_world_gravity");
        set => Component.SetBool("replace_world_gravity", value);
    }
    public enum ShapeOption : uint
    {
        Retangulo = 0,
        Circulo = 1,
    }
    /// <summary>Forma. Escala e orientação são as do objeto e de seus pais</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    public enum FalloffOption : uint
    {
        Uniforme = 0,
        Linear = 1,
        Suave = 2,
    }
    /// <summary>Queda de influência. Centro vale 1; Linear e Suave chegam a zero na borda</summary>
    public FalloffOption Falloff
    {
        get => (FalloffOption)Component.GetEnum("falloff");
        set => Component.SetEnum("falloff", (uint)value);
    }
    public enum AffectedLayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada afetada. Todas ou uma camada física do projeto; não altera a matriz de colisão</summary>
    public AffectedLayerOption AffectedLayer
    {
        get => (AffectedLayerOption)Component.GetEnum("affected_layer");
        set => Component.SetEnum("affected_layer", (uint)value);
    }
}

/// <summary>Campo de vento 2D: Acoplamento XY à velocidade do ar com massa real. Família Física 2D · Campos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_area2d.html</remarks>
public readonly struct WindField2D : IComponentFacade<WindField2D>
{
    public static string TypeId => "astra.physics2d.field.wind";
    public static WindField2D Wrap(Component component) => new(component);
    public Component Component { get; }
    public WindField2D(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Meias XY</summary>
    public Vector2 HalfExtents
    {
        get => new(Component.GetFloat("half_x"), Component.GetFloat("half_y"));
        set => Component.SetVector2("half_extents", value);
    }
    /// <summary>Centro local</summary>
    public Vector2 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"));
        set => Component.SetVector2("offset", value);
    }
    /// <summary>Vento local</summary>
    public Vector2 Vector
    {
        get => new(Component.GetFloat("vector_x"), Component.GetFloat("vector_y"));
        set => Component.SetVector2("vector", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 10000.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Acoplamento (kg/s)</summary>
    /// <remarks>Faixa válida: 0 a 10000.</remarks>
    public float Coefficient
    {
        get => Component.GetFloat("coefficient");
        set => Component.SetFloat("coefficient", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Acordar corpos. Desligado: corpos em repouso permanecem dormindo</summary>
    public bool WakeBodies
    {
        get => Component.GetBool("wake_bodies");
        set => Component.SetBool("wake_bodies", value);
    }
    public enum ShapeOption : uint
    {
        Retangulo = 0,
        Circulo = 1,
    }
    /// <summary>Forma. Escala e orientação são as do objeto e de seus pais</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    public enum FalloffOption : uint
    {
        Uniforme = 0,
        Linear = 1,
        Suave = 2,
    }
    /// <summary>Queda de influência. Centro vale 1; Linear e Suave chegam a zero na borda</summary>
    public FalloffOption Falloff
    {
        get => (FalloffOption)Component.GetEnum("falloff");
        set => Component.SetEnum("falloff", (uint)value);
    }
    public enum AffectedLayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada afetada. Todas ou uma camada física do projeto; não altera a matriz de colisão</summary>
    public AffectedLayerOption AffectedLayer
    {
        get => (AffectedLayerOption)Component.GetEnum("affected_layer");
        set => Component.SetEnum("affected_layer", (uint)value);
    }
}

/// <summary>Campo de arrasto 2D: Amortecimento linear XY e angular de Body2D. Família Física 2D · Campos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_area2d.html</remarks>
public readonly struct DragField2D : IComponentFacade<DragField2D>
{
    public static string TypeId => "astra.physics2d.field.drag";
    public static DragField2D Wrap(Component component) => new(component);
    public Component Component { get; }
    public DragField2D(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Meias XY</summary>
    public Vector2 HalfExtents
    {
        get => new(Component.GetFloat("half_x"), Component.GetFloat("half_y"));
        set => Component.SetVector2("half_extents", value);
    }
    /// <summary>Centro local</summary>
    public Vector2 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"));
        set => Component.SetVector2("offset", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 10000.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Arrasto linear (1/s)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float LinearDrag
    {
        get => Component.GetFloat("linear_drag");
        set => Component.SetFloat("linear_drag", value);
    }
    /// <summary>Arrasto angular (1/s)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float AngularDrag
    {
        get => Component.GetFloat("angular_drag");
        set => Component.SetFloat("angular_drag", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Acordar corpos. Desligado: corpos em repouso permanecem dormindo</summary>
    public bool WakeBodies
    {
        get => Component.GetBool("wake_bodies");
        set => Component.SetBool("wake_bodies", value);
    }
    public enum ShapeOption : uint
    {
        Retangulo = 0,
        Circulo = 1,
    }
    /// <summary>Forma. Escala e orientação são as do objeto e de seus pais</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    public enum FalloffOption : uint
    {
        Uniforme = 0,
        Linear = 1,
        Suave = 2,
    }
    /// <summary>Queda de influência. Centro vale 1; Linear e Suave chegam a zero na borda</summary>
    public FalloffOption Falloff
    {
        get => (FalloffOption)Component.GetEnum("falloff");
        set => Component.SetEnum("falloff", (uint)value);
    }
    public enum AffectedLayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada afetada. Todas ou uma camada física do projeto; não altera a matriz de colisão</summary>
    public AffectedLayerOption AffectedLayer
    {
        get => (AffectedLayerOption)Component.GetEnum("affected_layer");
        set => Component.SetEnum("affected_layer", (uint)value);
    }
}

/// <summary>Campo radial 2D: Atração, repulsão e vórtice no plano XY. Família Física 2D · Campos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_area2d.html</remarks>
public readonly struct RadialField2D : IComponentFacade<RadialField2D>
{
    public static string TypeId => "astra.physics2d.field.radial";
    public static RadialField2D Wrap(Component component) => new(component);
    public Component Component { get; }
    public RadialField2D(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Meias XY</summary>
    public Vector2 HalfExtents
    {
        get => new(Component.GetFloat("half_x"), Component.GetFloat("half_y"));
        set => Component.SetVector2("half_extents", value);
    }
    /// <summary>Centro local</summary>
    public Vector2 Offset
    {
        get => new(Component.GetFloat("offset_x"), Component.GetFloat("offset_y"));
        set => Component.SetVector2("offset", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.001 a 10000.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Aceleração radial (m/s²)</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float Acceleration
    {
        get => Component.GetFloat("acceleration");
        set => Component.SetFloat("acceleration", value);
    }
    /// <summary>Aceleração tangencial (m/s²)</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float TangentialAcceleration
    {
        get => Component.GetFloat("tangential_acceleration");
        set => Component.SetFloat("tangential_acceleration", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Acordar corpos. Desligado: corpos em repouso permanecem dormindo</summary>
    public bool WakeBodies
    {
        get => Component.GetBool("wake_bodies");
        set => Component.SetBool("wake_bodies", value);
    }
    public enum ShapeOption : uint
    {
        Retangulo = 0,
        Circulo = 1,
    }
    /// <summary>Forma. Escala e orientação são as do objeto e de seus pais</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    public enum FalloffOption : uint
    {
        Uniforme = 0,
        Linear = 1,
        Suave = 2,
    }
    /// <summary>Queda de influência. Centro vale 1; Linear e Suave chegam a zero na borda</summary>
    public FalloffOption Falloff
    {
        get => (FalloffOption)Component.GetEnum("falloff");
        set => Component.SetEnum("falloff", (uint)value);
    }
    public enum AffectedLayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada afetada. Todas ou uma camada física do projeto; não altera a matriz de colisão</summary>
    public AffectedLayerOption AffectedLayer
    {
        get => (AffectedLayerOption)Component.GetEnum("affected_layer");
        set => Component.SetEnum("affected_layer", (uint)value);
    }
}

/// <summary>Conexão física 2D: Evento real de sensor/contato 2D altera ativação do receptor. Família Física 2D · Eventos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_area2d.html</remarks>
public readonly struct PhysicsEventConnection2D : IComponentFacade<PhysicsEventConnection2D>
{
    public static string TypeId => "astra.physics2d.event_connection";
    public static PhysicsEventConnection2D Wrap(Component component) => new(component);
    public Component Component { get; }
    public PhysicsEventConnection2D(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Ativa</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum EventOption : uint
    {
        EntradaSensor = 0,
        PermSensor = 1,
        SaidaSensor = 2,
        EntradaContato = 3,
        PermContato = 4,
        SaidaContato = 5,
    }
    /// <summary>Evento. Sensor exige um Colisor 2D sensor; contato exige uma forma sólida no emissor</summary>
    public EventOption Event
    {
        get => (EventOption)Component.GetEnum("event");
        set => Component.SetEnum("event", (uint)value);
    }
    public enum ActionOption : uint
    {
        Desconectado = 0,
        AtivarObjeto = 1,
        DesativarObjeto = 2,
        AlternarObjeto = 3,
    }
    /// <summary>Ação. Executada antes do callback C#; permanência pode disparar a cada passo físico</summary>
    public ActionOption Action
    {
        get => (ActionOption)Component.GetEnum("action");
        set => Component.SetEnum("action", (uint)value);
    }
    /// <summary>Receptor. Objeto cuja ativação será alterada</summary>
    public ObjectReference Receiver
    {
        get => Component.GetReference("receiver");
        set => Component.SetReference("receiver", value);
    }
    /// <summary>Outro objeto. Opcional: somente eventos com este outro objeto; vazio aceita todos</summary>
    public ObjectReference OtherFilter
    {
        get => Component.GetReference("other_filter");
        set => Component.SetReference("other_filter", value);
    }
}

/// <summary>Força constante 2D: Força XY e torque contínuos sobre Body2D dinâmico. Família Física 2D · Forças.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/ScriptReference/ConstantForce2D.html</remarks>
public readonly struct ConstantForce2D : IComponentFacade<ConstantForce2D>
{
    public static string TypeId => "astra.physics2d.constant-force";
    public static ConstantForce2D Wrap(Component component) => new(component);
    public Component Component { get; }
    public ConstantForce2D(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Força X (N)</summary>
    /// <remarks>Faixa válida: -1000000 a 1000000.</remarks>
    public float ForceX
    {
        get => Component.GetFloat("force_x");
        set => Component.SetFloat("force_x", value);
    }
    /// <summary>Força Y (N)</summary>
    /// <remarks>Faixa válida: -1000000 a 1000000.</remarks>
    public float ForceY
    {
        get => Component.GetFloat("force_y");
        set => Component.SetFloat("force_y", value);
    }
    /// <summary>Força local X (N)</summary>
    /// <remarks>Faixa válida: -1000000 a 1000000.</remarks>
    public float RelativeForceX
    {
        get => Component.GetFloat("relative_force_x");
        set => Component.SetFloat("relative_force_x", value);
    }
    /// <summary>Força local Y (N)</summary>
    /// <remarks>Faixa válida: -1000000 a 1000000.</remarks>
    public float RelativeForceY
    {
        get => Component.GetFloat("relative_force_y");
        set => Component.SetFloat("relative_force_y", value);
    }
    /// <summary>Torque (N m)</summary>
    /// <remarks>Faixa válida: -1000000 a 1000000.</remarks>
    public float Torque
    {
        get => Component.GetFloat("torque");
        set => Component.SetFloat("torque", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Junta 2D: Fixed/Weld, Revolute, Prismatic ou Distance reais. Família Física 2D · Juntas.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/ScriptReference/Joint2D.html</remarks>
public readonly struct Joint2D : IComponentFacade<Joint2D>
{
    public static string TypeId => "astra.physics2d.joint";
    public static Joint2D Wrap(Component component) => new(component);
    public Component Component { get; }
    public Joint2D(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Anchor A X (m)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float AnchorAX
    {
        get => Component.GetFloat("anchor_a_x");
        set => Component.SetFloat("anchor_a_x", value);
    }
    /// <summary>Anchor A Y (m)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float AnchorAY
    {
        get => Component.GetFloat("anchor_a_y");
        set => Component.SetFloat("anchor_a_y", value);
    }
    /// <summary>Anchor B X (m)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float AnchorBX
    {
        get => Component.GetFloat("anchor_b_x");
        set => Component.SetFloat("anchor_b_x", value);
    }
    /// <summary>Anchor B Y (m)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float AnchorBY
    {
        get => Component.GetFloat("anchor_b_y");
        set => Component.SetFloat("anchor_b_y", value);
    }
    /// <summary>Ângulo referência (graus)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float ReferenceAngleDegrees
    {
        get => Component.GetFloat("reference_angle_degrees");
        set => Component.SetFloat("reference_angle_degrees", value);
    }
    /// <summary>Eixo local A (graus)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float AxisAngleDegrees
    {
        get => Component.GetFloat("axis_angle_degrees");
        set => Component.SetFloat("axis_angle_degrees", value);
    }
    /// <summary>Comprimento repouso (m)</summary>
    /// <remarks>Faixa válida: 0.005 a 1000.</remarks>
    public float Length
    {
        get => Component.GetFloat("length");
        set => Component.SetFloat("length", value);
    }
    /// <summary>Comprimento mínimo (m)</summary>
    /// <remarks>Faixa válida: 0.005 a 1000.</remarks>
    public float MinLength
    {
        get => Component.GetFloat("min_length");
        set => Component.SetFloat("min_length", value);
    }
    /// <summary>Comprimento máximo (m)</summary>
    /// <remarks>Faixa válida: 0.005 a 1000.</remarks>
    public float MaxLength
    {
        get => Component.GetFloat("max_length");
        set => Component.SetFloat("max_length", value);
    }
    /// <summary>Limite inferior</summary>
    /// <remarks>Faixa válida: -178 a 178.</remarks>
    public float LowerLimit
    {
        get => Component.GetFloat("lower_limit");
        set => Component.SetFloat("lower_limit", value);
    }
    /// <summary>Limite superior</summary>
    /// <remarks>Faixa válida: -178 a 178.</remarks>
    public float UpperLimit
    {
        get => Component.GetFloat("upper_limit");
        set => Component.SetFloat("upper_limit", value);
    }
    /// <summary>Velocidade motor (m/s)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float MotorSpeed
    {
        get => Component.GetFloat("motor_speed");
        set => Component.SetFloat("motor_speed", value);
    }
    /// <summary>Motor angular (graus/s)</summary>
    /// <remarks>Faixa válida: -36000 a 36000.</remarks>
    public float MotorAngularSpeedDegrees
    {
        get => Component.GetFloat("motor_angular_speed_degrees");
        set => Component.SetFloat("motor_angular_speed_degrees", value);
    }
    /// <summary>Força máxima motor (N)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float MaxMotorForce
    {
        get => Component.GetFloat("max_motor_force");
        set => Component.SetFloat("max_motor_force", value);
    }
    /// <summary>Torque máximo motor (N m)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float MaxMotorTorque
    {
        get => Component.GetFloat("max_motor_torque");
        set => Component.SetFloat("max_motor_torque", value);
    }
    /// <summary>Frequência mola (Hz)</summary>
    /// <remarks>Faixa válida: 0 a 120.</remarks>
    public float SpringHertz
    {
        get => Component.GetFloat("spring_hertz");
        set => Component.SetFloat("spring_hertz", value);
    }
    /// <summary>Razão amortecimento</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float SpringDamping
    {
        get => Component.GetFloat("spring_damping");
        set => Component.SetFloat("spring_damping", value);
    }
    /// <summary>Translação alvo (m)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float SpringTargetTranslation
    {
        get => Component.GetFloat("spring_target_translation");
        set => Component.SetFloat("spring_target_translation", value);
    }
    /// <summary>Ângulo alvo (graus)</summary>
    /// <remarks>Faixa válida: -180 a 180.</remarks>
    public float SpringTargetAngleDegrees
    {
        get => Component.GetFloat("spring_target_angle_degrees");
        set => Component.SetFloat("spring_target_angle_degrees", value);
    }
    /// <summary>Mola linear weld (Hz)</summary>
    /// <remarks>Faixa válida: 0 a 120.</remarks>
    public float LinearHertz
    {
        get => Component.GetFloat("linear_hertz");
        set => Component.SetFloat("linear_hertz", value);
    }
    /// <summary>Mola angular weld (Hz)</summary>
    /// <remarks>Faixa válida: 0 a 120.</remarks>
    public float AngularHertz
    {
        get => Component.GetFloat("angular_hertz");
        set => Component.SetFloat("angular_hertz", value);
    }
    /// <summary>Amortecimento linear weld</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float LinearDampingRatio
    {
        get => Component.GetFloat("linear_damping_ratio");
        set => Component.SetFloat("linear_damping_ratio", value);
    }
    /// <summary>Amortecimento angular weld</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float AngularDampingRatio
    {
        get => Component.GetFloat("angular_damping_ratio");
        set => Component.SetFloat("angular_damping_ratio", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Conectar ao mundo</summary>
    public bool WorldAnchor
    {
        get => Component.GetBool("world_anchor");
        set => Component.SetBool("world_anchor", value);
    }
    /// <summary>Colidir conectados</summary>
    public bool CollideConnected
    {
        get => Component.GetBool("collide_connected");
        set => Component.SetBool("collide_connected", value);
    }
    /// <summary>Limites</summary>
    public bool LimitEnabled
    {
        get => Component.GetBool("limit_enabled");
        set => Component.SetBool("limit_enabled", value);
    }
    /// <summary>Motor</summary>
    public bool MotorEnabled
    {
        get => Component.GetBool("motor_enabled");
        set => Component.SetBool("motor_enabled", value);
    }
    /// <summary>Mola</summary>
    public bool SpringEnabled
    {
        get => Component.GetBool("spring_enabled");
        set => Component.SetBool("spring_enabled", value);
    }
    public enum KindOption : uint
    {
        FixedWeld = 0,
        Revolute = 1,
        Prismatic = 2,
        Distance = 3,
    }
    /// <summary>Modo</summary>
    public KindOption Kind
    {
        get => (KindOption)Component.GetEnum("kind");
        set => Component.SetEnum("kind", (uint)value);
    }
    /// <summary>Corpo conectado</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Corpo 2D: Corpo XY independente de física 3D. Família Física 2D · Corpos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_rigidbody2d.html</remarks>
public readonly struct Body2D : IComponentFacade<Body2D>
{
    public static string TypeId => "astra.physics2d.body";
    public static Body2D Wrap(Component component) => new(component);
    public Component Component { get; }
    public Body2D(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Massa (kg)</summary>
    /// <remarks>Faixa válida: 0.001 a 100000.</remarks>
    public float Mass
    {
        get => Component.GetFloat("mass");
        set => Component.SetFloat("mass", value);
    }
    /// <summary>Escala gravidade</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float GravityScale
    {
        get => Component.GetFloat("gravity_scale");
        set => Component.SetFloat("gravity_scale", value);
    }
    /// <summary>Velocidade X (m/s)</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float VelocityX
    {
        get => Component.GetFloat("velocity_x");
        set => Component.SetFloat("velocity_x", value);
    }
    /// <summary>Velocidade Y (m/s)</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float VelocityY
    {
        get => Component.GetFloat("velocity_y");
        set => Component.SetFloat("velocity_y", value);
    }
    /// <summary>Velocidade angular (graus/s)</summary>
    /// <remarks>Faixa válida: -36000 a 36000.</remarks>
    public float AngularVelocityDegrees
    {
        get => Component.GetFloat("angular_velocity_degrees");
        set => Component.SetFloat("angular_velocity_degrees", value);
    }
    /// <summary>Arrasto linear</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float LinearDamping
    {
        get => Component.GetFloat("linear_damping");
        set => Component.SetFloat("linear_damping", value);
    }
    /// <summary>Arrasto angular</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float AngularDamping
    {
        get => Component.GetFloat("angular_damping");
        set => Component.SetFloat("angular_damping", value);
    }
    /// <summary>Fixar rotação</summary>
    public bool FixedRotation
    {
        get => Component.GetBool("fixed_rotation");
        set => Component.SetBool("fixed_rotation", value);
    }
    /// <summary>Permitir repouso</summary>
    public bool AllowSleep
    {
        get => Component.GetBool("allow_sleep");
        set => Component.SetBool("allow_sleep", value);
    }
    public enum MotionOption : uint
    {
        Estatico = 0,
        Cinematico = 1,
        Dinamico = 2,
    }
    /// <summary>Movimento</summary>
    public MotionOption Motion
    {
        get => (MotionOption)Component.GetEnum("motion");
        set => Component.SetEnum("motion", (uint)value);
    }
}

/// <summary>Colisor 2D: Caixa, círculo ou cápsula; sensor real. Família Física 2D · Formas.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_collisionshape2d.html</remarks>
public readonly struct Collider2D : IComponentFacade<Collider2D>
{
    public static string TypeId => "astra.physics2d.collider";
    public static Collider2D Wrap(Component component) => new(component);
    public Component Component { get; }
    public Collider2D(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Meia largura (m)</summary>
    /// <remarks>Faixa válida: 0.005 a 500.</remarks>
    public float HalfX
    {
        get => Component.GetFloat("half_x");
        set => Component.SetFloat("half_x", value);
    }
    /// <summary>Meia altura (m)</summary>
    /// <remarks>Faixa válida: 0.005 a 500.</remarks>
    public float HalfY
    {
        get => Component.GetFloat("half_y");
        set => Component.SetFloat("half_y", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.005 a 500.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Meia distância centros (m)</summary>
    /// <remarks>Faixa válida: 0.005 a 500.</remarks>
    public float CapsuleHalfLength
    {
        get => Component.GetFloat("capsule_half_length");
        set => Component.SetFloat("capsule_half_length", value);
    }
    /// <summary>Centro X (m)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float OffsetX
    {
        get => Component.GetFloat("offset_x");
        set => Component.SetFloat("offset_x", value);
    }
    /// <summary>Centro Y (m)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float OffsetY
    {
        get => Component.GetFloat("offset_y");
        set => Component.SetFloat("offset_y", value);
    }
    /// <summary>Atrito</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Friction
    {
        get => Component.GetFloat("friction");
        set => Component.SetFloat("friction", value);
    }
    /// <summary>Restituição</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Restitution
    {
        get => Component.GetFloat("restitution");
        set => Component.SetFloat("restitution", value);
    }
    /// <summary>Sensor sem resposta</summary>
    public bool Sensor
    {
        get => Component.GetBool("sensor");
        set => Component.SetBool("sensor", value);
    }
    public enum ShapeOption : uint
    {
        Caixa = 0,
        Circulo = 1,
        CapsulaY = 2,
    }
    /// <summary>Forma</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    /// <summary>Sensor: entrou. Outro corpo começou a sobrepor este sensor. Payload: Outro objeto: objeto</summary>
    public ComponentSubscription OnTriggerEnter(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "trigger_enter", handler);
    /// <summary>Sensor: saiu. Outro corpo deixou de sobrepor este sensor. Payload: Outro objeto: objeto</summary>
    public ComponentSubscription OnTriggerExit(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "trigger_exit", handler);
    /// <summary>Colisão: começou. Contato sólido começou; entregue aos dois colisores. Payload: Outro objeto: objeto</summary>
    public ComponentSubscription OnCollisionEnter(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "collision_enter", handler);
    /// <summary>Colisão: terminou. Contato sólido terminou; entregue aos dois colisores. Payload: Outro objeto: objeto</summary>
    public ComponentSubscription OnCollisionExit(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "collision_exit", handler);
}

/// <summary>Fonte de áudio: Clipe de projeto com reprodução e espaço acústico. Família Áudio · Reprodução.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioSource.html</remarks>
public readonly struct AudioSource : IComponentFacade<AudioSource>
{
    public static string TypeId => "astra.audio.source";
    public static AudioSource Wrap(Component component) => new(component);
    public Component Component { get; }
    public AudioSource(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Volume</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Volume
    {
        get => Component.GetFloat("volume");
        set => Component.SetFloat("volume", value);
    }
    /// <summary>Velocidade / pitch (×)</summary>
    /// <remarks>Faixa válida: 0.1 a 4.</remarks>
    public float Pitch
    {
        get => Component.GetFloat("pitch");
        set => Component.SetFloat("pitch", value);
    }
    /// <summary>Pan estéreo. Em 3D, vale para a parte 2D da mistura espacial</summary>
    /// <remarks>Faixa válida: -1 a 1.</remarks>
    public float Pan
    {
        get => Component.GetFloat("pan");
        set => Component.SetFloat("pan", value);
    }
    /// <summary>Distância mínima (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 100000.</remarks>
    public float MinDistance
    {
        get => Component.GetFloat("min_distance");
        set => Component.SetFloat("min_distance", value);
    }
    /// <summary>Distância máxima (m)</summary>
    /// <remarks>Faixa válida: 0.02 a 100001.</remarks>
    public float MaxDistance
    {
        get => Component.GetFloat("max_distance");
        set => Component.SetFloat("max_distance", value);
    }
    /// <summary>Decaimento</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float RolloffFactor
    {
        get => Component.GetFloat("rolloff_factor");
        set => Component.SetFloat("rolloff_factor", value);
    }
    /// <summary>Cone interno (°)</summary>
    /// <remarks>Faixa válida: 0 a 360.</remarks>
    public float ConeInner
    {
        get => Component.GetFloat("cone_inner");
        set => Component.SetFloat("cone_inner", value);
    }
    /// <summary>Cone externo (°)</summary>
    /// <remarks>Faixa válida: 0 a 360.</remarks>
    public float ConeOuter
    {
        get => Component.GetFloat("cone_outer");
        set => Component.SetFloat("cone_outer", value);
    }
    /// <summary>Ganho fora do cone</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float ConeGain
    {
        get => Component.GetFloat("cone_gain");
        set => Component.SetFloat("cone_gain", value);
    }
    /// <summary>Doppler (×)</summary>
    /// <remarks>Faixa válida: 0 a 4.</remarks>
    public float Doppler
    {
        get => Component.GetFloat("doppler");
        set => Component.SetFloat("doppler", value);
    }
    /// <summary>Mistura espacial. 1 = totalmente 3D; 0 = 2D sem distância nem direção</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float SpatialBlend
    {
        get => Component.GetFloat("spatial_blend");
        set => Component.SetFloat("spatial_blend", value);
    }
    /// <summary>Prioridade. 0 é a mais importante; acima do limite de vozes, as de menor prioridade ficam virtuais</summary>
    /// <remarks>Faixa válida: 0 a 256.</remarks>
    public float Priority
    {
        get => Component.GetFloat("priority");
        set => Component.SetFloat("priority", value);
    }
    /// <summary>Início do loop (s). Ao repetir, volta a este ponto</summary>
    /// <remarks>Faixa válida: 0 a 36000.</remarks>
    public float LoopStart
    {
        get => Component.GetFloat("loop_start");
        set => Component.SetFloat("loop_start", value);
    }
    /// <summary>Fim do loop (s). Zero usa o fim do clipe</summary>
    /// <remarks>Faixa válida: 0 a 36000.</remarks>
    public float LoopEnd
    {
        get => Component.GetFloat("loop_end");
        set => Component.SetFloat("loop_end", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Silenciar</summary>
    public bool Mute
    {
        get => Component.GetBool("mute");
        set => Component.SetBool("mute", value);
    }
    /// <summary>Repetir</summary>
    public bool Loop
    {
        get => Component.GetBool("loop");
        set => Component.SetBool("loop", value);
    }
    public enum PlaybackOption : uint
    {
        Parar = 0,
        Tocar = 1,
        Pausar = 2,
    }
    /// <summary>Pedido. Pedido persistido; consulte o estado real na nota do componente.</summary>
    public PlaybackOption Playback
    {
        get => (PlaybackOption)Component.GetEnum("playback");
        set => Component.SetEnum("playback", (uint)value);
    }
    public enum DimensionOption : uint
    {
        V2DEstereo = 0,
        V3DEspacial = 1,
    }
    /// <summary>Dimensão</summary>
    public DimensionOption Dimension
    {
        get => (DimensionOption)Component.GetEnum("dimension");
        set => Component.SetEnum("dimension", (uint)value);
    }
    public enum RolloffOption : uint
    {
        Linear = 0,
        Inverso = 1,
        Exponencial = 2,
    }
    /// <summary>Atenuação</summary>
    public RolloffOption Rolloff
    {
        get => (RolloffOption)Component.GetEnum("rolloff");
        set => Component.SetEnum("rolloff", (uint)value);
    }
    public enum LoadingOption : uint
    {
        Memoria = 0,
        Streaming = 1,
    }
    /// <summary>Carregamento. Streaming lê o arquivo aos poucos: para músicas e falas longas</summary>
    public LoadingOption Loading
    {
        get => (LoadingOption)Component.GetEnum("loading");
        set => Component.SetEnum("loading", (uint)value);
    }
    /// <summary>Bus</summary>
    public ObjectReference Bus
    {
        get => Component.GetReference("bus");
        set => Component.SetReference("bus", value);
    }
    /// <summary>Clipe WAV. Recurso do projeto por slot</summary>
    public AssetGuid GetClip(uint slot = 0) => Component.GetResource("clip", slot);
    public void SetClip(AssetGuid value, uint slot = 0) => Component.SetResource("clip", value, slot);
    /// <summary>Tocar. Recomeça do início</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Play() => Component.Invoke("play");
    /// <summary>Pausar. Congela o cursor</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Pause() => Component.Invoke("pause");
    /// <summary>Retomar. Continua do cursor pausado</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Resume() => Component.Invoke("resume");
    /// <summary>Parar. Interrompe e libera a voz</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Stop() => Component.Invoke("stop");
    /// <summary>Posicionar. Move o cursor; aplicado pelo mixer</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Seek(double seconds) => Component.Invoke("seek", ComponentValue.Number(seconds));
    /// <summary>Virtual. Verdadeiro quando a voz passou do limite de vozes e é acompanhada sem tocar</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool IsVirtual() => Component.Invoke("is_virtual").AsBoolean();
}

/// <summary>Ouvinte de áudio: Pose e volume de escuta escolhidos por prioridade. Família Áudio · Escuta.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioListener.html</remarks>
public readonly struct AudioListener : IComponentFacade<AudioListener>
{
    public static string TypeId => "astra.audio.listener";
    public static AudioListener Wrap(Component component) => new(component);
    public Component Component { get; }
    public AudioListener(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Volume global</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Volume
    {
        get => Component.GetFloat("volume");
        set => Component.SetFloat("volume", value);
    }
    /// <summary>Prioridade</summary>
    /// <remarks>Faixa válida: 0 a 255.</remarks>
    public float Priority
    {
        get => Component.GetFloat("priority");
        set => Component.SetFloat("priority", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Bus de áudio: Nó do mixer: ganho, mute, solo, cadeia de efeitos e envios até Master. Família Áudio · Mixer.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/tutorials/audio/audio_buses.html</remarks>
public readonly struct AudioBus : IComponentFacade<AudioBus>
{
    public static string TypeId => "astra.audio.bus";
    public static AudioBus Wrap(Component component) => new(component);
    public Component Component { get; }
    public AudioBus(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Ganho</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Volume
    {
        get => Component.GetFloat("volume");
        set => Component.SetFloat("volume", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Silenciar</summary>
    public bool Mute
    {
        get => Component.GetBool("mute");
        set => Component.SetBool("mute", value);
    }
    /// <summary>Solo</summary>
    public bool Solo
    {
        get => Component.GetBool("solo");
        set => Component.SetBool("solo", value);
    }
    /// <summary>Saída</summary>
    public ObjectReference Output
    {
        get => Component.GetReference("output");
        set => Component.SetReference("output", value);
    }
    /// <summary>Pico (dB). Pico de saída do bus, com queda de 20 dB/s; -120 em silêncio</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double PeakDb() => Component.Invoke("peak_db").AsNumber();
}

/// <summary>Filtro de áudio: Passa-baixa, passa-alta, banda, rejeita-banda, pico e prateleiras. Família Áudio · Mixer.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_audioeffectfilter.html</remarks>
public readonly struct AudioFilter : IComponentFacade<AudioFilter>
{
    public static string TypeId => "astra.audio.filter";
    public static AudioFilter Wrap(Component component) => new(component);
    public Component Component { get; }
    public AudioFilter(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Frequência de corte (Hz)</summary>
    /// <remarks>Faixa válida: 20 a 20000.</remarks>
    public float Cutoff
    {
        get => Component.GetFloat("cutoff");
        set => Component.SetFloat("cutoff", value);
    }
    /// <summary>Ressonância. Q do filtro; 0,707 é plano</summary>
    /// <remarks>Faixa válida: 0.1 a 10.</remarks>
    public float Resonance
    {
        get => Component.GetFloat("resonance");
        set => Component.SetFloat("resonance", value);
    }
    /// <summary>Ganho (dB). Reforço ou corte na faixa</summary>
    /// <remarks>Faixa válida: -24 a 24.</remarks>
    public float Gain
    {
        get => Component.GetFloat("gain");
        set => Component.SetFloat("gain", value);
    }
    /// <summary>Ativo. Desligado deixa o sinal passar sem alteração</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum ModeOption : uint
    {
        PassaBaixa = 0,
        PassaAlta = 1,
        PassaBanda = 2,
        RejeitaBanda = 3,
        Pico = 4,
        PrateleiraGrave = 5,
        PrateleiraAguda = 6,
    }
    /// <summary>Tipo. Passa-baixa abafa (porta fechada, embaixo d'água); passa-alta afina (rádio, telefone)</summary>
    public ModeOption Mode
    {
        get => (ModeOption)Component.GetEnum("mode");
        set => Component.SetEnum("mode", (uint)value);
    }
}

/// <summary>Eco: Repetições atrasadas com realimentação. Família Áudio · Mixer.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-AudioEchoEffect.html</remarks>
public readonly struct AudioEcho : IComponentFacade<AudioEcho>
{
    public static string TypeId => "astra.audio.echo";
    public static AudioEcho Wrap(Component component) => new(component);
    public Component Component { get; }
    public AudioEcho(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Atraso (ms). Intervalo entre as repetições</summary>
    /// <remarks>Faixa válida: 1 a 5000.</remarks>
    public float Delay
    {
        get => Component.GetFloat("delay");
        set => Component.SetFloat("delay", value);
    }
    /// <summary>Realimentação. Quanto de cada repetição volta ao atraso</summary>
    /// <remarks>Faixa válida: 0 a 0.95.</remarks>
    public float Feedback
    {
        get => Component.GetFloat("feedback");
        set => Component.SetFloat("feedback", value);
    }
    /// <summary>Mistura do eco</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Wet
    {
        get => Component.GetFloat("wet");
        set => Component.SetFloat("wet", value);
    }
    /// <summary>Sinal original</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Dry
    {
        get => Component.GetFloat("dry");
        set => Component.SetFloat("dry", value);
    }
    /// <summary>Ativo. Desligado esvazia as repetições</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Reverberação: Sala simulada com tamanho, amortecimento e pré-atraso. Família Áudio · Mixer.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_audioeffectreverb.html</remarks>
public readonly struct AudioReverb : IComponentFacade<AudioReverb>
{
    public static string TypeId => "astra.audio.reverb";
    public static AudioReverb Wrap(Component component) => new(component);
    public Component Component { get; }
    public AudioReverb(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Tamanho da sala. Maior deixa a cauda mais longa</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float RoomSize
    {
        get => Component.GetFloat("room_size");
        set => Component.SetFloat("room_size", value);
    }
    /// <summary>Amortecimento. Absorção dos agudos pelas paredes</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Damping
    {
        get => Component.GetFloat("damping");
        set => Component.SetFloat("damping", value);
    }
    /// <summary>Largura. Abertura estéreo da reverberação</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Width
    {
        get => Component.GetFloat("width");
        set => Component.SetFloat("width", value);
    }
    /// <summary>Pré-atraso (ms). Tempo até as primeiras reflexões</summary>
    /// <remarks>Faixa válida: 0 a 500.</remarks>
    public float Predelay
    {
        get => Component.GetFloat("predelay");
        set => Component.SetFloat("predelay", value);
    }
    /// <summary>Mistura da reverberação</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Wet
    {
        get => Component.GetFloat("wet");
        set => Component.SetFloat("wet", value);
    }
    /// <summary>Sinal original. Zero em um bus de envio deixa só a reverberação</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Dry
    {
        get => Component.GetFloat("dry");
        set => Component.SetFloat("dry", value);
    }
    /// <summary>Ativo. Desligado corta a reverberação</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
}

/// <summary>Compressor: Controla picos; com sidechain abaixa este bus quando outro soa. Família Áudio · Mixer.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_audioeffectcompressor.html</remarks>
public readonly struct AudioCompressor : IComponentFacade<AudioCompressor>
{
    public static string TypeId => "astra.audio.compressor";
    public static AudioCompressor Wrap(Component component) => new(component);
    public Component Component { get; }
    public AudioCompressor(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Limiar (dB). Acima deste nível o volume é reduzido</summary>
    /// <remarks>Faixa válida: -60 a 0.</remarks>
    public float Threshold
    {
        get => Component.GetFloat("threshold");
        set => Component.SetFloat("threshold", value);
    }
    /// <summary>Razão (: 1). 4 reduz 4 dB acima do limiar para cada 1 dB que passa</summary>
    /// <remarks>Faixa válida: 1 a 48.</remarks>
    public float Ratio
    {
        get => Component.GetFloat("ratio");
        set => Component.SetFloat("ratio", value);
    }
    /// <summary>Ataque (ms)</summary>
    /// <remarks>Faixa válida: 0.02 a 250.</remarks>
    public float Attack
    {
        get => Component.GetFloat("attack");
        set => Component.SetFloat("attack", value);
    }
    /// <summary>Liberação (ms)</summary>
    /// <remarks>Faixa válida: 1 a 2000.</remarks>
    public float Release
    {
        get => Component.GetFloat("release");
        set => Component.SetFloat("release", value);
    }
    /// <summary>Ganho de compensação (dB)</summary>
    /// <remarks>Faixa válida: 0 a 24.</remarks>
    public float Makeup
    {
        get => Component.GetFloat("makeup");
        set => Component.SetFloat("makeup", value);
    }
    /// <summary>Mistura. Menor que 1 soma o sinal sem compressão (paralela)</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Mix
    {
        get => Component.GetFloat("mix");
        set => Component.SetFloat("mix", value);
    }
    /// <summary>Ativo. Desligado deixa o sinal passar sem alteração</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Sidechain. Comprime quando OUTRO bus soa: música abaixa quando há fala (ducking)</summary>
    public ObjectReference Sidechain
    {
        get => Component.GetReference("sidechain");
        set => Component.SetReference("sidechain", value);
    }
    /// <summary>Redução (dB). Quanto o compressor está abaixando agora</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double ReductionDb() => Component.Invoke("reduction_db").AsNumber();
}

/// <summary>Envio de áudio: Copia o sinal deste ponto da cadeia para outro bus. Família Áudio · Mixer.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/AudioMixer.html</remarks>
public readonly struct AudioSend : IComponentFacade<AudioSend>
{
    public static string TypeId => "astra.audio.send";
    public static AudioSend Wrap(Component component) => new(component);
    public Component Component { get; }
    public AudioSend(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Nível do envio. Fração do sinal deste ponto da cadeia que vai ao destino</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Level
    {
        get => Component.GetFloat("level");
        set => Component.SetFloat("level", value);
    }
    /// <summary>Ativo. Desligado não envia nada</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Destino. Bus que recebe a cópia, por exemplo um bus só com Reverb</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
}

/// <summary>Snapshot de mixer: Leva ganhos e efeitos a valores salvos com transição. Família Áudio · Mixer.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/AudioMixerSnapshots.html</remarks>
public readonly struct AudioSnapshot : IComponentFacade<AudioSnapshot>
{
    public static string TypeId => "astra.audio.snapshot";
    public static AudioSnapshot Wrap(Component component) => new(component);
    public Component Component { get; }
    public AudioSnapshot(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Transição (s). Tempo real até os valores do snapshot</summary>
    /// <remarks>Faixa válida: 0 a 60.</remarks>
    public float Transition
    {
        get => Component.GetFloat("transition");
        set => Component.SetFloat("transition", value);
    }
    /// <summary>Valor. Limitado à faixa da propriedade de destino</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float Slot0Value
    {
        get => Component.GetFloat("slot_0_value");
        set => Component.SetFloat("slot_0_value", value);
    }
    /// <summary>Valor. Limitado à faixa da propriedade de destino</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float Slot1Value
    {
        get => Component.GetFloat("slot_1_value");
        set => Component.SetFloat("slot_1_value", value);
    }
    /// <summary>Valor. Limitado à faixa da propriedade de destino</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float Slot2Value
    {
        get => Component.GetFloat("slot_2_value");
        set => Component.SetFloat("slot_2_value", value);
    }
    /// <summary>Valor. Limitado à faixa da propriedade de destino</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float Slot3Value
    {
        get => Component.GetFloat("slot_3_value");
        set => Component.SetFloat("slot_3_value", value);
    }
    /// <summary>Valor. Limitado à faixa da propriedade de destino</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float Slot4Value
    {
        get => Component.GetFloat("slot_4_value");
        set => Component.SetFloat("slot_4_value", value);
    }
    /// <summary>Valor. Limitado à faixa da propriedade de destino</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float Slot5Value
    {
        get => Component.GetFloat("slot_5_value");
        set => Component.SetFloat("slot_5_value", value);
    }
    /// <summary>Valor. Limitado à faixa da propriedade de destino</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float Slot6Value
    {
        get => Component.GetFloat("slot_6_value");
        set => Component.SetFloat("slot_6_value", value);
    }
    /// <summary>Valor. Limitado à faixa da propriedade de destino</summary>
    /// <remarks>Faixa válida: -100000 a 100000.</remarks>
    public float Slot7Value
    {
        get => Component.GetFloat("slot_7_value");
        set => Component.SetFloat("slot_7_value", value);
    }
    /// <summary>Ativo. Desligado recusa transições</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Aplicar ao iniciar. Os valores entram no primeiro quadro do Play, sem transição</summary>
    public bool ApplyAtStart
    {
        get => Component.GetBool("apply_at_start");
        set => Component.SetBool("apply_at_start", value);
    }
    public enum Slot0ParameterOption : uint
    {
        GanhoDoBus = 0,
        CorteDoFiltro = 1,
        RessonanciaDoFiltro = 2,
        GanhoDoFiltro = 3,
        MisturaDoEco = 4,
        MisturaDaReverberacao = 5,
        TamanhoDaSala = 6,
        NivelDoEnvio = 7,
        LimiarDoCompressor = 8,
    }
    /// <summary>Parâmetro. Usa o primeiro componente daquele tipo no objeto</summary>
    public Slot0ParameterOption Slot0Parameter
    {
        get => (Slot0ParameterOption)Component.GetEnum("slot_0_parameter");
        set => Component.SetEnum("slot_0_parameter", (uint)value);
    }
    public enum Slot1ParameterOption : uint
    {
        GanhoDoBus = 0,
        CorteDoFiltro = 1,
        RessonanciaDoFiltro = 2,
        GanhoDoFiltro = 3,
        MisturaDoEco = 4,
        MisturaDaReverberacao = 5,
        TamanhoDaSala = 6,
        NivelDoEnvio = 7,
        LimiarDoCompressor = 8,
    }
    /// <summary>Parâmetro. Usa o primeiro componente daquele tipo no objeto</summary>
    public Slot1ParameterOption Slot1Parameter
    {
        get => (Slot1ParameterOption)Component.GetEnum("slot_1_parameter");
        set => Component.SetEnum("slot_1_parameter", (uint)value);
    }
    public enum Slot2ParameterOption : uint
    {
        GanhoDoBus = 0,
        CorteDoFiltro = 1,
        RessonanciaDoFiltro = 2,
        GanhoDoFiltro = 3,
        MisturaDoEco = 4,
        MisturaDaReverberacao = 5,
        TamanhoDaSala = 6,
        NivelDoEnvio = 7,
        LimiarDoCompressor = 8,
    }
    /// <summary>Parâmetro. Usa o primeiro componente daquele tipo no objeto</summary>
    public Slot2ParameterOption Slot2Parameter
    {
        get => (Slot2ParameterOption)Component.GetEnum("slot_2_parameter");
        set => Component.SetEnum("slot_2_parameter", (uint)value);
    }
    public enum Slot3ParameterOption : uint
    {
        GanhoDoBus = 0,
        CorteDoFiltro = 1,
        RessonanciaDoFiltro = 2,
        GanhoDoFiltro = 3,
        MisturaDoEco = 4,
        MisturaDaReverberacao = 5,
        TamanhoDaSala = 6,
        NivelDoEnvio = 7,
        LimiarDoCompressor = 8,
    }
    /// <summary>Parâmetro. Usa o primeiro componente daquele tipo no objeto</summary>
    public Slot3ParameterOption Slot3Parameter
    {
        get => (Slot3ParameterOption)Component.GetEnum("slot_3_parameter");
        set => Component.SetEnum("slot_3_parameter", (uint)value);
    }
    public enum Slot4ParameterOption : uint
    {
        GanhoDoBus = 0,
        CorteDoFiltro = 1,
        RessonanciaDoFiltro = 2,
        GanhoDoFiltro = 3,
        MisturaDoEco = 4,
        MisturaDaReverberacao = 5,
        TamanhoDaSala = 6,
        NivelDoEnvio = 7,
        LimiarDoCompressor = 8,
    }
    /// <summary>Parâmetro. Usa o primeiro componente daquele tipo no objeto</summary>
    public Slot4ParameterOption Slot4Parameter
    {
        get => (Slot4ParameterOption)Component.GetEnum("slot_4_parameter");
        set => Component.SetEnum("slot_4_parameter", (uint)value);
    }
    public enum Slot5ParameterOption : uint
    {
        GanhoDoBus = 0,
        CorteDoFiltro = 1,
        RessonanciaDoFiltro = 2,
        GanhoDoFiltro = 3,
        MisturaDoEco = 4,
        MisturaDaReverberacao = 5,
        TamanhoDaSala = 6,
        NivelDoEnvio = 7,
        LimiarDoCompressor = 8,
    }
    /// <summary>Parâmetro. Usa o primeiro componente daquele tipo no objeto</summary>
    public Slot5ParameterOption Slot5Parameter
    {
        get => (Slot5ParameterOption)Component.GetEnum("slot_5_parameter");
        set => Component.SetEnum("slot_5_parameter", (uint)value);
    }
    public enum Slot6ParameterOption : uint
    {
        GanhoDoBus = 0,
        CorteDoFiltro = 1,
        RessonanciaDoFiltro = 2,
        GanhoDoFiltro = 3,
        MisturaDoEco = 4,
        MisturaDaReverberacao = 5,
        TamanhoDaSala = 6,
        NivelDoEnvio = 7,
        LimiarDoCompressor = 8,
    }
    /// <summary>Parâmetro. Usa o primeiro componente daquele tipo no objeto</summary>
    public Slot6ParameterOption Slot6Parameter
    {
        get => (Slot6ParameterOption)Component.GetEnum("slot_6_parameter");
        set => Component.SetEnum("slot_6_parameter", (uint)value);
    }
    public enum Slot7ParameterOption : uint
    {
        GanhoDoBus = 0,
        CorteDoFiltro = 1,
        RessonanciaDoFiltro = 2,
        GanhoDoFiltro = 3,
        MisturaDoEco = 4,
        MisturaDaReverberacao = 5,
        TamanhoDaSala = 6,
        NivelDoEnvio = 7,
        LimiarDoCompressor = 8,
    }
    /// <summary>Parâmetro. Usa o primeiro componente daquele tipo no objeto</summary>
    public Slot7ParameterOption Slot7Parameter
    {
        get => (Slot7ParameterOption)Component.GetEnum("slot_7_parameter");
        set => Component.SetEnum("slot_7_parameter", (uint)value);
    }
    /// <summary>Objeto 1. Bus de áudio, ou o objeto do bus com o efeito</summary>
    public ObjectReference Slot0Target
    {
        get => Component.GetReference("slot_0_target");
        set => Component.SetReference("slot_0_target", value);
    }
    /// <summary>Objeto 2. Bus de áudio, ou o objeto do bus com o efeito</summary>
    public ObjectReference Slot1Target
    {
        get => Component.GetReference("slot_1_target");
        set => Component.SetReference("slot_1_target", value);
    }
    /// <summary>Objeto 3. Bus de áudio, ou o objeto do bus com o efeito</summary>
    public ObjectReference Slot2Target
    {
        get => Component.GetReference("slot_2_target");
        set => Component.SetReference("slot_2_target", value);
    }
    /// <summary>Objeto 4. Bus de áudio, ou o objeto do bus com o efeito</summary>
    public ObjectReference Slot3Target
    {
        get => Component.GetReference("slot_3_target");
        set => Component.SetReference("slot_3_target", value);
    }
    /// <summary>Objeto 5. Bus de áudio, ou o objeto do bus com o efeito</summary>
    public ObjectReference Slot4Target
    {
        get => Component.GetReference("slot_4_target");
        set => Component.SetReference("slot_4_target", value);
    }
    /// <summary>Objeto 6. Bus de áudio, ou o objeto do bus com o efeito</summary>
    public ObjectReference Slot5Target
    {
        get => Component.GetReference("slot_5_target");
        set => Component.SetReference("slot_5_target", value);
    }
    /// <summary>Objeto 7. Bus de áudio, ou o objeto do bus com o efeito</summary>
    public ObjectReference Slot6Target
    {
        get => Component.GetReference("slot_6_target");
        set => Component.SetReference("slot_6_target", value);
    }
    /// <summary>Objeto 8. Bus de áudio, ou o objeto do bus com o efeito</summary>
    public ObjectReference Slot7Target
    {
        get => Component.GetReference("slot_7_target");
        set => Component.SetReference("slot_7_target", value);
    }
    /// <summary>Transicionar. Leva os valores atuais aos do snapshot no tempo dado; negativo usa a Transição do componente</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void TransitionTo(double seconds) => Component.Invoke("transition_to", ComponentValue.Number(seconds));
    /// <summary>Aplicar. Aplica os valores na hora, sem transição</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Apply() => Component.Invoke("apply");
}

/// <summary>Path: Curva Bézier local com pontos persistentes. Família Lógica · Caminhos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_path3d.html</remarks>
public readonly struct PathComponent : IComponentFacade<PathComponent>
{
    public static string TypeId => "astra.path";
    public static PathComponent Wrap(Component component) => new(component);
    public Component Component { get; }
    public PathComponent(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Up inicial local</summary>
    public Vector3 Up
    {
        get => new(Component.GetFloat("up_x"), Component.GetFloat("up_y"), Component.GetFloat("up_z"));
        set => Component.SetVector3("up", value);
    }
    /// <summary>Fechado</summary>
    public bool Closed
    {
        get => Component.GetBool("closed");
        set => Component.SetBool("closed", value);
    }
    /// <summary>Posição X (u). Valor por slot</summary>
    public float GetPointPositionX(uint slot) => Component.GetSlotFloat("point_position_x", slot);
    public void SetPointPositionX(uint slot, float value) => Component.SetSlotFloat("point_position_x", value, slot);
    /// <summary>Posição Y (u). Valor por slot</summary>
    public float GetPointPositionY(uint slot) => Component.GetSlotFloat("point_position_y", slot);
    public void SetPointPositionY(uint slot, float value) => Component.SetSlotFloat("point_position_y", value, slot);
    /// <summary>Posição Z (u). Valor por slot</summary>
    public float GetPointPositionZ(uint slot) => Component.GetSlotFloat("point_position_z", slot);
    public void SetPointPositionZ(uint slot, float value) => Component.SetSlotFloat("point_position_z", value, slot);
    /// <summary>Entrada X (u). Valor por slot</summary>
    public float GetPointInX(uint slot) => Component.GetSlotFloat("point_in_x", slot);
    public void SetPointInX(uint slot, float value) => Component.SetSlotFloat("point_in_x", value, slot);
    /// <summary>Entrada Y (u). Valor por slot</summary>
    public float GetPointInY(uint slot) => Component.GetSlotFloat("point_in_y", slot);
    public void SetPointInY(uint slot, float value) => Component.SetSlotFloat("point_in_y", value, slot);
    /// <summary>Entrada Z (u). Valor por slot</summary>
    public float GetPointInZ(uint slot) => Component.GetSlotFloat("point_in_z", slot);
    public void SetPointInZ(uint slot, float value) => Component.SetSlotFloat("point_in_z", value, slot);
    /// <summary>Saída X (u). Valor por slot</summary>
    public float GetPointOutX(uint slot) => Component.GetSlotFloat("point_out_x", slot);
    public void SetPointOutX(uint slot, float value) => Component.SetSlotFloat("point_out_x", value, slot);
    /// <summary>Saída Y (u). Valor por slot</summary>
    public float GetPointOutY(uint slot) => Component.GetSlotFloat("point_out_y", slot);
    public void SetPointOutY(uint slot, float value) => Component.SetSlotFloat("point_out_y", value, slot);
    /// <summary>Saída Z (u). Valor por slot</summary>
    public float GetPointOutZ(uint slot) => Component.GetSlotFloat("point_out_z", slot);
    public void SetPointOutZ(uint slot, float value) => Component.SetSlotFloat("point_out_z", value, slot);
    /// <summary>Roll (°). Valor por slot</summary>
    public float GetPointRoll(uint slot) => Component.GetSlotFloat("point_roll", slot);
    public void SetPointRoll(uint slot, float value) => Component.SetSlotFloat("point_roll", value, slot);
}

/// <summary>Path Follow: Percorre curva em distância mundial e orienta +Z. Família Lógica · Caminhos.</summary>
/// <remarks>Referência estudada: https://docs.godotengine.org/en/4.5/classes/class_pathfollow3d.html</remarks>
public readonly struct PathFollow : IComponentFacade<PathFollow>
{
    public static string TypeId => "astra.path.follow";
    public static PathFollow Wrap(Component component) => new(component);
    public Component Component { get; }
    public PathFollow(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Distância inicial (u)</summary>
    /// <remarks>Faixa válida: 0 a 1000000.</remarks>
    public float ProgressDistance
    {
        get => Component.GetFloat("progress_distance");
        set => Component.SetFloat("progress_distance", value);
    }
    /// <summary>Velocidade (u/s)</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float Speed
    {
        get => Component.GetFloat("speed");
        set => Component.SetFloat("speed", value);
    }
    /// <summary>Duração (s)</summary>
    /// <remarks>Faixa válida: 0.001 a 100000.</remarks>
    public float Duration
    {
        get => Component.GetFloat("duration");
        set => Component.SetFloat("duration", value);
    }
    /// <summary>Deslocamento lateral (u)</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float OffsetX
    {
        get => Component.GetFloat("offset_x");
        set => Component.SetFloat("offset_x", value);
    }
    /// <summary>Deslocamento vertical (u)</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float OffsetY
    {
        get => Component.GetFloat("offset_y");
        set => Component.SetFloat("offset_y", value);
    }
    /// <summary>Deslocamento tangente (u)</summary>
    /// <remarks>Faixa válida: -10000 a 10000.</remarks>
    public float OffsetZ
    {
        get => Component.GetFloat("offset_z");
        set => Component.SetFloat("offset_z", value);
    }
    /// <summary>Ativo</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Iniciar no Play</summary>
    public bool Autoplay
    {
        get => Component.GetBool("autoplay");
        set => Component.SetBool("autoplay", value);
    }
    /// <summary>Repetir percurso</summary>
    public bool Loop
    {
        get => Component.GetBool("loop");
        set => Component.SetBool("loop", value);
    }
    /// <summary>Sentido inverso</summary>
    public bool Backwards
    {
        get => Component.GetBool("backwards");
        set => Component.SetBool("backwards", value);
    }
    /// <summary>Orientar +Z</summary>
    public bool Orient
    {
        get => Component.GetBool("orient");
        set => Component.SetBool("orient", value);
    }
    public enum ModeOption : uint
    {
        VelocidadeMundial = 0,
        DuracaoDoPercurso = 1,
    }
    /// <summary>Avanço</summary>
    public ModeOption Mode
    {
        get => (ModeOption)Component.GetEnum("mode");
        set => Component.SetEnum("mode", (uint)value);
    }
    /// <summary>Caminho</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
    /// <summary>Reiniciar. Volta à distância inicial e avança</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Restart() => Component.Invoke("restart");
    /// <summary>Parar. Congela pose e progresso</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Stop() => Component.Invoke("stop");
    /// <summary>Progresso. Distância percorrida no mundo</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double Progress() => Component.Invoke("progress").AsNumber();
    /// <summary>Em movimento. Verdadeiro enquanto avança</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool Playing() => Component.Invoke("playing").AsBoolean();
}

/// <summary>Superfície de navegação: Assa a malha de navegação dos colisores estáticos e a carrega no Play. Família Navegação · Malha.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.ai.navigation@2.0/manual/NavMeshSurface.html</remarks>
public readonly struct NavSurface : IComponentFacade<NavSurface>
{
    public static string TypeId => "astra.navigation.surface";
    public static NavSurface Wrap(Component component) => new(component);
    public Component Component { get; }
    public NavSurface(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Centro do volume</summary>
    public Vector3 VolumeCenter
    {
        get => new(Component.GetFloat("volume_center_x"), Component.GetFloat("volume_center_y"), Component.GetFloat("volume_center_z"));
        set => Component.SetVector3("volume_center", value);
    }
    /// <summary>Tamanho do volume</summary>
    public Vector3 VolumeSize
    {
        get => new(Component.GetFloat("volume_size_x"), Component.GetFloat("volume_size_y"), Component.GetFloat("volume_size_z"));
        set => Component.SetVector3("volume_size", value);
    }
    /// <summary>Raio do agente (m). Distância mantida das paredes; a malha recua por este raio</summary>
    /// <remarks>Faixa válida: 0 a 50.</remarks>
    public float AgentRadius
    {
        get => Component.GetFloat("agent_radius");
        set => Component.SetFloat("agent_radius", value);
    }
    /// <summary>Altura do agente (m). Espaço livre mínimo acima do chão</summary>
    /// <remarks>Faixa válida: 0.05 a 100.</remarks>
    public float AgentHeight
    {
        get => Component.GetFloat("agent_height");
        set => Component.SetFloat("agent_height", value);
    }
    /// <summary>Altura do degrau (m). Desnível que o agente sobe sem link</summary>
    /// <remarks>Faixa válida: 0 a 50.</remarks>
    public float AgentMaxClimb
    {
        get => Component.GetFloat("agent_max_climb");
        set => Component.SetFloat("agent_max_climb", value);
    }
    /// <summary>Inclinação máxima (°). Rampas mais íngremes não entram na malha</summary>
    /// <remarks>Faixa válida: 0 a 89.</remarks>
    public float AgentMaxSlope
    {
        get => Component.GetFloat("agent_max_slope");
        set => Component.SetFloat("agent_max_slope", value);
    }
    /// <summary>Tamanho da célula (m). Menor detalhe horizontal; menor é mais preciso e mais lento</summary>
    /// <remarks>Faixa válida: 0.01 a 10.</remarks>
    public float CellSize
    {
        get => Component.GetFloat("cell_size");
        set => Component.SetFloat("cell_size", value);
    }
    /// <summary>Altura da célula (m). Menor detalhe vertical</summary>
    /// <remarks>Faixa válida: 0.01 a 10.</remarks>
    public float CellHeight
    {
        get => Component.GetFloat("cell_height");
        set => Component.SetFloat("cell_height", value);
    }
    /// <summary>Área mínima de região (m²). Ilhas menores são descartadas</summary>
    /// <remarks>Faixa válida: 0 a 10000.</remarks>
    public float MinRegionArea
    {
        get => Component.GetFloat("min_region_area");
        set => Component.SetFloat("min_region_area", value);
    }
    /// <summary>Tile (células). Lado do tile; obstáculos reconstroem só os tiles que tocam</summary>
    /// <remarks>Faixa válida: 16 a 128.</remarks>
    public float TileSize
    {
        get => Component.GetFloat("tile_size");
        set => Component.SetFloat("tile_size", value);
    }
    /// <summary>Aresta máxima (m). Zero não limita o comprimento das bordas</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float EdgeMaxLength
    {
        get => Component.GetFloat("edge_max_length");
        set => Component.SetFloat("edge_max_length", value);
    }
    /// <summary>Erro de borda (células). Quanto o contorno pode se afastar das paredes</summary>
    /// <remarks>Faixa válida: 0.1 a 10.</remarks>
    public float EdgeMaxError
    {
        get => Component.GetFloat("edge_max_error");
        set => Component.SetFloat("edge_max_error", value);
    }
    /// <summary>Amostra de detalhe (células). Menor que 1 desliga a malha de detalhe de altura</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float DetailSampleDistance
    {
        get => Component.GetFloat("detail_sample_distance");
        set => Component.SetFloat("detail_sample_distance", value);
    }
    /// <summary>Erro de detalhe (células)</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float DetailSampleMaxError
    {
        get => Component.GetFloat("detail_sample_max_error");
        set => Component.SetFloat("detail_sample_max_error", value);
    }
    /// <summary>Ativa. Desligada não carrega a malha no Play; o bake é preservado</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    public enum CollectOption : uint
    {
        TodaACena = 0,
        EsteObjetoEFilhos = 1,
        Volume = 2,
    }
    /// <summary>Coletar. Colisores estáticos que entram no bake</summary>
    public CollectOption Collect
    {
        get => (CollectOption)Component.GetEnum("collect");
        set => Component.SetEnum("collect", (uint)value);
    }
    public enum LayerOption : uint
    {
        Todas = 0,
        Camada0 = 1,
        Camada1 = 2,
        Camada2 = 3,
        Camada3 = 4,
        Camada4 = 5,
        Camada5 = 6,
        Camada6 = 7,
        Camada7 = 8,
        Camada8 = 9,
        Camada9 = 10,
        Camada10 = 11,
        Camada11 = 12,
        Camada12 = 13,
        Camada13 = 14,
        Camada14 = 15,
        Camada15 = 16,
        Camada16 = 17,
        Camada17 = 18,
        Camada18 = 19,
        Camada19 = 20,
        Camada20 = 21,
        Camada21 = 22,
        Camada22 = 23,
        Camada23 = 24,
        Camada24 = 25,
        Camada25 = 26,
        Camada26 = 27,
        Camada27 = 28,
        Camada28 = 29,
        Camada29 = 30,
        Camada30 = 31,
        Camada31 = 32,
    }
    /// <summary>Camada dos colisores. Todas ou só uma camada física do projeto</summary>
    public LayerOption Layer
    {
        get => (LayerOption)Component.GetEnum("layer");
        set => Component.SetEnum("layer", (uint)value);
    }
    /// <summary>Malha assada. Recurso do projeto por slot</summary>
    public AssetGuid GetData(uint slot = 0) => Component.GetResource("data", slot);
    public void SetData(AssetGuid value, uint slot = 0) => Component.SetResource("data", value, slot);
    /// <summary>Pronta. Verdadeiro quando a malha desta superfície está carregada no Play</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool IsReady() => Component.Invoke("is_ready").AsBoolean();
    /// <summary>Polígonos. Polígonos da malha carregada, com recortes de obstáculos</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public long PolygonCount() => Component.Invoke("polygon_count").AsInteger();
}

/// <summary>Agente de navegação: Anda pela malha até um destino, desviando de outros agentes. Família Navegação · Movimento.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-NavMeshAgent.html</remarks>
public readonly struct NavAgent : IComponentFacade<NavAgent>
{
    public static string TypeId => "astra.navigation.agent";
    public static NavAgent Wrap(Component component) => new(component);
    public Component Component { get; }
    public NavAgent(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Velocidade (m/s)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float Speed
    {
        get => Component.GetFloat("speed");
        set => Component.SetFloat("speed", value);
    }
    /// <summary>Aceleração (m/s²)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float Acceleration
    {
        get => Component.GetFloat("acceleration");
        set => Component.SetFloat("acceleration", value);
    }
    /// <summary>Giro (°/s). Velocidade de giro para a direção do movimento</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float AngularSpeed
    {
        get => Component.GetFloat("angular_speed");
        set => Component.SetFloat("angular_speed", value);
    }
    /// <summary>Distância de parada (m). Chega quando o caminho restante fica abaixo disto</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float StoppingDistance
    {
        get => Component.GetFloat("stopping_distance");
        set => Component.SetFloat("stopping_distance", value);
    }
    /// <summary>Raio (m). Usado no desvio entre agentes</summary>
    /// <remarks>Faixa válida: 0.05 a 50.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Altura (m)</summary>
    /// <remarks>Faixa válida: 0.05 a 100.</remarks>
    public float Height
    {
        get => Component.GetFloat("height");
        set => Component.SetFloat("height", value);
    }
    /// <summary>Deslocamento da base (m). Altura do pivô acima da malha quando o agente move o próprio objeto</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float BaseOffset
    {
        get => Component.GetFloat("base_offset");
        set => Component.SetFloat("base_offset", value);
    }
    /// <summary>Custo do salto (×)</summary>
    /// <remarks>Faixa válida: 1 a 1000.</remarks>
    public float JumpCost
    {
        get => Component.GetFloat("jump_cost");
        set => Component.SetFloat("jump_cost", value);
    }
    /// <summary>Custo da área difícil (×). Multiplica o comprimento percorrido nessa área</summary>
    /// <remarks>Faixa válida: 1 a 1000.</remarks>
    public float DifficultCost
    {
        get => Component.GetFloat("difficult_cost");
        set => Component.SetFloat("difficult_cost", value);
    }
    /// <summary>Refazer caminho a (m). O alvo precisa se mover isto para o caminho ser refeito</summary>
    /// <remarks>Faixa válida: 0.05 a 100.</remarks>
    public float RepathDistance
    {
        get => Component.GetFloat("repath_distance");
        set => Component.SetFloat("repath_distance", value);
    }
    /// <summary>Ativo. Desligado sai da multidão e para de mover o objeto</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Frear ao chegar. Desacelera perto do destino; desligado mantém a velocidade até a distância de parada</summary>
    public bool AutoBraking
    {
        get => Component.GetBool("auto_braking");
        set => Component.SetBool("auto_braking", value);
    }
    /// <summary>Girar para o movimento. Gira o objeto em Y para a direção da velocidade</summary>
    public bool UpdateRotation
    {
        get => Component.GetBool("update_rotation");
        set => Component.SetBool("update_rotation", value);
    }
    /// <summary>Usar links e áreas de salto</summary>
    public bool UseJump
    {
        get => Component.GetBool("use_jump");
        set => Component.SetBool("use_jump", value);
    }
    /// <summary>Atravessar área difícil</summary>
    public bool UseDifficult
    {
        get => Component.GetBool("use_difficult");
        set => Component.SetBool("use_difficult", value);
    }
    public enum AvoidanceOption : uint
    {
        Nenhum = 0,
        Baixo = 1,
        Medio = 2,
        Bom = 3,
        Alto = 4,
    }
    /// <summary>Desvio de agentes. Qualidade da amostragem de velocidades contra outros agentes</summary>
    public AvoidanceOption Avoidance
    {
        get => (AvoidanceOption)Component.GetEnum("avoidance");
        set => Component.SetEnum("avoidance", (uint)value);
    }
    /// <summary>Superfície. Malha usada pelo agente; vazio escolhe a superfície que contém o objeto</summary>
    public ObjectReference Surface
    {
        get => Component.GetReference("surface");
        set => Component.SetReference("surface", value);
    }
    /// <summary>Seguir objeto. Persegue o objeto sem script; SetDestination substitui o alvo</summary>
    public ObjectReference Target
    {
        get => Component.GetReference("target");
        set => Component.SetReference("target", value);
    }
    /// <summary>Ir para. Calcula o caminho e começa a andar; falso quando o ponto está longe da malha</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool SetDestination(Vector3 point) => Component.Invoke("set_destination", ComponentValue.Vector(point)).AsBoolean();
    /// <summary>Parar. Para no lugar e guarda o destino para Retomar</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Stop() => Component.Invoke("stop");
    /// <summary>Retomar. Volta ao último destino depois de Parar</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Resume() => Component.Invoke("resume");
    /// <summary>Teleportar. Move o objeto e o agente para o ponto mais próximo da malha</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool Warp(Vector3 point) => Component.Invoke("warp", ComponentValue.Vector(point)).AsBoolean();
    /// <summary>Distância restante. Comprimento do caminho até o destino; infinito sem caminho</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double RemainingDistance() => Component.Invoke("remaining_distance").AsNumber();
    /// <summary>Estado do caminho. 0 sem caminho, 1 completo, 2 parcial, 3 inválido</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public long PathStatus() => Component.Invoke("path_status").AsInteger();
    /// <summary>Tem caminho. Verdadeiro enquanto há um destino válido</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool HasPath() => Component.Invoke("has_path").AsBoolean();
    /// <summary>Em link. Verdadeiro durante a travessia de um Link</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool IsOnLink() => Component.Invoke("is_on_link").AsBoolean();
    /// <summary>Velocidade. Velocidade pedida pela multidão neste quadro</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public Vector3 Velocity() => Component.Invoke("velocity").AsVector3();
    /// <summary>Chegou. Emitido uma vez quando o caminho restante fica abaixo da distância de parada</summary>
    public ComponentSubscription OnDestinationReached(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "destination_reached", handler);
    /// <summary>Caminho falhou. Emitido quando o destino não tem caminho</summary>
    public ComponentSubscription OnPathFailed(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "path_failed", handler);
    /// <summary>Entrou no link. Emitido quando o agente começa a atravessar um Link</summary>
    public ComponentSubscription OnLinkEntered(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "link_entered", handler);
}

/// <summary>Obstáculo de navegação: Recorta a malha onde o objeto está, em tempo de execução. Família Navegação · Malha.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/class-NavMeshObstacle.html</remarks>
public readonly struct NavObstacle : IComponentFacade<NavObstacle>
{
    public static string TypeId => "astra.navigation.obstacle";
    public static NavObstacle Wrap(Component component) => new(component);
    public Component Component { get; }
    public NavObstacle(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Centro</summary>
    public Vector3 Center
    {
        get => new(Component.GetFloat("center_x"), Component.GetFloat("center_y"), Component.GetFloat("center_z"));
        set => Component.SetVector3("center", value);
    }
    /// <summary>Tamanho</summary>
    public Vector3 Size
    {
        get => new(Component.GetFloat("size_x"), Component.GetFloat("size_y"), Component.GetFloat("size_z"));
        set => Component.SetVector3("size", value);
    }
    /// <summary>Raio (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 500.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Altura (m)</summary>
    /// <remarks>Faixa válida: 0.01 a 1000.</remarks>
    public float Height
    {
        get => Component.GetFloat("height");
        set => Component.SetFloat("height", value);
    }
    /// <summary>Limiar de movimento (m). Deslocamento que conta como movimento</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float MoveThreshold
    {
        get => Component.GetFloat("move_threshold");
        set => Component.SetFloat("move_threshold", value);
    }
    /// <summary>Tempo até parado (s). Parado por este tempo volta a recortar</summary>
    /// <remarks>Faixa válida: 0 a 60.</remarks>
    public float StationaryTime
    {
        get => Component.GetFloat("stationary_time");
        set => Component.SetFloat("stationary_time", value);
    }
    /// <summary>Ativo. Desligado devolve a área recortada à malha</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Recortar só parado. Em movimento o recorte sai; ao parar ele volta (menos reconstruções)</summary>
    public bool CarveOnlyStationary
    {
        get => Component.GetBool("carve_only_stationary");
        set => Component.SetBool("carve_only_stationary", value);
    }
    public enum ShapeOption : uint
    {
        Caixa = 0,
        Cilindro = 1,
    }
    /// <summary>Forma. Caixa gira com o objeto em Y; cilindro fica de pé</summary>
    public ShapeOption Shape
    {
        get => (ShapeOption)Component.GetEnum("shape");
        set => Component.SetEnum("shape", (uint)value);
    }
    /// <summary>Recortando. Verdadeiro quando o recorte deste obstáculo está aplicado na malha</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool IsCarving() => Component.Invoke("is_carving").AsBoolean();
}

/// <summary>Link de navegação: Liga dois pontos da malha: salto, escada, vão ou porta. Família Navegação · Malha.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.ai.navigation@2.0/manual/NavMeshLink.html</remarks>
public readonly struct NavLink : IComponentFacade<NavLink>
{
    public static string TypeId => "astra.navigation.link";
    public static NavLink Wrap(Component component) => new(component);
    public Component Component { get; }
    public NavLink(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Início</summary>
    public Vector3 Start
    {
        get => new(Component.GetFloat("start_x"), Component.GetFloat("start_y"), Component.GetFloat("start_z"));
        set => Component.SetVector3("start", value);
    }
    /// <summary>Fim</summary>
    public Vector3 End
    {
        get => new(Component.GetFloat("end_x"), Component.GetFloat("end_y"), Component.GetFloat("end_z"));
        set => Component.SetVector3("end", value);
    }
    /// <summary>Raio de conexão (m). Distância da malha em que cada ponta se conecta</summary>
    /// <remarks>Faixa válida: 0.05 a 50.</remarks>
    public float Radius
    {
        get => Component.GetFloat("radius");
        set => Component.SetFloat("radius", value);
    }
    /// <summary>Ativo. Desligado remove a conexão da malha</summary>
    public bool Enabled
    {
        get => Component.GetBool("enabled");
        set => Component.SetBool("enabled", value);
    }
    /// <summary>Nos dois sentidos. Desligado só vai do início para o fim</summary>
    public bool Bidirectional
    {
        get => Component.GetBool("bidirectional");
        set => Component.SetBool("bidirectional", value);
    }
    public enum AreaOption : uint
    {
        Caminhavel = 0,
        Salto = 2,
        Dificil = 3,
    }
    /// <summary>Área. Salto exige que o agente use links e áreas de salto</summary>
    public AreaOption Area
    {
        get => (AreaOption)Component.GetEnum("area");
        set => Component.SetEnum("area", (uint)value);
    }
}

/// <summary>Modificador de navegação: Muda a área dos colisores deste objeto no bake ou os ignora. Família Navegação · Malha.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/Packages/com.unity.ai.navigation@2.0/manual/NavMeshModifier.html</remarks>
public readonly struct NavModifier : IComponentFacade<NavModifier>
{
    public static string TypeId => "astra.navigation.modifier";
    public static NavModifier Wrap(Component component) => new(component);
    public Component Component { get; }
    public NavModifier(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Aplicar aos filhos. Filhos sem modificador próprio herdam este</summary>
    public bool ApplyToChildren
    {
        get => Component.GetBool("apply_to_children");
        set => Component.SetBool("apply_to_children", value);
    }
    public enum ModeOption : uint
    {
        AlterarArea = 0,
        IgnorarNoBake = 1,
    }
    /// <summary>Modo. Ignorar tira os colisores do bake</summary>
    public ModeOption Mode
    {
        get => (ModeOption)Component.GetEnum("mode");
        set => Component.SetEnum("mode", (uint)value);
    }
    public enum AreaOption : uint
    {
        Caminhavel = 0,
        NaoCaminhavel = 1,
        Salto = 2,
        Dificil = 3,
    }
    /// <summary>Área. Área do chão destes colisores na malha</summary>
    public AreaOption Area
    {
        get => (AreaOption)Component.GetEnum("area");
        set => Component.SetEnum("area", (uint)value);
    }
}

/// <summary>Sistema de partículas: Emite, simula e desenha partículas: fogo, faíscas, fumaça, magia. Família Efeitos · Partículas.</summary>
/// <remarks>Referência estudada: https://docs.unity3d.com/6000.0/Documentation/Manual/ParticleSystemModules.html</remarks>
public readonly struct ParticleSystem : IComponentFacade<ParticleSystem>
{
    public static string TypeId => "astra.effects.particle_system";
    public static ParticleSystem Wrap(Component component) => new(component);
    public Component Component { get; }
    public ParticleSystem(Component component)
    {
        if (component.TypeId != TypeId) throw new WorldException(WorldStatus.InvalidArgument, "tipo " + component.TypeId);
        Component = component;
    }
    public ulong InstanceId => Component.InstanceId;
    public GameObject Object => Component.Object;
    public bool IsAlive => Component.IsAlive;
    /// <summary>Remove a instância pelo ciclo nativo; dependências e ownership podem recusar.</summary>
    public void Remove() => Component.Remove();
    /// <summary>Cor inicial. Cor linear RGB</summary>
    public Vector3 StartColor
    {
        get => new(Component.GetFloat("start_color_r"), Component.GetFloat("start_color_g"), Component.GetFloat("start_color_b"));
        set => Component.SetVector3("start_color", value);
    }
    /// <summary>Cor mínima. Cor linear RGB</summary>
    public Vector3 StartColorMin
    {
        get => new(Component.GetFloat("start_color_min_r"), Component.GetFloat("start_color_min_g"), Component.GetFloat("start_color_min_b"));
        set => Component.SetVector3("start_color_min", value);
    }
    /// <summary>Tamanho da caixa</summary>
    public Vector3 ShapeBox
    {
        get => new(Component.GetFloat("shape_box_x"), Component.GetFloat("shape_box_y"), Component.GetFloat("shape_box_z"));
        set => Component.SetVector3("shape_box", value);
    }
    /// <summary>Posição</summary>
    public Vector3 ShapePosition
    {
        get => new(Component.GetFloat("shape_position_x"), Component.GetFloat("shape_position_y"), Component.GetFloat("shape_position_z"));
        set => Component.SetVector3("shape_position", value);
    }
    /// <summary>Rotação</summary>
    public Vector3 ShapeRotation
    {
        get => new(Component.GetFloat("shape_rotation_x"), Component.GetFloat("shape_rotation_y"), Component.GetFloat("shape_rotation_z"));
        set => Component.SetVector3("shape_rotation", value);
    }
    /// <summary>Tinta. Cor linear RGB</summary>
    public Vector3 MaterialTint
    {
        get => new(Component.GetFloat("material_tint_r"), Component.GetFloat("material_tint_g"), Component.GetFloat("material_tint_b"));
        set => Component.SetVector3("material_tint", value);
    }
    /// <summary>Rolagem de UV</summary>
    public Vector2 MaterialScroll
    {
        get => new(Component.GetFloat("material_scroll_x"), Component.GetFloat("material_scroll_y"));
        set => Component.SetVector2("material_scroll", value);
    }
    /// <summary>Duração (s). Comprimento de um ciclo de emissão</summary>
    /// <remarks>Faixa válida: 0.05 a 3600.</remarks>
    public float Duration
    {
        get => Component.GetFloat("duration");
        set => Component.SetFloat("duration", value);
    }
    /// <summary>Atraso inicial (s). Espera antes de começar a emitir</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float StartDelay
    {
        get => Component.GetFloat("start_delay");
        set => Component.SetFloat("start_delay", value);
    }
    /// <summary>Atraso inicial mínimo (s). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float StartDelayMin
    {
        get => Component.GetFloat("start_delay_min");
        set => Component.SetFloat("start_delay_min", value);
    }
    /// <summary>Vida inicial (s). Quanto tempo cada partícula vive</summary>
    /// <remarks>Faixa válida: 0.01 a 3600.</remarks>
    public float StartLifetime
    {
        get => Component.GetFloat("start_lifetime");
        set => Component.SetFloat("start_lifetime", value);
    }
    /// <summary>Vida inicial mínimo (s). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: 0.01 a 3600.</remarks>
    public float StartLifetimeMin
    {
        get => Component.GetFloat("start_lifetime_min");
        set => Component.SetFloat("start_lifetime_min", value);
    }
    /// <summary>Vida inicial · multiplicador (s). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: 0.01 a 3600.</remarks>
    public float StartLifetimeMultiplier
    {
        get => Component.GetFloat("start_lifetime_multiplier");
        set => Component.SetFloat("start_lifetime_multiplier", value);
    }
    /// <summary>Velocidade inicial (m/s). Velocidade na direção da forma</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float StartSpeed
    {
        get => Component.GetFloat("start_speed");
        set => Component.SetFloat("start_speed", value);
    }
    /// <summary>Velocidade inicial mínimo (m/s). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float StartSpeedMin
    {
        get => Component.GetFloat("start_speed_min");
        set => Component.SetFloat("start_speed_min", value);
    }
    /// <summary>Velocidade inicial · multiplicador (m/s). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float StartSpeedMultiplier
    {
        get => Component.GetFloat("start_speed_multiplier");
        set => Component.SetFloat("start_speed_multiplier", value);
    }
    /// <summary>Tamanho inicial (m). Lado do quadrado da partícula</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float StartSize
    {
        get => Component.GetFloat("start_size");
        set => Component.SetFloat("start_size", value);
    }
    /// <summary>Tamanho inicial mínimo (m). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float StartSizeMin
    {
        get => Component.GetFloat("start_size_min");
        set => Component.SetFloat("start_size_min", value);
    }
    /// <summary>Tamanho inicial · multiplicador (m). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float StartSizeMultiplier
    {
        get => Component.GetFloat("start_size_multiplier");
        set => Component.SetFloat("start_size_multiplier", value);
    }
    /// <summary>Rotação inicial (°). Giro da partícula no plano da tela</summary>
    /// <remarks>Faixa válida: -36000 a 36000.</remarks>
    public float StartRotation
    {
        get => Component.GetFloat("start_rotation");
        set => Component.SetFloat("start_rotation", value);
    }
    /// <summary>Rotação inicial mínimo (°). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -36000 a 36000.</remarks>
    public float StartRotationMin
    {
        get => Component.GetFloat("start_rotation_min");
        set => Component.SetFloat("start_rotation_min", value);
    }
    /// <summary>Rotação inicial · multiplicador (°). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -36000 a 36000.</remarks>
    public float StartRotationMultiplier
    {
        get => Component.GetFloat("start_rotation_multiplier");
        set => Component.SetFloat("start_rotation_multiplier", value);
    }
    /// <summary>Opacidade inicial</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float StartColorA
    {
        get => Component.GetFloat("start_color_a");
        set => Component.SetFloat("start_color_a", value);
    }
    /// <summary>Opacidade mínima</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float StartColorMinA
    {
        get => Component.GetFloat("start_color_min_a");
        set => Component.SetFloat("start_color_min_a", value);
    }
    /// <summary>Gravidade (×). Fração da gravidade de 9,81 m/s² para baixo</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float GravityModifier
    {
        get => Component.GetFloat("gravity_modifier");
        set => Component.SetFloat("gravity_modifier", value);
    }
    /// <summary>Gravidade mínimo (×). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float GravityModifierMin
    {
        get => Component.GetFloat("gravity_modifier_min");
        set => Component.SetFloat("gravity_modifier_min", value);
    }
    /// <summary>Gravidade · multiplicador (×). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float GravityModifierMultiplier
    {
        get => Component.GetFloat("gravity_modifier_multiplier");
        set => Component.SetFloat("gravity_modifier_multiplier", value);
    }
    /// <summary>Velocidade da simulação (×)</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float SimulationSpeed
    {
        get => Component.GetFloat("simulation_speed");
        set => Component.SetFloat("simulation_speed", value);
    }
    /// <summary>Máximo de partículas. Teto autoral; o nível do aparelho pode reduzir sem apagar este valor</summary>
    /// <remarks>Faixa válida: 1 a 100000.</remarks>
    public float MaxParticles
    {
        get => Component.GetFloat("max_particles");
        set => Component.SetFloat("max_particles", value);
    }
    /// <summary>Semente. Mesma semente, mesma sequência de partículas</summary>
    /// <remarks>Faixa válida: 0 a 16777215.</remarks>
    public float RandomSeed
    {
        get => Component.GetFloat("random_seed");
        set => Component.SetFloat("random_seed", value);
    }
    /// <summary>Taxa por segundo (/s). Partículas novas por segundo</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float EmissionRate
    {
        get => Component.GetFloat("emission_rate");
        set => Component.SetFloat("emission_rate", value);
    }
    /// <summary>Taxa por segundo mínimo (/s). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float EmissionRateMin
    {
        get => Component.GetFloat("emission_rate_min");
        set => Component.SetFloat("emission_rate_min", value);
    }
    /// <summary>Taxa por segundo · multiplicador (/s). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float EmissionRateMultiplier
    {
        get => Component.GetFloat("emission_rate_multiplier");
        set => Component.SetFloat("emission_rate_multiplier", value);
    }
    /// <summary>Rajadas. Quantas rajadas o ciclo tem (até 4)</summary>
    /// <remarks>Faixa válida: 0 a 4.</remarks>
    public float BurstCount
    {
        get => Component.GetFloat("burst_count");
        set => Component.SetFloat("burst_count", value);
    }
    /// <summary>Rajada 1 · tempo (s). Momento no ciclo</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Burst0Time
    {
        get => Component.GetFloat("burst_0_time");
        set => Component.SetFloat("burst_0_time", value);
    }
    /// <summary>Rajada 1 · mínimo. Partículas, sorteadas até o máximo</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float Burst0CountMin
    {
        get => Component.GetFloat("burst_0_count_min");
        set => Component.SetFloat("burst_0_count_min", value);
    }
    /// <summary>Rajada 1 · máximo</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float Burst0CountMax
    {
        get => Component.GetFloat("burst_0_count_max");
        set => Component.SetFloat("burst_0_count_max", value);
    }
    /// <summary>Rajada 1 · ciclos. 0 repete enquanto emitir</summary>
    /// <remarks>Faixa válida: 0 a 10000.</remarks>
    public float Burst0Cycles
    {
        get => Component.GetFloat("burst_0_cycles");
        set => Component.SetFloat("burst_0_cycles", value);
    }
    /// <summary>Rajada 1 · intervalo (s). Entre ciclos da mesma rajada</summary>
    /// <remarks>Faixa válida: 0.01 a 3600.</remarks>
    public float Burst0Interval
    {
        get => Component.GetFloat("burst_0_interval");
        set => Component.SetFloat("burst_0_interval", value);
    }
    /// <summary>Rajada 1 · chance. Chance de cada ciclo emitir</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Burst0Probability
    {
        get => Component.GetFloat("burst_0_probability");
        set => Component.SetFloat("burst_0_probability", value);
    }
    /// <summary>Rajada 2 · tempo (s). Momento no ciclo</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Burst1Time
    {
        get => Component.GetFloat("burst_1_time");
        set => Component.SetFloat("burst_1_time", value);
    }
    /// <summary>Rajada 2 · mínimo. Partículas, sorteadas até o máximo</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float Burst1CountMin
    {
        get => Component.GetFloat("burst_1_count_min");
        set => Component.SetFloat("burst_1_count_min", value);
    }
    /// <summary>Rajada 2 · máximo</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float Burst1CountMax
    {
        get => Component.GetFloat("burst_1_count_max");
        set => Component.SetFloat("burst_1_count_max", value);
    }
    /// <summary>Rajada 2 · ciclos. 0 repete enquanto emitir</summary>
    /// <remarks>Faixa válida: 0 a 10000.</remarks>
    public float Burst1Cycles
    {
        get => Component.GetFloat("burst_1_cycles");
        set => Component.SetFloat("burst_1_cycles", value);
    }
    /// <summary>Rajada 2 · intervalo (s). Entre ciclos da mesma rajada</summary>
    /// <remarks>Faixa válida: 0.01 a 3600.</remarks>
    public float Burst1Interval
    {
        get => Component.GetFloat("burst_1_interval");
        set => Component.SetFloat("burst_1_interval", value);
    }
    /// <summary>Rajada 2 · chance. Chance de cada ciclo emitir</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Burst1Probability
    {
        get => Component.GetFloat("burst_1_probability");
        set => Component.SetFloat("burst_1_probability", value);
    }
    /// <summary>Rajada 3 · tempo (s). Momento no ciclo</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Burst2Time
    {
        get => Component.GetFloat("burst_2_time");
        set => Component.SetFloat("burst_2_time", value);
    }
    /// <summary>Rajada 3 · mínimo. Partículas, sorteadas até o máximo</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float Burst2CountMin
    {
        get => Component.GetFloat("burst_2_count_min");
        set => Component.SetFloat("burst_2_count_min", value);
    }
    /// <summary>Rajada 3 · máximo</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float Burst2CountMax
    {
        get => Component.GetFloat("burst_2_count_max");
        set => Component.SetFloat("burst_2_count_max", value);
    }
    /// <summary>Rajada 3 · ciclos. 0 repete enquanto emitir</summary>
    /// <remarks>Faixa válida: 0 a 10000.</remarks>
    public float Burst2Cycles
    {
        get => Component.GetFloat("burst_2_cycles");
        set => Component.SetFloat("burst_2_cycles", value);
    }
    /// <summary>Rajada 3 · intervalo (s). Entre ciclos da mesma rajada</summary>
    /// <remarks>Faixa válida: 0.01 a 3600.</remarks>
    public float Burst2Interval
    {
        get => Component.GetFloat("burst_2_interval");
        set => Component.SetFloat("burst_2_interval", value);
    }
    /// <summary>Rajada 3 · chance. Chance de cada ciclo emitir</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Burst2Probability
    {
        get => Component.GetFloat("burst_2_probability");
        set => Component.SetFloat("burst_2_probability", value);
    }
    /// <summary>Rajada 4 · tempo (s). Momento no ciclo</summary>
    /// <remarks>Faixa válida: 0 a 3600.</remarks>
    public float Burst3Time
    {
        get => Component.GetFloat("burst_3_time");
        set => Component.SetFloat("burst_3_time", value);
    }
    /// <summary>Rajada 4 · mínimo. Partículas, sorteadas até o máximo</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float Burst3CountMin
    {
        get => Component.GetFloat("burst_3_count_min");
        set => Component.SetFloat("burst_3_count_min", value);
    }
    /// <summary>Rajada 4 · máximo</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float Burst3CountMax
    {
        get => Component.GetFloat("burst_3_count_max");
        set => Component.SetFloat("burst_3_count_max", value);
    }
    /// <summary>Rajada 4 · ciclos. 0 repete enquanto emitir</summary>
    /// <remarks>Faixa válida: 0 a 10000.</remarks>
    public float Burst3Cycles
    {
        get => Component.GetFloat("burst_3_cycles");
        set => Component.SetFloat("burst_3_cycles", value);
    }
    /// <summary>Rajada 4 · intervalo (s). Entre ciclos da mesma rajada</summary>
    /// <remarks>Faixa válida: 0.01 a 3600.</remarks>
    public float Burst3Interval
    {
        get => Component.GetFloat("burst_3_interval");
        set => Component.SetFloat("burst_3_interval", value);
    }
    /// <summary>Rajada 4 · chance. Chance de cada ciclo emitir</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float Burst3Probability
    {
        get => Component.GetFloat("burst_3_probability");
        set => Component.SetFloat("burst_3_probability", value);
    }
    /// <summary>Raio (m). Na aresta, é meio comprimento</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ShapeRadius
    {
        get => Component.GetFloat("shape_radius");
        set => Component.SetFloat("shape_radius", value);
    }
    /// <summary>Espessura do raio. 0 emite da borda; 1 emite do volume inteiro</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float ShapeRadiusThickness
    {
        get => Component.GetFloat("shape_radius_thickness");
        set => Component.SetFloat("shape_radius_thickness", value);
    }
    /// <summary>Ângulo (°). Abertura do cone</summary>
    /// <remarks>Faixa válida: 0 a 90.</remarks>
    public float ShapeAngle
    {
        get => Component.GetFloat("shape_angle");
        set => Component.SetFloat("shape_angle", value);
    }
    /// <summary>Arco (°). Fatia da volta usada</summary>
    /// <remarks>Faixa válida: 0 a 360.</remarks>
    public float ShapeArc
    {
        get => Component.GetFloat("shape_arc");
        set => Component.SetFloat("shape_arc", value);
    }
    /// <summary>Comprimento (m). Altura do volume do cone</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ShapeLength
    {
        get => Component.GetFloat("shape_length");
        set => Component.SetFloat("shape_length", value);
    }
    /// <summary>Direção aleatória. Mistura a direção com uma direção sorteada</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float ShapeRandomDirection
    {
        get => Component.GetFloat("shape_random_direction");
        set => Component.SetFloat("shape_random_direction", value);
    }
    /// <summary>Direção esférica. Mistura a direção com a saída do centro</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float ShapeSpherizeDirection
    {
        get => Component.GetFloat("shape_spherize_direction");
        set => Component.SetFloat("shape_spherize_direction", value);
    }
    /// <summary>Tamanho no tempo (×)</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float SizeOverLifetime
    {
        get => Component.GetFloat("size_over_lifetime");
        set => Component.SetFloat("size_over_lifetime", value);
    }
    /// <summary>Tamanho no tempo mínimo (×). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float SizeOverLifetimeMin
    {
        get => Component.GetFloat("size_over_lifetime_min");
        set => Component.SetFloat("size_over_lifetime_min", value);
    }
    /// <summary>Tamanho no tempo · multiplicador (×). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float SizeOverLifetimeMultiplier
    {
        get => Component.GetFloat("size_over_lifetime_multiplier");
        set => Component.SetFloat("size_over_lifetime_multiplier", value);
    }
    /// <summary>Velocidade angular (°/s)</summary>
    /// <remarks>Faixa válida: -36000 a 36000.</remarks>
    public float RotationOverLifetime
    {
        get => Component.GetFloat("rotation_over_lifetime");
        set => Component.SetFloat("rotation_over_lifetime", value);
    }
    /// <summary>Velocidade angular mínimo (°/s). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -36000 a 36000.</remarks>
    public float RotationOverLifetimeMin
    {
        get => Component.GetFloat("rotation_over_lifetime_min");
        set => Component.SetFloat("rotation_over_lifetime_min", value);
    }
    /// <summary>Velocidade angular · multiplicador (°/s). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -36000 a 36000.</remarks>
    public float RotationOverLifetimeMultiplier
    {
        get => Component.GetFloat("rotation_over_lifetime_multiplier");
        set => Component.SetFloat("rotation_over_lifetime_multiplier", value);
    }
    /// <summary>Escala da velocidade (s). Comprimento extra por m/s</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float StretchVelocityScale
    {
        get => Component.GetFloat("stretch_velocity_scale");
        set => Component.SetFloat("stretch_velocity_scale", value);
    }
    /// <summary>Escala do comprimento (×). Comprimento em tamanhos de partícula</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float StretchLengthScale
    {
        get => Component.GetFloat("stretch_length_scale");
        set => Component.SetFloat("stretch_length_scale", value);
    }
    /// <summary>Ajuste de ordem (m). Desloca o sistema na ordem entre sistemas; positivo desenha antes (atrás)</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float SortingFudge
    {
        get => Component.GetFloat("sorting_fudge");
        set => Component.SetFloat("sorting_fudge", value);
    }
    /// <summary>Tamanho máximo na tela. Fração da altura da tela; limita o custo de partículas perto da câmera</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float MaxParticleSize
    {
        get => Component.GetFloat("max_particle_size");
        set => Component.SetFloat("max_particle_size", value);
    }
    /// <summary>Emissão (×). Intensidade HDR; acima de 1 alimenta o brilho estourado</summary>
    /// <remarks>Faixa válida: 0 a 64.</remarks>
    public float MaterialEmission
    {
        get => Component.GetFloat("material_emission");
        set => Component.SetFloat("material_emission", value);
    }
    /// <summary>Escala do ruído (×)</summary>
    /// <remarks>Faixa válida: 0.05 a 64.</remarks>
    public float MaterialNoiseScale
    {
        get => Component.GetFloat("material_noise_scale");
        set => Component.SetFloat("material_noise_scale", value);
    }
    /// <summary>Rolagem do ruído X (/s)</summary>
    /// <remarks>Faixa válida: -64 a 64.</remarks>
    public float MaterialNoiseScrollX
    {
        get => Component.GetFloat("material_noise_scroll_x");
        set => Component.SetFloat("material_noise_scroll_x", value);
    }
    /// <summary>Rolagem do ruído Y (/s)</summary>
    /// <remarks>Faixa válida: -64 a 64.</remarks>
    public float MaterialNoiseScrollY
    {
        get => Component.GetFloat("material_noise_scroll_y");
        set => Component.SetFloat("material_noise_scroll_y", value);
    }
    /// <summary>Recorte do ruído. Quanto o ruído apaga a opacidade</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float MaterialNoiseStrength
    {
        get => Component.GetFloat("material_noise_strength");
        set => Component.SetFloat("material_noise_strength", value);
    }
    /// <summary>Distorção do ruído. Quanto o ruído desloca a textura principal</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float MaterialNoiseDistortion
    {
        get => Component.GetFloat("material_noise_distortion");
        set => Component.SetFloat("material_noise_distortion", value);
    }
    /// <summary>Brilho central. Clareia o miolo denso, como fogo quente</summary>
    /// <remarks>Faixa válida: 0 a 16.</remarks>
    public float MaterialCenterGlow
    {
        get => Component.GetFloat("material_center_glow");
        set => Component.SetFloat("material_center_glow", value);
    }
    /// <summary>Concentração do brilho</summary>
    /// <remarks>Faixa válida: 1 a 32.</remarks>
    public float MaterialCenterGlowPower
    {
        get => Component.GetFloat("material_center_glow_power");
        set => Component.SetFloat("material_center_glow_power", value);
    }
    /// <summary>Partícula suave (m). Esmaece perto das superfícies; 0 desliga</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float MaterialSoftDistance
    {
        get => Component.GetFloat("material_soft_distance");
        set => Component.SetFloat("material_soft_distance", value);
    }
    /// <summary>Velocidade X (m/s). Somada à velocidade da partícula, sem acumular</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float VelocityX
    {
        get => Component.GetFloat("velocity_x");
        set => Component.SetFloat("velocity_x", value);
    }
    /// <summary>Velocidade X mínimo (m/s). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float VelocityXMin
    {
        get => Component.GetFloat("velocity_x_min");
        set => Component.SetFloat("velocity_x_min", value);
    }
    /// <summary>Velocidade X · multiplicador (m/s). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float VelocityXMultiplier
    {
        get => Component.GetFloat("velocity_x_multiplier");
        set => Component.SetFloat("velocity_x_multiplier", value);
    }
    /// <summary>Velocidade Y (m/s). Somada à velocidade da partícula, sem acumular</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float VelocityY
    {
        get => Component.GetFloat("velocity_y");
        set => Component.SetFloat("velocity_y", value);
    }
    /// <summary>Velocidade Y mínimo (m/s). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float VelocityYMin
    {
        get => Component.GetFloat("velocity_y_min");
        set => Component.SetFloat("velocity_y_min", value);
    }
    /// <summary>Velocidade Y · multiplicador (m/s). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float VelocityYMultiplier
    {
        get => Component.GetFloat("velocity_y_multiplier");
        set => Component.SetFloat("velocity_y_multiplier", value);
    }
    /// <summary>Velocidade Z (m/s). Somada à velocidade da partícula, sem acumular</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float VelocityZ
    {
        get => Component.GetFloat("velocity_z");
        set => Component.SetFloat("velocity_z", value);
    }
    /// <summary>Velocidade Z mínimo (m/s). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float VelocityZMin
    {
        get => Component.GetFloat("velocity_z_min");
        set => Component.SetFloat("velocity_z_min", value);
    }
    /// <summary>Velocidade Z · multiplicador (m/s). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float VelocityZMultiplier
    {
        get => Component.GetFloat("velocity_z_multiplier");
        set => Component.SetFloat("velocity_z_multiplier", value);
    }
    /// <summary>Modificador de velocidade (×). Speed Modifier: escala o deslocamento ao longo da vida</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float SpeedModifier
    {
        get => Component.GetFloat("speed_modifier");
        set => Component.SetFloat("speed_modifier", value);
    }
    /// <summary>Modificador de velocidade mínimo (×). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float SpeedModifierMin
    {
        get => Component.GetFloat("speed_modifier_min");
        set => Component.SetFloat("speed_modifier_min", value);
    }
    /// <summary>Modificador de velocidade · multiplicador (×). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float SpeedModifierMultiplier
    {
        get => Component.GetFloat("speed_modifier_multiplier");
        set => Component.SetFloat("speed_modifier_multiplier", value);
    }
    /// <summary>Velocidade máxima (m/s). Acima disso a partícula é freada</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float LimitSpeed
    {
        get => Component.GetFloat("limit_speed");
        set => Component.SetFloat("limit_speed", value);
    }
    /// <summary>Velocidade máxima mínimo (m/s). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float LimitSpeedMin
    {
        get => Component.GetFloat("limit_speed_min");
        set => Component.SetFloat("limit_speed_min", value);
    }
    /// <summary>Velocidade máxima · multiplicador (m/s). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float LimitSpeedMultiplier
    {
        get => Component.GetFloat("limit_speed_multiplier");
        set => Component.SetFloat("limit_speed_multiplier", value);
    }
    /// <summary>Amortecimento. Dampen: fração do excesso removida (1 corta no limite)</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float LimitDampen
    {
        get => Component.GetFloat("limit_dampen");
        set => Component.SetFloat("limit_dampen", value);
    }
    /// <summary>Arrasto (1/s). Drag: perda de velocidade por segundo, como resistência do ar</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float LimitDrag
    {
        get => Component.GetFloat("limit_drag");
        set => Component.SetFloat("limit_drag", value);
    }
    /// <summary>Força X (m/s²). Aceleração neste eixo</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float ForceX
    {
        get => Component.GetFloat("force_x");
        set => Component.SetFloat("force_x", value);
    }
    /// <summary>Força X mínimo (m/s²). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float ForceXMin
    {
        get => Component.GetFloat("force_x_min");
        set => Component.SetFloat("force_x_min", value);
    }
    /// <summary>Força X · multiplicador (m/s²). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float ForceXMultiplier
    {
        get => Component.GetFloat("force_x_multiplier");
        set => Component.SetFloat("force_x_multiplier", value);
    }
    /// <summary>Força Y (m/s²). Aceleração neste eixo</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float ForceY
    {
        get => Component.GetFloat("force_y");
        set => Component.SetFloat("force_y", value);
    }
    /// <summary>Força Y mínimo (m/s²). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float ForceYMin
    {
        get => Component.GetFloat("force_y_min");
        set => Component.SetFloat("force_y_min", value);
    }
    /// <summary>Força Y · multiplicador (m/s²). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float ForceYMultiplier
    {
        get => Component.GetFloat("force_y_multiplier");
        set => Component.SetFloat("force_y_multiplier", value);
    }
    /// <summary>Força Z (m/s²). Aceleração neste eixo</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float ForceZ
    {
        get => Component.GetFloat("force_z");
        set => Component.SetFloat("force_z", value);
    }
    /// <summary>Força Z mínimo (m/s²). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float ForceZMin
    {
        get => Component.GetFloat("force_z_min");
        set => Component.SetFloat("force_z_min", value);
    }
    /// <summary>Força Z · multiplicador (m/s²). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -1000 a 1000.</remarks>
    public float ForceZMultiplier
    {
        get => Component.GetFloat("force_z_multiplier");
        set => Component.SetFloat("force_z_multiplier", value);
    }
    /// <summary>Força do ruído (m/s). Velocidade máxima do desvio</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float NoiseStrength
    {
        get => Component.GetFloat("noise_strength");
        set => Component.SetFloat("noise_strength", value);
    }
    /// <summary>Força do ruído mínimo (m/s). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float NoiseStrengthMin
    {
        get => Component.GetFloat("noise_strength_min");
        set => Component.SetFloat("noise_strength_min", value);
    }
    /// <summary>Força do ruído · multiplicador (m/s). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: 0 a 100.</remarks>
    public float NoiseStrengthMultiplier
    {
        get => Component.GetFloat("noise_strength_multiplier");
        set => Component.SetFloat("noise_strength_multiplier", value);
    }
    /// <summary>Frequência (1/m). Tamanho do redemoinho: maior frequência, turbulência mais miúda</summary>
    /// <remarks>Faixa válida: 0.01 a 20.</remarks>
    public float NoiseFrequency
    {
        get => Component.GetFloat("noise_frequency");
        set => Component.SetFloat("noise_frequency", value);
    }
    /// <summary>Rolagem (1/s). Faz o campo de ruído mudar com o tempo</summary>
    /// <remarks>Faixa válida: -20 a 20.</remarks>
    public float NoiseScrollSpeed
    {
        get => Component.GetFloat("noise_scroll_speed");
        set => Component.SetFloat("noise_scroll_speed", value);
    }
    /// <summary>Oitavas. Camadas de detalhe; cada uma custa uma amostra a mais por partícula</summary>
    /// <remarks>Faixa válida: 1 a 4.</remarks>
    public float NoiseOctaves
    {
        get => Component.GetFloat("noise_octaves");
        set => Component.SetFloat("noise_octaves", value);
    }
    /// <summary>Amortecimento. Fração da velocidade ao longo do plano perdida no toque</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float CollisionDampen
    {
        get => Component.GetFloat("collision_dampen");
        set => Component.SetFloat("collision_dampen", value);
    }
    /// <summary>Quique. Fração da velocidade contra o plano devolvida (0 gruda, 1 quica inteiro)</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float CollisionBounce
    {
        get => Component.GetFloat("collision_bounce");
        set => Component.SetFloat("collision_bounce", value);
    }
    /// <summary>Perda de vida. Fração da vida consumida a cada toque (1 morre ao tocar)</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float CollisionLifetimeLoss
    {
        get => Component.GetFloat("collision_lifetime_loss");
        set => Component.SetFloat("collision_lifetime_loss", value);
    }
    /// <summary>Velocidade mínima (m/s). Depois do toque, abaixo disso a partícula morre</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float CollisionMinKillSpeed
    {
        get => Component.GetFloat("collision_min_kill_speed");
        set => Component.SetFloat("collision_min_kill_speed", value);
    }
    /// <summary>Escala do raio (×). Raio de colisão em relação ao tamanho da partícula</summary>
    /// <remarks>Faixa válida: 0 a 10.</remarks>
    public float CollisionRadiusScale
    {
        get => Component.GetFloat("collision_radius_scale");
        set => Component.SetFloat("collision_radius_scale", value);
    }
    /// <summary>Partículas ao nascer. Quantas o sub-sistema emite por partícula nascida</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float SubEmitterBirthCount
    {
        get => Component.GetFloat("sub_emitter_birth_count");
        set => Component.SetFloat("sub_emitter_birth_count", value);
    }
    /// <summary>Partículas ao colidir. Quantas o sub-sistema emite por toque</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float SubEmitterCollisionCount
    {
        get => Component.GetFloat("sub_emitter_collision_count");
        set => Component.SetFloat("sub_emitter_collision_count", value);
    }
    /// <summary>Partículas ao morrer. Quantas o sub-sistema emite por partícula morta</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float SubEmitterDeathCount
    {
        get => Component.GetFloat("sub_emitter_death_count");
        set => Component.SetFloat("sub_emitter_death_count", value);
    }
    /// <summary>Taxa por distância (/m). Rate over Distance: partículas por metro que o emissor anda (rastro de pegadas, fumaça de carro)</summary>
    /// <remarks>Faixa válida: 0 a 100000.</remarks>
    public float EmissionRateOverDistance
    {
        get => Component.GetFloat("emission_rate_over_distance");
        set => Component.SetFloat("emission_rate_over_distance", value);
    }
    /// <summary>Multiplicador (×). Parte da velocidade do emissor herdada no nascimento</summary>
    /// <remarks>Faixa válida: -100 a 100.</remarks>
    public float InheritVelocityMultiplier
    {
        get => Component.GetFloat("inherit_velocity_multiplier");
        set => Component.SetFloat("inherit_velocity_multiplier", value);
    }
    /// <summary>Tamanho (×). Fator sobre o tamanho inicial</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float SizeBySpeed
    {
        get => Component.GetFloat("size_by_speed");
        set => Component.SetFloat("size_by_speed", value);
    }
    /// <summary>Tamanho mínimo (×). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float SizeBySpeedMin
    {
        get => Component.GetFloat("size_by_speed_min");
        set => Component.SetFloat("size_by_speed_min", value);
    }
    /// <summary>Tamanho · multiplicador (×). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float SizeBySpeedMultiplier
    {
        get => Component.GetFloat("size_by_speed_multiplier");
        set => Component.SetFloat("size_by_speed_multiplier", value);
    }
    /// <summary>Faixa mínima (m/s). Velocidade que lê o início da curva</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float SizeBySpeedRangeMin
    {
        get => Component.GetFloat("size_by_speed_range_min");
        set => Component.SetFloat("size_by_speed_range_min", value);
    }
    /// <summary>Faixa máxima (m/s). Velocidade que lê o fim da curva</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float SizeBySpeedRangeMax
    {
        get => Component.GetFloat("size_by_speed_range_max");
        set => Component.SetFloat("size_by_speed_range_max", value);
    }
    /// <summary>Faixa mínima (m/s). Velocidade que lê o início da curva</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ColorBySpeedRangeMin
    {
        get => Component.GetFloat("color_by_speed_range_min");
        set => Component.SetFloat("color_by_speed_range_min", value);
    }
    /// <summary>Faixa máxima (m/s). Velocidade que lê o fim da curva</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ColorBySpeedRangeMax
    {
        get => Component.GetFloat("color_by_speed_range_max");
        set => Component.SetFloat("color_by_speed_range_max", value);
    }
    /// <summary>Velocidade angular (°/s). Giro por segundo</summary>
    /// <remarks>Faixa válida: -36000 a 36000.</remarks>
    public float RotationBySpeed
    {
        get => Component.GetFloat("rotation_by_speed");
        set => Component.SetFloat("rotation_by_speed", value);
    }
    /// <summary>Velocidade angular mínimo (°/s). Limite inferior do sorteio por partícula</summary>
    /// <remarks>Faixa válida: -36000 a 36000.</remarks>
    public float RotationBySpeedMin
    {
        get => Component.GetFloat("rotation_by_speed_min");
        set => Component.SetFloat("rotation_by_speed_min", value);
    }
    /// <summary>Velocidade angular · multiplicador (°/s). Escala aplicada à curva</summary>
    /// <remarks>Faixa válida: -36000 a 36000.</remarks>
    public float RotationBySpeedMultiplier
    {
        get => Component.GetFloat("rotation_by_speed_multiplier");
        set => Component.SetFloat("rotation_by_speed_multiplier", value);
    }
    /// <summary>Faixa mínima (m/s). Velocidade que lê o início da curva</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float RotationBySpeedRangeMin
    {
        get => Component.GetFloat("rotation_by_speed_range_min");
        set => Component.SetFloat("rotation_by_speed_range_min", value);
    }
    /// <summary>Faixa máxima (m/s). Velocidade que lê o fim da curva</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float RotationBySpeedRangeMax
    {
        get => Component.GetFloat("rotation_by_speed_range_max");
        set => Component.SetFloat("rotation_by_speed_range_max", value);
    }
    /// <summary>Raio do tubo (m). Grossura da rosca; o Raio é o do anel</summary>
    /// <remarks>Faixa válida: 0 a 1000.</remarks>
    public float ShapeDonutRadius
    {
        get => Component.GetFloat("shape_donut_radius");
        set => Component.SetFloat("shape_donut_radius", value);
    }
    /// <summary>Repetir. Recomeça o ciclo ao terminar a duração</summary>
    public bool Looping
    {
        get => Component.GetBool("looping");
        set => Component.SetBool("looping", value);
    }
    /// <summary>Pré-aquecer. Começa como se já tivesse rodado um ciclo inteiro</summary>
    public bool Prewarm
    {
        get => Component.GetBool("prewarm");
        set => Component.SetBool("prewarm", value);
    }
    /// <summary>Tocar ao iniciar. Começa sozinho ao entrar em Play e na prévia da cena</summary>
    public bool PlayOnAwake
    {
        get => Component.GetBool("play_on_awake");
        set => Component.SetBool("play_on_awake", value);
    }
    /// <summary>Semente automática. Semente derivada do objeto; desligue para fixar</summary>
    public bool AutoRandomSeed
    {
        get => Component.GetBool("auto_random_seed");
        set => Component.SetBool("auto_random_seed", value);
    }
    /// <summary>Emissão ativa</summary>
    public bool EmissionEnabled
    {
        get => Component.GetBool("emission_enabled");
        set => Component.SetBool("emission_enabled", value);
    }
    /// <summary>Forma ativa. Desligada: nascem no centro, para a frente (+Z)</summary>
    public bool ShapeEnabled
    {
        get => Component.GetBool("shape_enabled");
        set => Component.SetBool("shape_enabled", value);
    }
    /// <summary>Cor no tempo ativa. Multiplica a cor pela idade da partícula</summary>
    public bool ColorOverLifetimeEnabled
    {
        get => Component.GetBool("color_over_lifetime_enabled");
        set => Component.SetBool("color_over_lifetime_enabled", value);
    }
    /// <summary>Tamanho no tempo ativo. Multiplica o tamanho pela idade da partícula</summary>
    public bool SizeOverLifetimeEnabled
    {
        get => Component.GetBool("size_over_lifetime_enabled");
        set => Component.SetBool("size_over_lifetime_enabled", value);
    }
    /// <summary>Rotação no tempo ativa</summary>
    public bool RotationOverLifetimeEnabled
    {
        get => Component.GetBool("rotation_over_lifetime_enabled");
        set => Component.SetBool("rotation_over_lifetime_enabled", value);
    }
    /// <summary>Velocidade no tempo ativa. Velocity over Lifetime: soma uma velocidade ao longo da vida</summary>
    public bool VelocityOverLifetimeEnabled
    {
        get => Component.GetBool("velocity_over_lifetime_enabled");
        set => Component.SetBool("velocity_over_lifetime_enabled", value);
    }
    /// <summary>Limite de velocidade ativo. Limit Velocity over Lifetime: freia acima do limite e aplica arrasto</summary>
    public bool LimitVelocityEnabled
    {
        get => Component.GetBool("limit_velocity_enabled");
        set => Component.SetBool("limit_velocity_enabled", value);
    }
    /// <summary>Força no tempo ativa. Force over Lifetime: aceleração somada à velocidade (vento, sucção)</summary>
    public bool ForceOverLifetimeEnabled
    {
        get => Component.GetBool("force_over_lifetime_enabled");
        set => Component.SetBool("force_over_lifetime_enabled", value);
    }
    /// <summary>Ruído ativo. Noise: turbulência que desvia as partículas (neve, fumaça, magia)</summary>
    public bool NoiseEnabled
    {
        get => Component.GetBool("noise_enabled");
        set => Component.SetBool("noise_enabled", value);
    }
    /// <summary>Colisão ativa. Collision (Planes): partículas quicam em até dois planos da cena</summary>
    public bool CollisionEnabled
    {
        get => Component.GetBool("collision_enabled");
        set => Component.SetBool("collision_enabled", value);
    }
    /// <summary>Sub-emissores ativos. Sub Emitters: outro sistema emite quando a partícula nasce, colide ou morre</summary>
    public bool SubEmittersEnabled
    {
        get => Component.GetBool("sub_emitters_enabled");
        set => Component.SetBool("sub_emitters_enabled", value);
    }
    /// <summary>Herdar velocidade ativo. Inherit Velocity (Initial): a partícula nasce com a velocidade do emissor</summary>
    public bool InheritVelocityEnabled
    {
        get => Component.GetBool("inherit_velocity_enabled");
        set => Component.SetBool("inherit_velocity_enabled", value);
    }
    /// <summary>Tamanho pela velocidade ativo. Size by Speed: tamanho lido pela velocidade da partícula</summary>
    public bool SizeBySpeedEnabled
    {
        get => Component.GetBool("size_by_speed_enabled");
        set => Component.SetBool("size_by_speed_enabled", value);
    }
    /// <summary>Cor pela velocidade ativa. Color by Speed: cor lida pela velocidade da partícula</summary>
    public bool ColorBySpeedEnabled
    {
        get => Component.GetBool("color_by_speed_enabled");
        set => Component.SetBool("color_by_speed_enabled", value);
    }
    /// <summary>Rotação pela velocidade ativa. Rotation by Speed: velocidade angular lida pela velocidade da partícula</summary>
    public bool RotationBySpeedEnabled
    {
        get => Component.GetBool("rotation_by_speed_enabled");
        set => Component.SetBool("rotation_by_speed_enabled", value);
    }
    public enum StartDelayModeOption : uint
    {
        Constante = 0,
        EntreDuasConstantes = 2,
    }
    /// <summary>Atraso inicial · modo. Constante, curva no tempo, ou sorteio entre dois valores por partícula</summary>
    public StartDelayModeOption StartDelayMode
    {
        get => (StartDelayModeOption)Component.GetEnum("start_delay_mode");
        set => Component.SetEnum("start_delay_mode", (uint)value);
    }
    public enum StartLifetimeModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Vida inicial · modo. Constante, curva no tempo, ou sorteio entre dois valores por partícula</summary>
    public StartLifetimeModeOption StartLifetimeMode
    {
        get => (StartLifetimeModeOption)Component.GetEnum("start_lifetime_mode");
        set => Component.SetEnum("start_lifetime_mode", (uint)value);
    }
    public enum StartSpeedModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Velocidade inicial · modo. Constante, curva no tempo, ou sorteio entre dois valores por partícula</summary>
    public StartSpeedModeOption StartSpeedMode
    {
        get => (StartSpeedModeOption)Component.GetEnum("start_speed_mode");
        set => Component.SetEnum("start_speed_mode", (uint)value);
    }
    public enum StartSizeModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Tamanho inicial · modo. Constante, curva no tempo, ou sorteio entre dois valores por partícula</summary>
    public StartSizeModeOption StartSizeMode
    {
        get => (StartSizeModeOption)Component.GetEnum("start_size_mode");
        set => Component.SetEnum("start_size_mode", (uint)value);
    }
    public enum StartRotationModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Rotação inicial · modo. Constante, curva no tempo, ou sorteio entre dois valores por partícula</summary>
    public StartRotationModeOption StartRotationMode
    {
        get => (StartRotationModeOption)Component.GetEnum("start_rotation_mode");
        set => Component.SetEnum("start_rotation_mode", (uint)value);
    }
    public enum StartColorModeOption : uint
    {
        Cor = 0,
        Gradiente = 1,
        EntreDuasCores = 2,
        EntreDoisGradientes = 3,
        CorAleatoria = 4,
    }
    /// <summary>Cor inicial · modo. Cor fixa, gradiente no tempo do ciclo, sorteio entre duas cores ou dois gradientes</summary>
    public StartColorModeOption StartColorMode
    {
        get => (StartColorModeOption)Component.GetEnum("start_color_mode");
        set => Component.SetEnum("start_color_mode", (uint)value);
    }
    public enum GravityModifierModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Gravidade · modo. Constante, curva no tempo, ou sorteio entre dois valores por partícula</summary>
    public GravityModifierModeOption GravityModifierMode
    {
        get => (GravityModifierModeOption)Component.GetEnum("gravity_modifier_mode");
        set => Component.SetEnum("gravity_modifier_mode", (uint)value);
    }
    public enum SimulationSpaceOption : uint
    {
        Local = 0,
        Mundo = 1,
    }
    /// <summary>Espaço. Local: as partículas acompanham o objeto. Mundo: ficam onde nasceram</summary>
    public SimulationSpaceOption SimulationSpace
    {
        get => (SimulationSpaceOption)Component.GetEnum("simulation_space");
        set => Component.SetEnum("simulation_space", (uint)value);
    }
    public enum StopActionOption : uint
    {
        Nenhuma = 0,
        DesativarObjeto = 1,
        DestruirObjeto = 2,
        EventoParou = 3,
    }
    /// <summary>Ao parar. O que acontece quando o sistema termina e não há partículas vivas</summary>
    public StopActionOption StopAction
    {
        get => (StopActionOption)Component.GetEnum("stop_action");
        set => Component.SetEnum("stop_action", (uint)value);
    }
    public enum EmissionRateModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Taxa por segundo · modo. Constante, curva no tempo, ou sorteio entre dois valores por partícula</summary>
    public EmissionRateModeOption EmissionRateMode
    {
        get => (EmissionRateModeOption)Component.GetEnum("emission_rate_mode");
        set => Component.SetEnum("emission_rate_mode", (uint)value);
    }
    public enum ShapeTypeOption : uint
    {
        Esfera = 0,
        Hemisferio = 1,
        Cone = 2,
        Caixa = 3,
        Circulo = 4,
        Aresta = 5,
        Rosca = 6,
        Retangulo = 7,
    }
    /// <summary>Forma</summary>
    public ShapeTypeOption ShapeType
    {
        get => (ShapeTypeOption)Component.GetEnum("shape_type");
        set => Component.SetEnum("shape_type", (uint)value);
    }
    public enum ShapeConeEmitFromOption : uint
    {
        Base = 0,
        Volume = 1,
    }
    /// <summary>Emitir do</summary>
    public ShapeConeEmitFromOption ShapeConeEmitFrom
    {
        get => (ShapeConeEmitFromOption)Component.GetEnum("shape_cone_emit_from");
        set => Component.SetEnum("shape_cone_emit_from", (uint)value);
    }
    public enum ShapeBoxEmitFromOption : uint
    {
        Volume = 0,
        Casca = 1,
        Arestas = 2,
    }
    /// <summary>Emitir da</summary>
    public ShapeBoxEmitFromOption ShapeBoxEmitFrom
    {
        get => (ShapeBoxEmitFromOption)Component.GetEnum("shape_box_emit_from");
        set => Component.SetEnum("shape_box_emit_from", (uint)value);
    }
    public enum ColorOverLifetimeModeOption : uint
    {
        Gradiente = 1,
        EntreDoisGradientes = 3,
    }
    /// <summary>Cor no tempo · modo</summary>
    public ColorOverLifetimeModeOption ColorOverLifetimeMode
    {
        get => (ColorOverLifetimeModeOption)Component.GetEnum("color_over_lifetime_mode");
        set => Component.SetEnum("color_over_lifetime_mode", (uint)value);
    }
    public enum SizeOverLifetimeModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Tamanho no tempo · modo. Constante, curva no tempo, ou sorteio entre dois valores por partícula</summary>
    public SizeOverLifetimeModeOption SizeOverLifetimeMode
    {
        get => (SizeOverLifetimeModeOption)Component.GetEnum("size_over_lifetime_mode");
        set => Component.SetEnum("size_over_lifetime_mode", (uint)value);
    }
    public enum RotationOverLifetimeModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Velocidade angular · modo. Constante, curva no tempo, ou sorteio entre dois valores por partícula</summary>
    public RotationOverLifetimeModeOption RotationOverLifetimeMode
    {
        get => (RotationOverLifetimeModeOption)Component.GetEnum("rotation_over_lifetime_mode");
        set => Component.SetEnum("rotation_over_lifetime_mode", (uint)value);
    }
    public enum RenderModeOption : uint
    {
        Billboard = 0,
        BillboardEsticado = 1,
    }
    /// <summary>Modo. Billboard encara a câmera; esticado alonga na direção da velocidade</summary>
    public RenderModeOption RenderMode
    {
        get => (RenderModeOption)Component.GetEnum("render_mode");
        set => Component.SetEnum("render_mode", (uint)value);
    }
    public enum SortModeOption : uint
    {
        Nenhuma = 0,
        PorDistancia = 1,
        MaisAntigasNaFrente = 2,
        MaisNovasNaFrente = 3,
    }
    /// <summary>Ordenação. Ordem das partículas deste sistema na mistura</summary>
    public SortModeOption SortMode
    {
        get => (SortModeOption)Component.GetEnum("sort_mode");
        set => Component.SetEnum("sort_mode", (uint)value);
    }
    public enum MaterialBlendOption : uint
    {
        Mistura = 0,
        Aditivo = 1,
    }
    /// <summary>Mistura. Mistura cobre o fundo pela opacidade; Aditivo soma luz (fogo, magia)</summary>
    public MaterialBlendOption MaterialBlend
    {
        get => (MaterialBlendOption)Component.GetEnum("material_blend");
        set => Component.SetEnum("material_blend", (uint)value);
    }
    public enum MaterialTextureOption : uint
    {
        PontoSuave = 0,
        Brilho = 1,
        Chama = 2,
        Fumaca = 3,
        Faisca = 4,
        Anel = 5,
        Estrela = 6,
        Quadrado = 7,
    }
    /// <summary>Textura. Textura própria da engine</summary>
    public MaterialTextureOption MaterialTexture
    {
        get => (MaterialTextureOption)Component.GetEnum("material_texture");
        set => Component.SetEnum("material_texture", (uint)value);
    }
    public enum MaterialNoiseOption : uint
    {
        Nenhum = 0,
        Nuvens = 1,
        Celulas = 2,
        Faixas = 3,
    }
    /// <summary>Ruído. Textura de ruído que recorta e deforma a principal</summary>
    public MaterialNoiseOption MaterialNoise
    {
        get => (MaterialNoiseOption)Component.GetEnum("material_noise");
        set => Component.SetEnum("material_noise", (uint)value);
    }
    public enum MaterialMaskOption : uint
    {
        Nenhuma = 0,
        Radial = 1,
        DesvaneceParaCima = 2,
        Anel = 3,
    }
    /// <summary>Máscara. Multiplica a opacidade por uma forma fixa na partícula</summary>
    public MaterialMaskOption MaterialMask
    {
        get => (MaterialMaskOption)Component.GetEnum("material_mask");
        set => Component.SetEnum("material_mask", (uint)value);
    }
    public enum VelocityXModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Velocidade X · modo. Constante, curva ao longo da vida, ou sorteio entre dois valores por partícula</summary>
    public VelocityXModeOption VelocityXMode
    {
        get => (VelocityXModeOption)Component.GetEnum("velocity_x_mode");
        set => Component.SetEnum("velocity_x_mode", (uint)value);
    }
    public enum VelocityYModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Velocidade Y · modo. Constante, curva ao longo da vida, ou sorteio entre dois valores por partícula</summary>
    public VelocityYModeOption VelocityYMode
    {
        get => (VelocityYModeOption)Component.GetEnum("velocity_y_mode");
        set => Component.SetEnum("velocity_y_mode", (uint)value);
    }
    public enum VelocityZModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Velocidade Z · modo. Constante, curva ao longo da vida, ou sorteio entre dois valores por partícula</summary>
    public VelocityZModeOption VelocityZMode
    {
        get => (VelocityZModeOption)Component.GetEnum("velocity_z_mode");
        set => Component.SetEnum("velocity_z_mode", (uint)value);
    }
    public enum VelocitySpaceOption : uint
    {
        Local = 0,
        Mundo = 1,
    }
    /// <summary>Espaço da velocidade. Local: eixos do objeto. Mundo: eixos da cena</summary>
    public VelocitySpaceOption VelocitySpace
    {
        get => (VelocitySpaceOption)Component.GetEnum("velocity_space");
        set => Component.SetEnum("velocity_space", (uint)value);
    }
    public enum SpeedModifierModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Modificador de velocidade · modo. Constante, curva ao longo da vida, ou sorteio entre dois valores por partícula</summary>
    public SpeedModifierModeOption SpeedModifierMode
    {
        get => (SpeedModifierModeOption)Component.GetEnum("speed_modifier_mode");
        set => Component.SetEnum("speed_modifier_mode", (uint)value);
    }
    public enum LimitSpeedModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Velocidade máxima · modo. Constante, curva ao longo da vida, ou sorteio entre dois valores por partícula</summary>
    public LimitSpeedModeOption LimitSpeedMode
    {
        get => (LimitSpeedModeOption)Component.GetEnum("limit_speed_mode");
        set => Component.SetEnum("limit_speed_mode", (uint)value);
    }
    public enum ForceXModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Força X · modo. Constante, curva ao longo da vida, ou sorteio entre dois valores por partícula</summary>
    public ForceXModeOption ForceXMode
    {
        get => (ForceXModeOption)Component.GetEnum("force_x_mode");
        set => Component.SetEnum("force_x_mode", (uint)value);
    }
    public enum ForceYModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Força Y · modo. Constante, curva ao longo da vida, ou sorteio entre dois valores por partícula</summary>
    public ForceYModeOption ForceYMode
    {
        get => (ForceYModeOption)Component.GetEnum("force_y_mode");
        set => Component.SetEnum("force_y_mode", (uint)value);
    }
    public enum ForceZModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Força Z · modo. Constante, curva ao longo da vida, ou sorteio entre dois valores por partícula</summary>
    public ForceZModeOption ForceZMode
    {
        get => (ForceZModeOption)Component.GetEnum("force_z_mode");
        set => Component.SetEnum("force_z_mode", (uint)value);
    }
    public enum ForceSpaceOption : uint
    {
        Local = 0,
        Mundo = 1,
    }
    /// <summary>Espaço da força. Mundo: o vento não gira com o objeto</summary>
    public ForceSpaceOption ForceSpace
    {
        get => (ForceSpaceOption)Component.GetEnum("force_space");
        set => Component.SetEnum("force_space", (uint)value);
    }
    public enum NoiseStrengthModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Força do ruído · modo. Constante, curva ao longo da vida, ou sorteio entre dois valores por partícula</summary>
    public NoiseStrengthModeOption NoiseStrengthMode
    {
        get => (NoiseStrengthModeOption)Component.GetEnum("noise_strength_mode");
        set => Component.SetEnum("noise_strength_mode", (uint)value);
    }
    public enum SizeBySpeedModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Tamanho · modo. Constante, curva pela velocidade, ou sorteio entre dois valores</summary>
    public SizeBySpeedModeOption SizeBySpeedMode
    {
        get => (SizeBySpeedModeOption)Component.GetEnum("size_by_speed_mode");
        set => Component.SetEnum("size_by_speed_mode", (uint)value);
    }
    public enum ColorBySpeedModeOption : uint
    {
        Gradiente = 1,
        EntreDoisGradientes = 3,
    }
    /// <summary>Cor · modo. Gradiente lido pela velocidade, ou sorteio entre dois</summary>
    public ColorBySpeedModeOption ColorBySpeedMode
    {
        get => (ColorBySpeedModeOption)Component.GetEnum("color_by_speed_mode");
        set => Component.SetEnum("color_by_speed_mode", (uint)value);
    }
    public enum RotationBySpeedModeOption : uint
    {
        Constante = 0,
        Curva = 1,
        EntreDuasConstantes = 2,
        EntreDuasCurvas = 3,
    }
    /// <summary>Velocidade angular · modo. Constante, curva pela velocidade, ou sorteio entre dois valores</summary>
    public RotationBySpeedModeOption RotationBySpeedMode
    {
        get => (RotationBySpeedModeOption)Component.GetEnum("rotation_by_speed_mode");
        set => Component.SetEnum("rotation_by_speed_mode", (uint)value);
    }
    /// <summary>Plano 1. O +Y do objeto é a normal; a partícula quica do lado de cima</summary>
    public ObjectReference CollisionPlane0
    {
        get => Component.GetReference("collision_plane_0");
        set => Component.SetReference("collision_plane_0", value);
    }
    /// <summary>Plano 2. Segundo plano (parede, teto)</summary>
    public ObjectReference CollisionPlane1
    {
        get => Component.GetReference("collision_plane_1");
        set => Component.SetReference("collision_plane_1", value);
    }
    /// <summary>Ao nascer. Sistema que emite onde cada partícula nasce</summary>
    public ObjectReference SubEmitterBirth
    {
        get => Component.GetReference("sub_emitter_birth");
        set => Component.SetReference("sub_emitter_birth", value);
    }
    /// <summary>Ao colidir. Sistema que emite no ponto do toque (respingo, faísca)</summary>
    public ObjectReference SubEmitterCollision
    {
        get => Component.GetReference("sub_emitter_collision");
        set => Component.SetReference("sub_emitter_collision", value);
    }
    /// <summary>Ao morrer. Sistema que emite onde cada partícula morre (fogos)</summary>
    public ObjectReference SubEmitterDeath
    {
        get => Component.GetReference("sub_emitter_death");
        set => Component.SetReference("sub_emitter_death", value);
    }
    /// <summary>Tocar. Inicia ou retoma a simulação e a emissão</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Play() => Component.Invoke("play");
    /// <summary>Parar. Para de emitir; as partículas vivas terminam a vida</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Stop() => Component.Invoke("stop");
    /// <summary>Parar e limpar. Para de emitir e remove todas as partículas</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void StopAndClear() => Component.Invoke("stop_and_clear");
    /// <summary>Pausar. Congela simulação e emissão</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Pause() => Component.Invoke("pause");
    /// <summary>Limpar. Remove as partículas vivas sem mudar o estado de reprodução</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Clear() => Component.Invoke("clear");
    /// <summary>Emitir. Emite partículas agora, pela forma, mesmo parado</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public void Emit(long count) => Component.Invoke("emit", ComponentValue.Integer(count));
    /// <summary>Tocando. Verdadeiro enquanto toca (emitindo ou com partículas vivas)</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public bool IsPlaying() => Component.Invoke("is_playing").AsBoolean();
    /// <summary>Partículas vivas. Quantas partículas estão vivas neste quadro</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public long ParticleCount() => Component.Invoke("particle_count").AsInteger();
    /// <summary>Tempo. Tempo do ciclo atual em segundos</summary>
    /// <remarks>Executado no mundo de Play; fora dele lança WorldException NotRunning.</remarks>
    public double Time() => Component.Invoke("time").AsNumber();
    /// <summary>Parou. O sistema terminou: não emite mais e não há partículas vivas (Ação ao parar = Evento Parou)</summary>
    public ComponentSubscription OnStopped(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "stopped", handler);
}
