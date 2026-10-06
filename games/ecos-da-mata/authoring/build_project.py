from pathlib import Path
import importlib.util,json,hashlib,shutil,random,math,wave,struct,zipfile
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parents[1];PROJECT=ROOT/'project'
def load(name,path):
    sp=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m);return m
kit=load('kit',REPO/'tools/generate-highlevel-games.py');base=kit.base;kit.OUT=ROOT
sign=load('sign',REPO/'games/realism-collection/authoring/signage.py')
# Correct winding in exported game primitives, with the existing production geometry.
def outward(fn):
    def generate(*args,**kw):
        p,n,uv,idx=fn(*args,**kw);idx=list(idx)
        for k in range(0,len(idx),3):
            a,b,c=idx[k:k+3];u=[p[b][j]-p[a][j] for j in range(3)];v=[p[c][j]-p[a][j] for j in range(3)];cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
            if sum(cross[j]*(n[a][j]+n[b][j]+n[c][j]) for j in range(3))<0:idx[k+1],idx[k+2]=c,b
        return p,n,uv,idx
    return generate
for p in ['cube','sphere','cylinder','torus']:setattr(kit,p,outward(getattr(kit,p)))
def patch(scene,id,comps):
    e=list(scene.entities[id-1]);e[7]=tuple(e[7])+tuple(comps);scene.entities[id-1]=tuple(e)
def inactive(scene,id):
    hidden.add(id)
def audio(clip,volume=.5,spatial=True):
    return ('astra.audio.source',1,f'{base.guid("fonte:Audio/"+clip+".wav")} 0 1 1 0 0 {int(spatial)} 0 {volume} 1 0 2 24 1 360 360 0 0')
def tween(dest):return ('astra.tween.transform',3,' '.join(map(base.f,[1,0,0,0,1,0,0,1.6,0,1,1,*dest,0,0,0,1,1,1,0,0,0])))
def lightcomp(kind,color,power,radius=12):return ('astra.render.light',3,' '.join(map(base.f,[kind,1,0,0,2 if kind==0 else 0,2,*color,6500,power,radius,20,35,1,.015,.08,.1])))
base.light_component=lightcomp
assets=PROJECT/'Assets';maps=ROOT/'sources/materials';maps.mkdir(parents=True,exist_ok=True)
for category,source in [('concrete',ROOT/'sources/forest_ground_04'),('rock',REPO/'games/realism-collection/sources/quarry_wall'),('wood',REPO/'games/realism-collection/sources/wood_planks_dirt'),('steel',REPO/'games/realism-collection/sources/rusty_metal')]:
    if not source.exists():
        source=REPO/'games/realism-collection/projects/carga-bruta/ArtSources'
        for name in ['base.jpg','normal.png','arm.png']:shutil.copyfile(source/(category+'-'+name),maps/(category+'-'+name))
    else:
        for name in ['base.jpg','normal.png','arm.png']:shutil.copyfile(source/name,maps/(category+'-'+name))
from PIL import Image
for image in maps.glob('*'):
    im=Image.open(image);im.thumbnail((1024,1024),Image.Resampling.LANCZOS);im.save(image)
(assets/'world.glb').write_bytes(kit.world_glb(texture_root=maps))
audio_dir=PROJECT/'Audio';audio_dir.mkdir(exist_ok=True)
for number,freq in enumerate([440,554.37,659.25],1):
    with wave.open(str(audio_dir/f'beacon{number}.wav'),'wb') as out:
        out.setparams((2,2,48000,0,'NONE','not compressed'));samples=bytearray()
        for t in range(48000*3):
            seconds=t/48000;p=seconds%1;env=max(0,1-p/.23) if p<.23 else 0
            value=int(10000*env*math.sin(2*math.pi*freq*seconds));samples.extend(struct.pack('<hh',value,value))
        out.writeframes(samples)
