using System.Numerics;

namespace Astra;

/// <summary>Handles are offsets from Position in the path object's local space.</summary>
public readonly record struct CurvePoint3D(ulong Id,Vector3 Position,Vector3 In,Vector3 Out)
{
    public float RollDegrees { get; init; }
}
public readonly record struct PathSample(Vector3 Position,Vector3 Tangent,double Length);
public readonly record struct PathFrame(Vector3 Position,Vector3 Tangent,Vector3 Up,float RollDegrees,double Length)
{
    public Vector3 Right => Vector3.Cross(Up,Tangent);
}

/// <summary>A scene-owned Curve3D. Point identity survives reorder and value edits.</summary>
public readonly struct CurvePath
{
    private readonly Component component;
    public CurvePath(Component component)
    {
        if(component.TypeId!=ComponentIds.Path) throw new ArgumentException("Component must be Path.",nameof(component));
        this.component=component;
    }
    public int Count => Command(0,0,0,[],[],out _);
    public CurvePoint3D At(int index)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(index);
        Span<float> values=stackalloc float[10];
        Command(7,0,(uint)index,[],values,out var id);
        return Decode(id,values);
    }
    public CurvePoint3D ById(ulong id)
    {
        Span<float> values=stackalloc float[10];
        Command(8,id,0,[],values,out var identity);
        return Decode(identity,values);
    }
    public ulong Insert(int index,Vector3 position,Vector3 incoming=default,Vector3 outgoing=default,float rollDegrees=0)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(index);
        Span<float> values=stackalloc float[10]; Encode(position,incoming,outgoing,values[..9]);values[9]=ValidateRoll(rollDegrees);
        Command(9,0,(uint)index,values,[],out var identity);return identity;
    }
    public void Set(ulong id,Vector3 position,Vector3 incoming,Vector3 outgoing)
    {
        Span<float> values=stackalloc float[9];Encode(position,incoming,outgoing,values);
        Command(4,id,0,values,[],out _);
    }
    /// <summary>Atomically edits geometry and roll; the overload without roll preserves it.</summary>
    public void Set(ulong id,Vector3 position,Vector3 incoming,Vector3 outgoing,float rollDegrees)
    {
        Span<float> values=stackalloc float[10];Encode(position,incoming,outgoing,values[..9]);values[9]=ValidateRoll(rollDegrees);
        Command(10,id,0,values,[],out _);
    }
    public Vector3 Up {get=>new(component.GetFloat("up_x"),component.GetFloat("up_y"),component.GetFloat("up_z"));set=>component.SetVector3("up",value);}
    public void Remove(ulong id) => Command(5,id,0,[],[],out _);
    public void Move(ulong id,int destinationIndex)
    {
        ArgumentOutOfRangeException.ThrowIfNegative(destinationIndex);
        Command(6,id,(uint)destinationIndex,[],[],out _);
    }
    public bool Closed {get=>component.GetBool("closed");set=>component.SetBool("closed",value);}
    public PathSample Sample(double worldDistance,bool wrap=false)
    {
        CheckAlive();
        if(!double.IsFinite(worldDistance)) throw new ArgumentOutOfRangeException(nameof(worldDistance));
        Span<float> values=stackalloc float[6];
        if(!component.Scene.PathRuntimeCommand(component.Object.ObjectId,component.Object.World,component.Object.Generation,component.InstanceId,0,worldDistance,wrap,values,out var length))
            throw new WorldException(component.Scene.LastStatus,"amostrar caminho");
        return new(new(values[0],values[1],values[2]),new(values[3],values[4],values[5]),length);
    }
    public PathFrame SampleFrame(double worldDistance,bool wrap=false)
    {
        CheckAlive();
        if(!double.IsFinite(worldDistance))throw new ArgumentOutOfRangeException(nameof(worldDistance));
        Span<float> values=stackalloc float[10];
        if(!component.Scene.PathRuntimeCommand(component.Object.ObjectId,component.Object.World,component.Object.Generation,component.InstanceId,5,worldDistance,wrap,values,out var length))
            throw new WorldException(component.Scene.LastStatus,"amostrar orientação do caminho");
        return new(new(values[0],values[1],values[2]),new(values[3],values[4],values[5]),new(values[6],values[7],values[8]),values[9],length);
    }
    private int Command(uint operation,ulong id,uint index,ReadOnlySpan<float> input,Span<float> output,out ulong identity)
    {
        CheckAlive();
        int result=component.Scene.PathPointCommand(component.Object.ObjectId,component.Object.World,component.Object.Generation,component.InstanceId,operation,id,index,input,output,out identity);
        if(result<0) throw new WorldException(component.Scene.LastStatus,"editar pontos do caminho");
        return result;
    }
    private void CheckAlive()
    {
        if(component.Object is null) throw new InvalidOperationException("Uninitialized curve access.");
        if(!component.IsAlive) throw new WorldException(WorldStatus.StaleHandle,"acessar caminho");
    }
    private static CurvePoint3D Decode(ulong id,ReadOnlySpan<float> values) =>
        new(id,new(values[0],values[1],values[2]),new(values[3],values[4],values[5]),new(values[6],values[7],values[8])){RollDegrees=values[9]};
    private static float ValidateRoll(float value) => float.IsFinite(value)&&MathF.Abs(value)<=3600?value:throw new ArgumentOutOfRangeException(nameof(value));
    private static void Encode(Vector3 position,Vector3 incoming,Vector3 outgoing,Span<float> values)
    {
        values[0]=position.X;values[1]=position.Y;values[2]=position.Z;
        values[3]=incoming.X;values[4]=incoming.Y;values[5]=incoming.Z;
        values[6]=outgoing.X;values[7]=outgoing.Y;values[8]=outgoing.Z;
        foreach(float value in values) if(!float.IsFinite(value)||MathF.Abs(value)>100000) throw new ArgumentOutOfRangeException(nameof(position));
    }
}

public readonly struct PathFollower
{
    private readonly Component component;
    public PathFollower(Component component)
    {
        if(component.TypeId!=ComponentIds.PathFollow) throw new ArgumentException("Component must be PathFollow.",nameof(component));
        this.component=component;
    }
    public void Restart() => Command(1);
    public void Stop() => Command(2);
    public double Progress => Command(3);
    /// <summary>Playback request; may be blocked by a runtime diagnostic or held at an endpoint.</summary>
    public bool IsPlaying => Command(4)!=0;
    private double Command(uint operation)
    {
        if(component.Object is null) throw new InvalidOperationException("Uninitialized follower access.");
        if(!component.IsAlive) throw new WorldException(WorldStatus.StaleHandle,"acessar seguidor");
        if(!component.Scene.PathRuntimeCommand(component.Object.ObjectId,component.Object.World,component.Object.Generation,component.InstanceId,operation,0,false,[],out var distance))
            throw new WorldException(component.Scene.LastStatus,"controlar seguidor");
        return distance;
    }
}
