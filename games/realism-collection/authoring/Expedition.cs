using Astra;
using System;
using System.Collections.Generic;
using System.Numerics;

// Compiled separately inside each project. All physics calls use the public Astra API.
[ComponentId("project.Expedition")]
public sealed class Expedition : Behavior
{
    private static readonly int Scenario = SCENARIO_NUMBER;
    [PropertyId("interactionReach")] public float InteractionReach = 3.7f;
    [PropertyId("gripForce")] public float GripForce = 450;
    [PropertyId("missionMinutes")] public float MissionMinutes = 25;
    [PropertyId("springRate")] public float SpringRate = 11000;
    [PropertyId("shockDamping")] public float ShockDamping = 1500;
    [PropertyId("driveForce")] public float DriveForce = 4300;
    private readonly Dictionary<string, GameObject> _nodes = new();
    private readonly Dictionary<ulong, Vector3> _lastPosition = new();
    private readonly HashSet<ulong> _delivered = new();
    private readonly Dictionary<string,float> _servoLoad = new();
    private GameObject? _camera, _held, _vehicle, _driveCamera;
    private float _clock, _report, _pressTime, _filter = 240, _lamp = 420, _fuel = 100;
    private float _plateMass, _integrity = 100, _holdStable;
    private int _fuses;
    private bool _ended, _power, _vent = true, _liftUp, _doorOpen, _driving, _battery, _relic;
    private bool _lampOn = true, _bridge, _artifactRemoved;
    private readonly float[] _compression = new float[4];
    private readonly Vector3[] _wheel = {new(-.86f,0,-1.22f),new(.86f,0,-1.22f),new(-.86f,0,1.22f),new(.86f,0,1.22f)};

    public override void Start()
    {
        var root = Object.Parent ?? throw new InvalidOperationException("Missing project root");
        foreach (var node in root.Children()) Index(node);
        _camera = Node("Câmera dos olhos");
        _vehicle = Maybe("Transportador");
        _driveCamera = Maybe("Câmera de condução");
        _driveCamera?.GetComponent(ComponentIds.Camera)?.SetBool("enabled", false);
        _clock = MissionMinutes * 60;
        SetLightGroup("Luzes de trabalho", false);
        Say(Scenario == 0 ? "USINA 17 | encontre 2 fusíveis e a bateria; leve ao gerador. Toque: usar/soltar. Segure: arremessar." :
            Scenario == 1 ? "CARGA BRUTA | carregue as caixas. Use a mesa de controle para conduzir. Saltar freia; Usar retorna à base." :
            "SEPULCRO | coloque 30 kg na balança para abrir o portão. A estátua pesa 20 kg. Segure Usar para arremessar.");
    }

    private void Index(GameObject node)
    {
        _nodes[node.Name] = node;
        foreach (var child in node.Children()) Index(child);
    }
    private GameObject Node(string name) => Maybe(name) ?? throw new InvalidOperationException("Objeto necessário ausente: "+name);
    private GameObject? Maybe(string name) => _nodes.TryGetValue(name, out var n) && n.IsAlive ? n : null;
    private static Vector3 Pos(GameObject node) => node.WorldTransform.Position;
    private static Vector3 Forward(GameObject node) => Vector3.Normalize(Vector3.Transform(Vector3.UnitZ,node.WorldTransform.Rotation));
    private void Say(string message) => Scene.Log(ObjectId, message);
    private float Mass(GameObject node) => node.GetComponent(ComponentIds.PhysicsBody)?.GetFloat("mass") ?? 0;
    private Vector3 Velocity(GameObject node) => Scene.GetBodyVelocity(node.ObjectId);
    private static Vector3 Limit(Vector3 value, float length) => value.LengthSquared()>length*length ? Vector3.Normalize(value)*length : value;
    private void Force(GameObject body, Vector3 force)
    {
        if (!Scene.AddForce(body.ObjectId, force)) throw new InvalidOperationException("Força recusada: "+body.Name);
    }
    private void Torque(GameObject body, Vector3 torque)
    {
        if (!Scene.AddTorque(body.ObjectId, torque)) throw new InvalidOperationException("Torque recusado: "+body.Name);
    }
    private void SetLightGroup(string name, bool on)
    {
        var group = Maybe(name);
        if (group == null) return;
        foreach(var node in group.Children()) node.GetComponent(ComponentIds.Light)?.SetBool("enabled",on);
    }

