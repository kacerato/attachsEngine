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
    /// <summary>Cor do zênite. Cor linear RGB</summary>
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
    /// <summary>Alta qualidade. Aumenta as amostras de vista e luz; custa mais GPU</summary>
    public bool PhysicalAtmosphereHighQuality
    {
        get => Component.GetBool("physical_atmosphere_high_quality");
        set => Component.SetBool("physical_atmosphere_high_quality", value);
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
    /// <summary>Cor linear. Cor linear RGB</summary>
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
    /// <summary>Meio-cone interno (°)</summary>
    /// <remarks>Faixa válida: 0 a 89.</remarks>
    public float InnerAngle
    {
        get => Component.GetFloat("inner_angle");
        set => Component.SetFloat("inner_angle", value);
    }
    /// <summary>Meio-cone externo (°)</summary>
    /// <remarks>Faixa válida: 0 a 89.</remarks>
    public float OuterAngle
    {
        get => Component.GetFloat("outer_angle");
        set => Component.SetFloat("outer_angle", value);
    }
    /// <summary>Força da sombra. 1 é sombra opaca; abaixo disso a luz vaza pelo oclusor</summary>
    /// <remarks>Faixa válida: 0 a 1.</remarks>
    public float ShadowStrength
    {
        get => Component.GetFloat("shadow_strength");
        set => Component.SetFloat("shadow_strength", value);
    }
    /// <summary>Desvio (texel). Afasta a comparação de profundidade e some com a acne</summary>
    /// <remarks>Faixa válida: 0 a 2.</remarks>
    public float ShadowBias
    {
        get => Component.GetFloat("shadow_bias");
        set => Component.SetFloat("shadow_bias", value);
    }
    /// <summary>Desvio na normal (texel). Desloca a amostra ao longo da normal; some com o serrilhado da borda</summary>
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
    /// <summary>Modalidade. Tipo de emissão da luz</summary>
    public KindOption Kind
    {
        get => (KindOption)Component.GetEnum("kind");
        set => Component.SetEnum("kind", (uint)value);
    }
    public enum UnitOption : uint
    {
        InternaLegada = 0,
        LuxCandela = 1,
        LuxLumen = 2,
    }
    /// <summary>Unidade. Direcional usa lux; luz local usa candela ou lúmen</summary>
    public UnitOption Unit
    {
        get => (UnitOption)Component.GetEnum("unit");
        set => Component.SetEnum("unit", (uint)value);
    }
    public enum ShadowModeOption : uint
    {
        Nenhuma = 0,
        Dura = 1,
        Suave = 2,
    }
    /// <summary>Sombra. Projeção de sombra desta luz no atlas local</summary>
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
    /// <summary>Entrou no estado. Emitido quando um estado começa (no início da transição para ele). Payload: Camada: inteiro, Estado: inteiro</summary>
    public ComponentSubscription OnStateEntered(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "state_entered", handler);
    /// <summary>Evento do estado. Emitido quando o tempo do estado passa por um evento marcado nele. Payload: Camada: inteiro, Estado: inteiro, Marca: inteiro</summary>
    public ComponentSubscription OnStateEvent(Behavior owner, Action<ComponentEventArgs> handler) => owner.Connect(Component, "state_event", handler);
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
