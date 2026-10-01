using System.Numerics;

namespace Astra;

/// <summary>Frame in which a displacement or rotation is expressed.</summary>
public enum TransformSpace { Local, World }

public sealed partial class GameObject
{
    public Quaternion LocalRotation
    {
        get => LocalTransform.Rotation;
        set { var t = LocalTransform; LocalTransform = t with { Rotation = UnitRotation(value) }; }
    }
    public Vector3 LocalScale
    {
        get => LocalTransform.Scale;
        set { Finite(value); var t = LocalTransform; LocalTransform = t with { Scale = value }; }
    }
    public Vector3 WorldPosition
    {
        get { var position = LocalToWorldMatrix.Translation; Finite(position); return position; }
        set
        {
            Finite(value);
            var parent = Parent;
            var local = LocalTransform;
            LocalTransform = local with { Position = parent is null ? value : parent.InverseTransformPoint(value) };
        }
    }
    public Quaternion WorldRotation
    {
        get => HierarchyRotation();
        set
        {
            var rotation = UnitRotation(value);
            var parent = Parent;
            var local = LocalTransform;
            LocalTransform = local with { Rotation = parent is null ? rotation :
                UnitRotation(Quaternion.Conjugate(parent.HierarchyRotation()) * rotation) };
        }
    }

    /// <summary>Full affine matrix, including shear caused by rotated nonuniform parents.
    /// System.Numerics uses row vectors: local * parent * ancestor.
    /// O(depth) native reads and a cycle-detection set; capture once for batches of points.</summary>
    public Matrix4x4 LocalToWorldMatrix
    {
        get
        {
            var matrix = Matrix4x4.Identity;
            var seen = new HashSet<ulong>();
            for (GameObject? node = this; node is not null; node = node.Parent)
            {
                if (!seen.Add(node.ObjectId)) throw new InvalidOperationException("Cycle in transform hierarchy.");
                var pose = node.LocalTransform;
                matrix *= Matrix4x4.CreateScale(pose.Scale) *
                          Matrix4x4.CreateFromQuaternion(UnitRotation(pose.Rotation)) *
                          Matrix4x4.CreateTranslation(pose.Position);
                if (!IsFiniteMatrix(matrix)) throw new InvalidOperationException("Transform hierarchy exceeds finite affine range.");
            }
            return matrix;
        }
    }

    public Matrix4x4 WorldToLocalMatrix
    {
        get => Matrix4x4.Invert(LocalToWorldMatrix, out var inverse) && IsFiniteMatrix(inverse)
            ? inverse : throw new InvalidOperationException("Transform has a singular scale; inverse space is undefined.");
    }

    /// <summary>Position conversion includes scale and translation (Unity 6000.0 TransformPoint).</summary>
    public Vector3 TransformPoint(Vector3 point) { Finite(point); return CheckedResult(Vector3.Transform(point, LocalToWorldMatrix)); }
    public Vector3 InverseTransformPoint(Vector3 point) { Finite(point); return CheckedResult(Vector3.Transform(point, WorldToLocalMatrix)); }
    /// <summary>Vector conversion includes scale, but excludes translation.</summary>
    public Vector3 TransformVector(Vector3 vector) { Finite(vector); return CheckedResult(Vector3.TransformNormal(vector, LocalToWorldMatrix)); }
    public Vector3 InverseTransformVector(Vector3 vector) { Finite(vector); return CheckedResult(Vector3.TransformNormal(vector, WorldToLocalMatrix)); }
    /// <summary>Direction conversion ignores scale and translation, preserving length.</summary>
    public Vector3 TransformDirection(Vector3 direction) { Finite(direction); return CheckedResult(Vector3.Transform(direction, HierarchyRotation())); }
    public Vector3 InverseTransformDirection(Vector3 direction) { Finite(direction); return CheckedResult(Vector3.Transform(direction, Quaternion.Conjugate(HierarchyRotation()))); }

    /// <summary>World axes of the object's local +X, +Y and +Z. Cameras may use a different viewing axis.</summary>
    public Vector3 Right => TransformDirection(Vector3.UnitX);
    public Vector3 Up => TransformDirection(Vector3.UnitY);
    public Vector3 Forward => TransformDirection(Vector3.UnitZ);

    /// <summary>Moves through the existing world-pose write. Physical ownership and native validation still apply.</summary>
    public void Translate(Vector3 displacement, TransformSpace space = TransformSpace.Local)
    {
        Finite(displacement); CheckSpace(space);
        var delta = space == TransformSpace.Local ? Vector3.Transform(displacement, HierarchyRotation()) : displacement;
        WorldPosition += delta;
    }

    /// <summary>Atomic orientation change; angle is in radians, matching System.Numerics.</summary>
    public void Rotate(Vector3 axis, float angleRadians, TransformSpace space = TransformSpace.Local)
    {
        Finite(axis); CheckSpace(space);
        if (!float.IsFinite(angleRadians) || !float.IsFinite(axis.LengthSquared()) || axis.LengthSquared() < 1e-12f)
            throw new ArgumentOutOfRangeException(nameof(axis));
        var rotation = Quaternion.CreateFromAxisAngle(Vector3.Normalize(axis), angleRadians);
        var current = WorldRotation;
        WorldRotation = UnitRotation(space == TransformSpace.Local
            ? current * rotation : rotation * current);
    }