    public override void Update(float dt)
    {
        if (_ended) { Object.MoveCharacter(Vector2.Zero,0); return; }
        _clock -= dt;
        if (_clock <= 0 || Pos(Object).Y < -12) { Finish(false,"Tempo esgotado ou queda fora da área"); return; }
        if (_driving) Object.MoveCharacter(Vector2.Zero,0);
        if (Input.JustPressed("Interagir")) _pressTime = 0;
        if (Input.Pressed("Interagir")) _pressTime += dt;
        if (Input.JustReleased("Interagir"))
        {
            if (_held != null && _pressTime > .55f) Release(true);
            else Use();
        }
        if (Scenario == 0)
        {
            if (_lampOn) _lamp = MathF.Max(0,_lamp-dt);
            Maybe("Lanterna")?.GetComponent(ComponentIds.Light)?.SetBool("enabled", _lampOn && _lamp>0);
            if (Pos(Object).Z > 9 && (!_power || !_vent)) _filter -= dt;
            else _filter = MathF.Min(240,_filter+dt*.25f);
            if (_filter<=0) Finish(false,"Filtro saturado");
            if (_relic && Pos(Object).Z < -18) Finish(true,"Amostra recuperada; extração concluída");
        }
        else if (Scenario == 1 && _vehicle != null)
        {
            if (Pos(_vehicle).Y < -5 || Vector3.Dot(Vector3.Transform(Vector3.UnitY,_vehicle.WorldTransform.Rotation),Vector3.UnitY)<.15f)
                _holdStable += dt;
            else _holdStable=0;
            if (_holdStable>4 || _fuel<=0) Finish(false,"Transportador imobilizado; reinicie a missão");
            var destination = new Vector3(0,0,33);
            foreach(var pair in _nodes)
            {
                var n=pair.Value;
                if (!n.IsAlive || !pair.Key.StartsWith("Carga ") || pair.Key=="Carga suspensa" || Mass(n)<=0 || _delivered.Contains(n.ObjectId)) continue;
                var p=Pos(n);
                if (Vector2.Distance(new(p.X,p.Z),new(destination.X,destination.Z))<4 && p.Y<2.4f && Velocity(n).Length()<.5f)
                { _delivered.Add(n.ObjectId); Say("CARGA ENTREGUE "+_delivered.Count+"/3"); }
            }
            if (_delivered.Count>=3) Finish(true,"Contrato entregue com integridade "+MathF.Round(_integrity)+"%");
        }
        else if (Scenario == 2 && _artifactRemoved && _held?.Name=="Artefato" && Pos(Object).Z < -18)
            Finish(true,"Artefato retirado; equilíbrio restaurado e expedição concluída");
        _report-=dt;
        if (_report<=0)
        {
            _report=8;
            Say(Scenario==0 ? $"USINA | fusíveis {_fuses}/2 | bateria {(_battery?"sim":"não")} | circuito {(_vent?"ventilação":"elevador")} | filtro {(int)_filter}s | luz {(int)_lamp}s" :
                Scenario==1 ? $"CARGA | entregas {_delivered.Count}/3 | energia {(int)_fuel}% | integridade {(int)_integrity}% | {(_driving?"Usar: voltar ao posto; Saltar: frear":"mire na mesa de controle")}" :
                $"SEPULCRO | balança {_plateMass:F0}/30 kg | portão {(_plateMass>=29?"liberado":"fechado")} | artefato {(_artifactRemoved?"retirado":"na câmara")}");
        }
    }

