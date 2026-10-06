"""Build three independent, editable Astra projects using existing engine contracts."""
from pathlib import Path
import hashlib, importlib.util, json, math, shutil, struct, wave
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[1]
OUT = ROOT/'projects'

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

kit = load('backrooms_kit', REPO/'tools/generate-highlevel-games.py')
signs = load('backrooms_signs', REPO/'games/realism-collection/authoring/signage.py')
base = kit.base
kit.OUT = OUT

def outward(fn):
    def generate(*args, **kwargs):
        p,n,uv,indices = fn(*args, **kwargs)
        indices = list(indices)
        for k in range(0,len(indices),3):
            a,b,c = indices[k:k+3]
            u = [p[b][j]-p[a][j] for j in range(3)]
            v = [p[c][j]-p[a][j] for j in range(3)]
            cross = (u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
            if sum(cross[j]*(n[a][j]+n[b][j]+n[c][j]) for j in range(3)) < 0:
                indices[k+1],indices[k+2] = c,b
        return p,n,uv,indices
    return generate

for name in ['cube','sphere','cylinder','torus']:
    setattr(kit,name,outward(getattr(kit,name)))

def light_component(kind,color,intensity,radius=12):
    # Engine units preserve the authored production scale. No fake photometric claim.
    return ('astra.render.light',3,' '.join(map(base.f,[kind,1,0,0,2,3,*color,6500,intensity,radius,30,65,1,.025,.12,.08])))
base.light_component = light_component

def environment(mode, local=False):
    # Ordering follows Environment::write and descriptor arrays in scene/environment.h v12.
    flags = [1,1,1,1,int(mode==1),0,1,2,1,1 if local else 0,
             1,1,1,0,'-',1,0,'-',int(mode==1),1]
    sky = [0]*9
    fog = [0,0,0]
    numbers = [20 if local else 0,*sky,0,.53,0,*fog,1,
               .014 if mode==1 else .006,7,0,.12,-.8,1.6,.025,1.02]
    numbers += [.9,.10,.012, .65,1.15,1.3,.025] # saturation/vignette/grain; SSAO
    numbers += [1,3,20,8,16,5] # volume weight, blend and dimensions
    numbers += [0,0] # No ambient fill or environmental specular radiance in an unpowered interior.
    numbers += [1,1,1,.76,6371,.002,8,1.2,100,.1]
    numbers += [-2,-1.5,.05,.95,.18,1.5,.7,0,0] # adaptation cannot expose dark corridors like daylight
    assert len(numbers)==59
    return ('astra.render.environment',12,' '.join(map(str,flags+numbers)))

def patch(scene, entity, components):
    node = list(scene.entities[entity-1])
    node[7] = tuple(node[7])+tuple(components)
    scene.entities[entity-1] = tuple(node)

def tween(destination, duration=2.5):
    return ('astra.tween.transform',3,' '.join(map(base.f,[1,0,0,0,1,0,0,duration,0,1,1,*destination,0,0,0,1,1,1,0,0,0])))

def action_map():
    actions = 'INPUT ASTRA_ACTION_MAP 2 5 "Mover" "Olhar" "Saltar"'
    definitions = [('Mover',2,.12,0,.4,[(1,0,0,0,1,0),(1,0,0,1,1,0)]),
                   ('Olhar',2,0,0,.4,[(2,0,0,0,1,0),(2,0,0,1,1,0)]),
                   ('Saltar',0,0,0,.4,[(3,0,0,0,1,0)]),
                   ('Operar',0,0,1,2,[(3,1,0,0,1,0),(4,33,0,0,1,0)]),
                   ('Lanterna',0,0,0,.4,[(4,34,0,0,1,0)])]
    for name,kind,dead,interaction,duration,bindings in definitions:
        actions += ' '+base.quoted(name)+f' {kind} {dead} 1 "" {len(bindings)} 1 {interaction} {duration} 7'
        for binding in bindings: actions += ' '+' '.join(map(str,binding))
    return actions

GAMES = [('nivel-0','Nível 0 — Circuito Amarelo','CircuitoAmarelo',0),
         ('reservatorio-04','Reservatório 04','Reservatorio04',1),
         ('ultimo-turno','Subsolo — Último Turno','UltimoTurno',2)]

def readable_signs(definitions):
    data=bytearray(signs.make_signs(definitions))
    json_size=struct.unpack_from('<I',data,12)[0]
    document=json.loads(data[20:20+json_size])
    binary_start=28+json_size
    for mesh in document['meshes']:
        accessor=document['accessors'][mesh['primitives'][0]['attributes']['TEXCOORD_0']]
        view=document['bufferViews'][accessor['bufferView']]
        offset=binary_start+view.get('byteOffset',0)+accessor.get('byteOffset',0)
        for vertex in range(accessor['count']):
            u=struct.unpack_from('<f',data,offset+vertex*8)[0]
            struct.pack_into('<f',data,offset+vertex*8,1-u)
    return data

def surfaces(folder,mode):
    pairs = {'concrete':'decrepit_wallpaper' if mode==0 else 'dirty_tiles' if mode==1 else 'concrete_floor_02',
             'wood':'dirty_carpet' if mode==0 else 'yellow_plaster',
             'rock':'yellow_plaster' if mode==0 else 'dirty_tiles' if mode==1 else 'quarry_wall',
             'steel':'worn_shutter'}
    for label,asset in pairs.items():
        source = ROOT/'sources'/asset
        if not source.exists(): source = REPO/'games/realism-collection/sources'/asset
        for role in ['base.jpg','normal.png','arm.png']:
            with Image.open(source/role) as im:
                im = im.convert('RGB')
                limit=2048 if mode==1 else 1024
                im.thumbnail((limit,limit),Image.Resampling.LANCZOS)
                im.save(folder/(label+'-'+role))

def make_project(slug,title,behavior,mode):
    project = OUT/slug
    for sub in ['Assets','Scripts','scenes','.astra','ArtSources','Audio']:
        (project/sub).mkdir(parents=True,exist_ok=True)
    surfaces(project/'ArtSources',mode)
    # Lamp slot is a closed cube here: luminous diffusers must have flat profiles.
    sphere = kit.sphere
    kit.sphere = lambda *args,**kw: kit.cube()
    (project/'Assets/world.glb').write_bytes(kit.world_glb(texture_root=project/'ArtSources'))
    kit.sphere = sphere
    s = kit.Scene()
    root = s.add(title)
    architecture = s.add('Arquitetura / colisão',root)
    dressing = s.add('Instalações / acabamento',root)
    lights = s.add('Iluminação dinâmica / 8 locais no máximo',root)
    mission = s.add('Mecânicas / objetivos',root)
    s.add('Ambiente global escuro',root,components=(environment(mode),))
    if mode==1: s.add('Volume úmido / setor central',root,pos=(0,2.5,24),components=(environment(mode,True),))
    width = 8 if mode==0 else 10
    height = 3.2 if mode==0 else 5
    floor_mesh = 'Timber' if mode==0 else 'Rock' if mode==1 else 'Concrete'
    color = (1,.9,.67) if mode==0 else (.62,.69,.68) if mode==1 else (.55,.53,.48)
    def shape(name,mesh,parent,pos,half,**kw):
        return kit.shape(s,name,mesh,parent,pos,half,**kw)
    for sector in range(3):
        z = 8+sector*16
        shape('Piso setor '+str(sector+1),floor_mesh,architecture,(0,-.2,z),(width,.2,8),rough=.9 if mode==0 else .28 if mode==1 else .8,solid=True)
        # Reservoir skylights are real openings, with a physically occluding roof around them.
        if mode==1:
            for x in [-7,7]: shape('Cobertura lateral '+str((sector,x)),'Concrete',architecture,(x,height+.15,z),(3,.15,8),solid=True)
            for zz in [z-5,z+5]: shape('Cobertura transversal '+str((sector,zz)),'Concrete',architecture,(0,height+.15,zz),(4,.15,3),solid=True)
            for x in [-3.8,3.8]: shape('Caixilho da claraboia '+str((sector,x)),'Steel',dressing,(x,height,z),(.08,.1,2),metallic=.8,rough=.28)
            shape('Persiana '+str(sector+1),'Steel',architecture,(0,height+.15,z),(4,.15,2),
                  kinematic=True,metallic=.6,rough=.6,extra=(tween((0,height+.15,z+4),3),))
        else: shape('Forro '+str(sector),'Concrete',architecture,(0,height+.15,z),(width,.15,8),color=(.66,.65,.59),solid=True)
        for x in [-width,width]:
            shape('Parede externa '+str((sector,x)),'Concrete',architecture,(x,height/2,z),(.2,height/2,8),color=color,solid=True)
            shape('Rodapé '+str((sector,x)),'Steel' if mode else 'Timber',dressing,(x*.978,.12,z),(.045,.12,8),color=(.2,.18,.13),rough=.8)
        if sector<2:
            gap_x = -3.5 if sector==0 else 3.5
            edge = z+8
            for lo,hi in [(-width,gap_x-1.15),(gap_x+1.15,width)]:
                shape('Divisória '+str((sector,lo)),'Concrete',architecture,((lo+hi)/2,height/2,edge),((hi-lo)/2,height/2,.2),color=color,solid=True)
            shape('Verga de passagem '+str(sector),'Concrete',architecture,(gap_x,2.9,edge),(1.15,.3,.2),color=color,solid=True)
        for x,zl in [(-3,z-3),(3,z+3)]:
            index=sector*2+(0 if x<0 else 1)
            fixture=shape('Difusor '+str(index),'Lamp',dressing,(x,height-.13,zl),(.65,.045,.18),color=(.7,.74,.61),emission=(.85,.9,.65) if mode==0 else (.72,.84,1),strength=1.8,uv_world=False)
            shape('Carcaça luminária '+str(index),'Steel',dressing,(x,height-.07,zl),(.73,.03,.25),rough=.45,metallic=.7)
            point=mode==1 and index%2==1
            kit.light(s,'Circuito '+str(index),lights,(-width+.7,2,zl) if point else (x,height-.22,zl),
                      (1,.86,.61) if mode==0 else (.65,.8,1) if mode==1 else (1,.75,.46),
                      25 if point else 100 if mode==1 else 30,7 if point else 8,kind=1 if point else 2,rot=(0,0,0) if point else (90,0,0))
            if point:
                shape('Arandela de serviço '+str(index),'Lamp',dressing,(-width+.35,2,zl),(.12,.3,.16),emission=(.5,.7,1),strength=1.2,uv_world=False)
        if mode==0:
            # Offset islands form walkable loops without trapping any authored objectives.
            for x in [-5,5]:
                shape('Ilha labiríntica '+str((sector,x)),'Concrete',architecture,(x,1.6,z+1),(1.1,1.6,2.6),color=color,solid=True)
                for end in [-1,1]: shape('Acabamento ilha '+str((sector,x,end)),'Timber',dressing,(x+end*1.12,.12,z+1),(.025,.12,2.6),color=(.23,.18,.1))
        else:
            for x in [-8,8]:
                shape('Coluna '+str((sector,x)),'Concrete',architecture,(x,height/2,z),(.4,height/2,.45),solid=True)
                shape('Tubulação '+str((sector,x)),'Pipe',dressing,(x*.88,3.4,z),(.12,7.5,.12),metallic=.75,rough=.36,rot=(90,0,0))
            if mode==1:
                for x in [-7,7]:
                    shape('Reservatório cilíndrico '+str((sector,x)),'Pipe',dressing,(x,1.45,z+3),(1.05,1.45,1.05),color=(.42,.51,.52),metallic=.8,rough=.28,solid=True)
                    for y in [.4,2.45]: shape('Cinta do tanque '+str((sector,x,y)),'Valve',dressing,(x,y,z+3),(1.08,1.08,.08),rot=(90,0,0),metallic=.9,rough=.23)
            else:
                for y in [.3,1.2,2.1]: shape('Estante '+str((sector,y)),'Steel',dressing,(-7,y,z+3),(1,.06,2.6),rough=.7,metallic=.65)
        # Panels are physical controls: progress segments and completion have consumers in script.
        px = -2 if sector!=1 else 2
        pz = z-3
        panel=s.add('Painel '+str(sector+1),mission,pos=(px,0,pz))
        shape('Suporte '+str(sector),'Steel',panel,(0,.55,0),(.08,.55,.08),solid=True)
        shape('Caixa de comando '+str(sector),'Steel',panel,(0,1.3,0),(.4,.32,.18),color=(.4,.43,.37),solid=True)
        for k in range(8): shape(f'Progresso {sector+1} {k}','Lamp',panel,(-.32+k*.09,1.4,-.195),(.03,.055,.015),color=(.1,.8,.35),emission=(.1,.8,.35),strength=0,uv_world=False)
        if mode==1:
            valve=shape('Válvula '+str(sector+1),'Valve',panel,(0,1.2,-.28),(.22,.22,.055),color=(.55,.12,.06),metallic=.65,rough=.48,extra=(tween((0,1.48,-.28),1.5),))
        if mode==0:
            fuse=shape('Fusível '+str(sector+1),'Steel',mission,(-px,1.05,z-1),(.08,.21,.08),color=(.75,.62,.28),metallic=.65,rough=.25)
            shape('Mesa do fusível '+str(sector),'Timber',dressing,(-px,.72,z-1),(.55,.055,.4),solid=True)
    for z in [0,49]: shape('Fechamento '+str(z),'Concrete',architecture,(0,height/2,z),(width,height/2,.2),color=color,solid=True)
    # Split physical barrier from animated visual; removal happens only after the tween completes.
    gate=shape('Porta de saída','Steel',mission,(0,1.4,46),(1.15,1.4,.12),color=(.3,.36,.32),extra=(tween((0,4.8,46)),))
    s.add('Colisão saída',mission,pos=(0,1.4,46),components=(base.body_component(0),base.collider_component((1.15,1.4,.12))))
    for x in [-1,1]: shape('Parede da saída '+str(x),'Concrete',architecture,(x*(width+1.15)/2,height/2,46),((width-1.15)/2,height/2,.2),color=color,solid=True)
    kit.light(s,'Luz saída',lights,(0,2.9,47),(.3,.65,.42),3,4)
    shape('Sinal de saída','Lamp',dressing,(0,2.65,45.8),(.48,.09,.02),color=(.1,.35,.12),emission=(.1,.65,.2),strength=1.2,uv_world=False)
    # Explicit zero suppresses default daylight; the graphical acceptance scene exercises this.
    kit.light(s,'Direcional / claraboias',lights,(0,15,24),(.72,.82,1),0,kind=0,rot=(75,-15,0))
    player=kit.player(s,'Explorador',root,(0,0,2),behavior,speed=4,jump=4.5,fov=72)
    torch=kit.light(s,'Lanterna móvel',player+1,(.12,-.12,.25),(1,.93,.8),1200 if mode==1 else 400,40,kind=2)
    node=list(s.entities[torch-1]);component=node[7][0];payload=component[2].split();payload[12:14]=['14','27']
    node[7]=((component[0],component[1],' '.join(payload)),);s.entities[torch-1]=tuple(node)
    patch(s,player+1,[('astra.audio.listener',1,'1 1 100')])
    # Electrical hum, PCM source, not a mock sound event.
    with wave.open(str(project/'Audio/electrical.wav'),'wb') as out:
        out.setparams((2,2,48000,0,'NONE','not compressed'))
        samples=bytearray()
        for sample in range(48000*2):
            t=sample/48000
            v=int(1000*(math.sin(2*math.pi*60*t)+.18*math.sin(2*math.pi*180*t)))
            samples.extend(struct.pack('<hh',v,v))
        out.writeframes(samples)
    for index in range(3):
        s.add('Zumbido '+str(index),root,pos=(0,height-.3,8+16*index),components=(('astra.audio.source',1,f'{base.guid("fonte:Audio/electrical.wav")} 0 1 1 0 0 1 0 .25 1 0 1 15 1 360 360 0 0'),))
    if mode>0:
        model='modular_industrial_pipes_01'
        shutil.copyfile(REPO/f'games/realism-collection/sources/{model}/{model}.glb',project/f'Assets/{model}.glb')
        kit.real_prop(s,slug,model,'Tubulação industrial CC0',dressing,(7,0,42),(1.2,)*3)
    instructions = [['Recolha três fusíveis em suas mesas.','Segure Operar por 2s no painel próximo.','Cada circuito restaurado ilumina o setor.','Toque curto em Operar alterna a lanterna.'],
                    ['Ordem das válvulas: 2, depois 1, depois 3.','Segure Operar por 2s junto ao comando.','Ordem errada reinicia o procedimento.','Toque curto em Operar alterna a lanterna.'],
                    ['Ative os painéis na ordem: 1, 2, 3.','O primeiro inicia 180s de evacuação.','A rede entra em blecaute periodicamente.','Toque curto em Operar alterna a lanterna.']][mode]
    definitions=[('Missao',title[:35].upper(),instructions)]
    for i in range(3): definitions.append(('Setor'+str(i+1),'SETOR '+str(i+1),['Controle '+str(i+1)+' / manter Operar por 2s','Oito segmentos mostram o progresso.','Soltar ou sair do alcance cancela.','Lanterna: botão próprio / tecla F.']))
    (project/'Assets/signage.glb').write_bytes(readable_signs(definitions))
    for name,pos in [('Missao',(4,1.8,5))]+[('Setor'+str(i+1),(-2 if i!=1 else 2,2.2,6+i*16)) for i in range(3)]:
        identity=base.guid('glb:'+base.guid('fonte:Assets/signage.glb')+':'+name+'/'+name+'#0')
        s.add('Placa '+name,mission,1,pos=pos,scale=(1.25,.95,1),components=(kit.source_mesh_component(identity),))
        for offset in [-1.1,1.1]:shape('Suporte placa '+name+str(offset),'Steel',mission,(pos[0]+offset,pos[1]/2,pos[2]+.05),(.025,pos[1]/2,.025),metallic=.7,rough=.6)
    lines=s.archive().splitlines()
    lines[0]=lines[0].replace('AETHER_EDITOR 12','AETHER_EDITOR 17')
    for i in range(1,len(s.entities)+1): lines[i]+=' "Untagged" GROUPS 1 0'
    lines[-1]=action_map()
    lines.append('VIEWS 0')
    (project/'scenes/editor.aescene').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    for file in ['BackroomsExpedition.cs','LightProbe.cs']:
        shutil.copyfile(ROOT/'authoring'/file,project/'Scripts'/file)
    (project/f'Scripts/{behavior}.cs').write_text(f'using Astra;\n[ComponentId("project.{behavior}")]\npublic sealed class {behavior} : BackroomsExpedition\n{{ protected override int Mode => {mode}; }}\n',encoding='utf-8')
    (project/'project.json').write_text(json.dumps({'format':'ASTRA-PROJECT-1','resourceSource':'independent','mainScene':'scenes/editor.aescene','editorScene':'scenes/editor.aescene','project':{'name':title,'path':str(project),'template':'empty','scenes':1,'assets':len(list((project/'Assets').iterdir()))+1}},ensure_ascii=False,indent=2),encoding='utf-8')
    paths=sorted(str(p.relative_to(project)).replace('\\','/') for sub in ['Assets','Audio'] for p in (project/sub).iterdir())
    registry=['AETHER_ASSETS 1 '+str(len(paths))]
    for path in paths:
        kind,recipe=('audio_clip','WAV_PCM_STEREO_F32_48000_V1') if path.endswith('.wav') else ('mesh','glb')
        sha=hashlib.sha256((project/path).read_bytes()).hexdigest()
        registry.append(f'{base.guid("fonte:"+path)} {kind} "{path}" "{path}" "{sha}" 1 "{recipe}" 0 0')
    (project/'.astra/assets.astra').write_text('\n'.join(registry)+'\n',encoding='utf-8')
    print(slug,'entities=',len(s.entities),'assets=',len(paths),flush=True)
    return {'slug':slug,'entities':len(s.entities),'assets':len(paths),'maximumGraphics':mode==1}

if __name__=='__main__':
    report=[make_project(*game) for game in GAMES]
    (ROOT/'production.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
