using System.Numerics;

namespace Astra;

/// <summary>Position-only follow behavior evaluated after animation and physics.
/// Camera rotation remains under authoring, CameraLook, or scripts.</summary>
public readonly struct CameraFollowRig(Component component)
{
    public ObjectReference Target
    {
        get => component.GetReference("target");
        set => component.SetReference("target", value);
    }
    public Vector3 Offset
    {
        get => new(component.GetFloat("offset_x"), component.GetFloat("offset_y"), component.GetFloat("offset_z"));
        set
        {
            if (!float.IsFinite(value.X) || !float.IsFinite(value.Y) || !float.IsFinite(value.Z) ||
                MathF.Abs(value.X) > 10000 || MathF.Abs(value.Y) > 10000 || MathF.Abs(value.Z) > 10000)
                throw new ArgumentOutOfRangeException(nameof(value));
            component.SetVector3("offset", value);
        }
    }
    public float DampingSeconds
    {
        get => component.GetFloat("damping_seconds");
        set => component.SetFloat("damping_seconds", value);
    }
    public bool Enabled
    {
        get => component.GetBool("enabled");
        set => component.SetBool("enabled", value);
    }
}