    private GameObject? Target()
    {
        if (_camera==null) return null;
        var hits=Physics.RayCastAll(Pos(_camera),Forward(_camera)*InteractionReach,out var truncated,QueryFilter.Default.Ignoring(Object),32);
        if(truncated) { Say("Interação ambígua: aproxime-se"); return null; }
        foreach(var hit in hits) if(hit.Object!=_held) return hit.Object;
        return null;
    }
    private bool Portable(GameObject target) => target.Name.StartsWith("Fusível") || target.Name=="Bateria" ||
        target.Name.StartsWith("Peso ") || target.Name.StartsWith("Carga ") || target.Name=="Estátua" || target.Name=="Artefato" ||
        target.Name.StartsWith("Ferramenta") || target.Name.StartsWith("Caixa móvel");
    private void Use()
    {
        if (_driving) { Drive(false); return; }
        var target=Target();
        if(_held!=null)
        {
            if(Scenario==0 && target?.Name=="Painel do gerador" && (_held.Name.StartsWith("Fusível") || _held.Name=="Bateria"))
            {
                if(_held.Name=="Bateria") _battery=true; else _fuses++;
                _held.Destroy(); _held=null; _power=_fuses>=2&&_battery;
                SetLightGroup("Luzes de trabalho",_power);
                Say(_power?"Gerador operacional. Selecione o circuito no painel secundário.":"Peça instalada; faltam componentes.");
            }
            else Release(false);
            return;
        }
        if(target==null)
        {
            if(Scenario==0) { _lampOn=!_lampOn; Say(_lampOn?"Lanterna ligada":"Lanterna desligada"); }
            else Say("Mire em um objeto ou controle ao alcance da mão");
            return;
        }
        if(Portable(target)) { _held=target; if(target.Name=="Artefato") _artifactRemoved=true; Say("Segurando "+target.Name+" · "+Mass(target)+" kg"); return; }
        switch(target.Name)
        {
            case "Porta da oficina": _doorOpen=!_doorOpen; Say(_doorOpen?"Abrindo oficina":"Fechando oficina"); break;
            case "Painel do gerador": Say($"Gerador: {_fuses}/2 fusíveis; bateria {(_battery?"instalada":"ausente")}"); break;
            case "Seletor de circuito": if(_power) { _vent=!_vent; Say(_vent?"Ventilação alimentada":"Elevador alimentado; ventilação parada"); } else Say("Restaure o gerador"); break;
            case "Comando do elevador": if(_power&&!_vent) { _liftUp=!_liftUp; Say(_liftUp?"Subindo plataforma":"Descendo plataforma"); } else Say("Selecione o circuito elevador"); break;
            case "Amostra": if(_power) { _relic=true; Say("Amostra selada. Volte à extração."); } else Say("Câmara sem energia"); break;
            case "Mesa de controle": Drive(true); break;
            case "Comando da plataforma": _liftUp=!_liftUp; Say(_liftUp?"Plataforma elevada":"Plataforma baixando"); break;
            case "Alavanca da ponte": _bridge=!_bridge; Say(_bridge?"Ponte liberada":"Ponte recolhida"); break;
            default: Say(target.Name); break;
        }
    }
    private void Release(bool impulse)
    {
        if(_held is { IsAlive:true } && impulse && _camera!=null)
            Scene.AddImpulse(_held.ObjectId,(Forward(_camera)+Vector3.UnitY*.18f)*MathF.Min(32,Mass(_held)*5));
        _held=null; Say(impulse?"Objeto lançado":"Objeto solto");
    }
    private void Drive(bool on)
    {
        if(_vehicle==null||_driveCamera==null||_camera==null) return;
        _driving=on;
        _driveCamera.GetComponent(ComponentIds.Camera)?.SetBool("enabled",on);
        _camera.GetComponent(ComponentIds.Camera)?.SetBool("enabled",!on);
        Say(on?"Controle remoto: mover acelera/vira; Saltar freia; Usar volta ao posto.":"Controle devolvido ao posto de operação");
    }