s=kit.Scene();hidden=set();root=s.add('Reserva de Vale Frio');terrain=s.add('Solo e limites',root);forest=s.add('Floresta CC0',root);mission=s.add('Rede de comunicação',root)
env=(REPO/'games/realism-collection/authoring/environment-quarry.txt').read_text().split();env[6]='0';env[18]='1';env[19]='1';env[17]=base.guid('fonte:Assets/forest_slope.hdr');env[37]='0.003';env[38]='25';env[41]='0';env[44]='1';env[45]='1';env[58]='1';env[59]='1';env[27:30]=['0.18','0.24','0.20'];env[24:27]=['0.4','0.5','0.44']
s.add('Atmosfera da floresta',root,components=(('astra.render.environment',12,' '.join(env)),))
kit.shape(s,'Solo florestal PBR','Ground',terrain,(0,-.22,0),(23,.22,32),solid=True)
# Keep the winding trail clear while composing dense peripheral tree silhouettes.
rng=random.Random(20261001)
for i in range(44):
    z=-27+(i//2)*2.65;x=(-1 if i%2 else 1)*rng.uniform(7,20)
    model='fir_tree_01' if i%3 else 'pine_tree_01';scale=rng.uniform(.72,1.12)
    tree=kit.real_prop(s,'project',model,f'Árvore {i+1:02d}',forest,(x,0,z),(scale,)*3)
    kit.collision_proxy(s,tree,'Tronco sólido '+str(i+1),(0,2,0),(.3,2,.3))
for i in range(34):
    z=rng.uniform(-27,27);x=rng.choice([-1,1])*rng.uniform(4.5,17)
    kit.real_prop(s,'project','fern_02',f'Samambaia {i+1:02d}',forest,(x,.02,z),(rng.uniform(.8,1.5),)*3)
for i in range(8):
    x=rng.choice([-1,1])*rng.uniform(6,19);z=rng.uniform(-24,24)
    kit.real_prop(s,'project','rock_moss_set_01',f'Pedras com musgo {i+1}',forest,(x,0,z),(2,)*3)
for i in range(3):kit.real_prop(s,'project','dead_tree_trunk',f'Tronco caído {i+1}',forest,((-1 if i%2 else 1)*8,0,-15+i*13),(1.5,)*3)
for x in [-23,23]:s.add('Limite lateral '+str(x),terrain,pos=(x,2,0),components=(base.body_component(0),base.collider_component((.3,2,32))))
for z in [-32,32]:s.add('Limite extremo '+str(z),terrain,pos=(0,2,z),components=(base.body_component(0),base.collider_component((23,2,.3))))
for i,(x,z) in enumerate([(-2,-11),(2,2),(-2,15)],1):
    station=s.add('Estação '+str(i),mission,pos=(x,0,z))
    kit.shape(s,'Base da estação '+str(i),'Concrete',station,(0,.3,0),(.75,.3,.65),solid=True)
    kit.shape(s,'Equipamento '+str(i),'Steel',station,(0,1.1,0),(.5,.5,.25),(.5,.56,.46),solid=True)
    antenna=kit.shape(s,'Antena '+str(i),'Pipe',station,(0,1.85,0),(.05,.8,.05),extra=(tween((0,2.8,0)),))
    s.add('Sinal '+str(i),station,pos=(0,1.5,0),components=(audio('beacon'+str(i)),))
    for k in range(8):
        led=kit.shape(s,f'LED {i} {k}','Lamp',station,(-.36+k*.103,1.15,-.27),(.035,.075,.025),(.1,.8,.35),emission=(.1,.8,.35),strength=3,uv_world=False);inactive(s,led)
    l=kit.light(s,'Luz confirmada '+str(i),station,(0,2.4,0),(.24,1,.38),9,5);inactive(s,l)
    kit.shape(s,'Marcador da estação '+str(i),'Timber',station,(1.1,1.3,0),(.06,1.3,.06))
# Physical gate and moving visual share the same authored footprint until completion.
gate=kit.shape(s,'Portão de extração','Timber',mission,(0,1.2,22),(3,1.2,.2),extra=(tween((0,5,22)),))
s.add('Colisão do portão',mission,pos=(0,1.2,22),components=(base.body_component(0),base.collider_component((3,1.2,.2))))
for x in [-6,6]:kit.shape(s,'Cerca de extração '+str(x),'Timber',mission,(x,1.2,22),(3,1.2,.2),solid=True)
for z in range(-23,26,5):
    for x in [-3.8,3.8]:kit.shape(s,'Baliza '+str((x,z)),'Timber',terrain,(x,.35,z),(.08,.35,.08))
p=kit.player(s,'Guarda florestal',root,(0,0,-24),'ForestExpedition',speed=4.5,jump=4.8,fov=76)
patch(s,p,[('astra.time.timer',4,'600 0 1 0 0 0 0')]);patch(s,p+1,[('astra.audio.listener',1,'1 1 100')])
# Directional radiance and ambient IBL come from the same imported HDR environment.
definitions=[('Missao','ECOS DA MATA',['Siga os sinais das três estações.','Aproxime-se e segure Calibrar por 2s.','Soltar ou afastar-se cancela a leitura.','Complete a rede e atravesse o portão.']),('Estacao','REDE  /  CALIBRAÇÃO',['8 luzes mostram o avanço da leitura.','Antena elevada = estação calibrada.','Cada sinal tem uma frequência própria.','A janela de comunicação dura 10 min.'])]
(assets/'signage.glb').write_bytes(sign.make_signs(definitions))
for name,pos in [('Missao',(0,2.1,-20)),('Estacao',(4,2,0))]:
    identity=base.guid('glb:'+base.guid('fonte:Assets/signage.glb')+':'+name+'/'+name+'#0');s.add('Placa '+name,mission,1,pos=pos,scale=(1.65,1.65,1),components=(kit.source_mesh_component(identity),))
# Version 17 carries real interaction policy, tags, groups and view state.
lines=s.archive().splitlines();lines[0]=lines[0].replace('AETHER_EDITOR 12','AETHER_EDITOR 17')
for i in range(1,len(s.entities)+1):
    if i in hidden:
        import shlex
        # Rewrite the fixed flags through entity data rather than parsing names with spaces.
        e=s.entities[i-1];prefix=' '.join(map(str,[e[0],e[1],e[2],base.quoted(e[3]),*map(base.f,e[4]),*map(base.f,e[5]),*map(base.f,e[6])]))
        lines[i]=lines[i].replace(prefix+' 1 1 1 1 ',prefix+' 0 1 1 1 ',1)
    lines[i]+=' "Untagged" GROUPS 1 0'
actions='INPUT ASTRA_ACTION_MAP 2 4 "Mover" "Olhar" "Saltar"'
for name,kind,dead,interaction,duration,bindings in [('Mover',2,.12,0,.4,[(1,0,0,0,1,0),(1,0,0,1,1,0)]),('Olhar',2,0,0,.4,[(2,0,0,0,1,0),(2,0,0,1,1,0)]),('Saltar',0,0,0,.4,[(3,0,0,0,1,0)]),('Calibrar',0,0,1,2,[(3,1,0,0,1,0),(4,33,0,0,1,0)])]:
    actions+=' '+base.quoted(name)+f' {kind} {dead} 1 "" {len(bindings)} 1 {interaction} {duration} 7'
    for binding in bindings:actions+=' '+' '.join(map(str,binding))
lines[-1]=actions;lines.append('VIEWS 0')
scenes=PROJECT/'scenes';scenes.mkdir(exist_ok=True);(scenes/'editor.aescene').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(PROJECT/'Scripts').mkdir(exist_ok=True);shutil.copyfile(ROOT/'authoring/ForestExpedition.cs',PROJECT/'Scripts/ForestExpedition.cs')
(PROJECT/'project.json').write_text(json.dumps({'format':'ASTRA-PROJECT-1','resourceSource':'independent','mainScene':'scenes/editor.aescene','editorScene':'scenes/editor.aescene','project':{'name':'Ecos da Mata','path':str(PROJECT),'template':'empty','scenes':1,'assets':10}},indent=2,ensure_ascii=False),encoding='utf-8')
paths=sorted(str(p.relative_to(PROJECT)).replace('\\','/') for p in assets.iterdir() if p.suffix in ['.glb','.hdr'])+['Audio/beacon'+str(i)+'.wav' for i in range(1,4)]
registry=['AETHER_ASSETS 1 '+str(len(paths))]
for path in paths:
    kind,recipe=('audio_clip','WAV_PCM_STEREO_F32_48000_V1') if path.endswith('.wav') else ('environment_map','AEM_IMPORT 1 1024 128 64 128 128') if path.endswith('.hdr') else ('mesh','glb');sha=hashlib.sha256((PROJECT/path).read_bytes()).hexdigest();registry.append(f'{base.guid("fonte:"+path)} {kind} "{path}" "{path}" "{sha}" 1 "{recipe}" 0 0')
(PROJECT/'.astra').mkdir(exist_ok=True);(PROJECT/'.astra/assets.astra').write_text('\n'.join(registry)+'\n',encoding='utf-8')
print('PROJECT',len(s.entities),'objects',len(paths),'assets',flush=True)