    /// <summary>Rotates around a world-space pivot and axis with one native pose write.</summary>
    public void RotateAround(Vector3 pivot, Vector3 axis, float angleRadians)
    {
        Finite(pivot); Finite(axis);
        if (!float.IsFinite(angleRadians) || !float.IsFinite(axis.LengthSquared()) || axis.LengthSquared() < 1e-12f)
            throw new ArgumentOutOfRangeException(nameof(axis));
        var rotation = Quaternion.CreateFromAxisAngle(Vector3.Normalize(axis), angleRadians);
        SetWorldPose(pivot + Vector3.Transform(WorldPosition - pivot, rotation),
            UnitRotation(rotation * WorldRotation));
    }

    /// <summary>Aims the object's local +Z at a world point. Up is a world-space hint.
    /// Coincident target and collinear up are rejected rather than silently selecting an orientation.
    /// A camera's viewing axis may differ; this method is an object orientation operation.</summary>
    public void LookAt(Vector3 target, Vector3 up)
    {
        Finite(target); Finite(up);
        var forward = target - WorldPosition;
        if (!float.IsFinite(forward.LengthSquared()) || forward.LengthSquared() < 1e-12f || up.LengthSquared() < 1e-12f)
            throw new ArgumentOutOfRangeException(nameof(target));
        forward = Vector3.Normalize(forward);
        var right = Vector3.Cross(up, forward);
        if (!float.IsFinite(right.LengthSquared()) || right.LengthSquared() < 1e-12f)
            throw new ArgumentOutOfRangeException(nameof(up));
        right = Vector3.Normalize(right); var correctedUp = Vector3.Cross(forward, right);
        var basis = new Matrix4x4(right.X, right.Y, right.Z, 0,
            correctedUp.X, correctedUp.Y, correctedUp.Z, 0,
            forward.X, forward.Y, forward.Z, 0, 0, 0, 0, 1);
        WorldRotation = UnitRotation(Quaternion.CreateFromRotationMatrix(basis));
    }
    public void LookAt(Vector3 target) => LookAt(target, Vector3.UnitY);

    // Preserve authored local scale and publish position+rotation in one existing
    // native write. A sheared world matrix does not need a lossy TRS decomposition.
    private void SetWorldPose(Vector3 position, Quaternion rotation)
    {
        Finite(position); rotation = UnitRotation(rotation);
        var parent = Parent;
        var local = LocalTransform;
        var localPosition = parent is null ? position : parent.InverseTransformPoint(position);
        var localRotation = parent is null ? rotation :
            UnitRotation(Quaternion.Conjugate(parent.HierarchyRotation()) * rotation);
        LocalTransform = local with { Position = localPosition, Rotation = localRotation };
    }

    private static void CheckSpace(TransformSpace space)
    {
        if (space != TransformSpace.Local && space != TransformSpace.World)
            throw new ArgumentOutOfRangeException(nameof(space));
    }
    // Compose authoring rotations separately: decomposition of a sheared world matrix
    // is not an orientation-only direction transform.
    private Quaternion HierarchyRotation()
    {
        var rotation = Quaternion.Identity; var seen = new HashSet<ulong>();
        for (GameObject? node = this; node is not null; node = node.Parent)
        {
            if (!seen.Add(node.ObjectId)) throw new InvalidOperationException("Cycle in transform hierarchy.");
            rotation = UnitRotation(node.LocalRotation * rotation);
        }
        return rotation;
    }
    private static void Finite(Vector3 value)
    {
        if (!float.IsFinite(value.X) || !float.IsFinite(value.Y) || !float.IsFinite(value.Z))
            throw new ArgumentOutOfRangeException(nameof(value));
    }
    private static Vector3 CheckedResult(Vector3 value)
    {
        if (!float.IsFinite(value.X) || !float.IsFinite(value.Y) || !float.IsFinite(value.Z))
            throw new InvalidOperationException("Transform conversion exceeds finite range.");
        return value;
    }
    private static bool IsFiniteMatrix(Matrix4x4 m) =>
        float.IsFinite(m.M11) && float.IsFinite(m.M12) && float.IsFinite(m.M13) && float.IsFinite(m.M14) &&
        float.IsFinite(m.M21) && float.IsFinite(m.M22) && float.IsFinite(m.M23) && float.IsFinite(m.M24) &&
        float.IsFinite(m.M31) && float.IsFinite(m.M32) && float.IsFinite(m.M33) && float.IsFinite(m.M34) &&
        float.IsFinite(m.M41) && float.IsFinite(m.M42) && float.IsFinite(m.M43) && float.IsFinite(m.M44);
    private static Quaternion UnitRotation(Quaternion value)
    {
        if (!float.IsFinite(value.X) || !float.IsFinite(value.Y) || !float.IsFinite(value.Z) ||
            !float.IsFinite(value.W) || !float.IsFinite(value.LengthSquared()) || value.LengthSquared() < 1e-12f)
            throw new ArgumentOutOfRangeException(nameof(value));
        return Quaternion.Normalize(value);
    }
}