    public override void FixedUpdate(float dt)
    {
        if (_ended) return;
        if(_held is {IsAlive:true} body && _camera!=null)
        {
            var delta=Pos(_camera)+Forward(_camera)*2.1f-body.WorldTransform.Position;
            if(delta.Length()>5) Release(false);
            else Force(body,Limit(delta*180-Velocity(body)*28+Vector3.UnitY*Mass(body)*9.81f,GripForce));
        }
        if(Scenario==0)
        {
            var door=Maybe("Porta da oficina");
            if(door!=null)
            {
                var f=Forward(door); var angle=MathF.Atan2(f.X,f.Z);
                Torque(door,Vector3.UnitY*Math.Clamp(((_doorOpen?1.45f:0)-angle)*70,-80,80));
            }
            Servo("Elevador",_power&&!_vent&&_liftUp?3.4f:.25f,1300,280,dt);
        }
        if(Scenario==1)
        {
            Vehicle(dt);
            Servo("Plataforma de carga",_liftUp?1.8f:.25f,2200,400,dt);
            foreach(var pair in _nodes)
            {
                if(!pair.Key.StartsWith("Carga ") || pair.Key=="Carga suspensa" || !pair.Value.IsAlive || Mass(pair.Value)<=0) continue;
                var n=pair.Value; var p=Pos(n);
                if(p.Y<-.5f && !_delivered.Contains(n.ObjectId)) { Finish(false,"Carga perdida na encosta"); break; }
                // Integrity is a documented gameplay estimate from abrupt deceleration, not solver contact impulse.
                var v=Velocity(n);
                if(_lastPosition.TryGetValue(n.ObjectId,out var oldV) && oldV.Length()-v.Length()>4) _integrity=MathF.Max(0,_integrity-(oldV.Length()-v.Length())*2);
                _lastPosition[n.ObjectId]=v;
            }
            if(_integrity<=0) Finish(false,"Carga danificada");
        }
        if(Scenario==2)
        {
            var plate=Node("Balança");
            _plateMass=0;
            var hits=Physics.Overlap(ShapeQuery.Box(new(1.5f,.7f,1.5f)),Pos(plate)+Vector3.UnitY*.55f,out var truncated,QueryFilter.Default.Ignoring(plate),64);
            if(truncated) throw new InvalidOperationException("Balança excedeu 64 contatos consultáveis");
            var unique=new HashSet<ulong>();
            foreach(var hit in hits)
                if(unique.Add(hit.Object.ObjectId) && Portable(hit.Object) && hit.Object!=_held && Velocity(hit.Object).Length()<1)
                    _plateMass+=Mass(hit.Object);
            Servo("Balança",.55f-MathF.Min(.3f,_plateMass*.008f),1400,140,dt);
            Servo("Portão do sepulcro",_plateMass>=29?5.2f:2.2f,2200,500,dt);
            Servo("Ponte móvel",_bridge ? .3f : 4.5f,2300,450,dt);
            var support=Maybe("Suporte de ruína");
            if(_artifactRemoved && support!=null)
                Scene.MoveKinematic(support.ObjectId,new Vector3(6,-3,21),Quaternion.Identity);
        }
    }
    private void Servo(string name,float y,float spring,float damping,float dt)
    {
        var node=Maybe(name); if(node==null) return;
        var error=y-Pos(node).Y;
        _servoLoad.TryGetValue(name,out var load);
        // Integral load compensation allows a loaded lift to reach its landing.
        load=Math.Clamp(load+error*spring*.4f*dt,-8000,8000);
        _servoLoad[name]=load;
        var force=error*spring-Velocity(node).Y*damping+Mass(node)*9.81f+load;
        Force(node,Vector3.UnitY*Math.Clamp(force,-18000,18000));
    }
    private void Vehicle(float dt)
    {
        if(_vehicle==null) return;
        var pose=_vehicle.WorldTransform;
        var velocity=Velocity(_vehicle);
        var input=_driving?Input.Move:Vector2.Zero;
        var up=Vector3.Transform(Vector3.UnitY,pose.Rotation);
        var forward=Vector3.Transform(Vector3.UnitZ,pose.Rotation);
        var right=Vector3.Transform(Vector3.UnitX,pose.Rotation);
        var filter=QueryFilter.Default.Ignoring(_vehicle);
        filter.IncludeDynamic=false;
        int grounded=0;
        for(int i=0;i<4;i++)
        {
            var lever=Vector3.Transform(_wheel[i],pose.Rotation);
            var origin=pose.Position+lever;
            var hit=Physics.RayCast(origin,-up*1.05f,filter);
            var compression=hit.HasValue?MathF.Max(0,.87f-hit.Value.Distance):0;
            if(hit.HasValue && compression>0)
            {
                grounded++;
                var rate=Math.Clamp((compression-_compression[i])/MathF.Max(dt,.001f),-6,6);
                var support=up*Math.Clamp(compression*SpringRate+rate*ShockDamping,0,18000);
                Force(_vehicle,support); Torque(_vehicle,Vector3.Cross(lever,support));
                var traction=forward*(input.Y*DriveForce/4) - right*(Vector3.Dot(velocity,right)*500);
                if(!_driving || Input.Pressed(Input.RoleAction(InputRole.Jump))) traction-=forward*Vector3.Dot(velocity,forward)*700;
                traction=Limit(traction,support.Length()*.7f);
                Force(_vehicle,traction); Torque(_vehicle,Vector3.Cross(lever,traction));
            }
            _compression[i]=compression;
            var wheel=Maybe("Roda "+i);
            if(wheel!=null) wheel.Position=_wheel[i]-Vector3.UnitY*(.44f-compression);
        }
        if(grounded>1)
        {
            var signedSpeed=Vector3.Dot(velocity,forward);
            Torque(_vehicle,Vector3.UnitY*(input.X*Math.Clamp(signedSpeed,-6,6)*110));
        }
        _fuel=MathF.Max(0,_fuel-MathF.Abs(input.Y)*dt*.065f);
    }
    public override void LateUpdate(float dt)
    {
        if(!_driving||_vehicle==null||_driveCamera==null) return;
        var p=Pos(_vehicle); var forward=Forward(_vehicle); forward.Y=0;
        if(forward.LengthSquared()<.01f) forward=Vector3.UnitZ;
        forward=Vector3.Normalize(forward);
        var eye=p-forward*7+Vector3.UnitY*4;
        var direction=Vector3.Normalize(p+Vector3.UnitY*.5f-eye);
        var yaw=MathF.Atan2(direction.X,direction.Z);
        var pitch=-MathF.Asin(direction.Y);
        _driveCamera.WorldTransform=new(eye,Quaternion.CreateFromYawPitchRoll(yaw,pitch,0),Vector3.One);
    }
    private void Finish(bool won,string reason)
    {
        if(_ended) return;
        _ended=true; Release(false); _driving=false;
        Say((won?"MISSÃO CONCLUÍDA | ":"MISSÃO ENCERRADA | ")+reason+" | Stop e Play reiniciam sem alterar a cena autoral.");
    }
}

