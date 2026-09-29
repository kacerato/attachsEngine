"""Author standalone projects. Never writes engine, bundled examples or APK files."""
from pathlib import Path
import hashlib, importlib.util, json, math, shutil
from signage import make_signs

ROOT=Path(__file__).resolve().parents[1]
REPO=ROOT.parents[1]
OUT=ROOT/'projects'
SOURCE=ROOT/'sources'
spec=importlib.util.spec_from_file_location('kit',REPO/'tools/generate-highlevel-games.py')
kit=importlib.util.module_from_spec(spec); spec.loader.exec_module(kit)
base=kit.base; kit.OUT=OUT
# Repair winding in the GAME'S exported geometry. Existing engine and kit files stay intact.
def outward(generator):
    def wrapped(*args,**kwargs):
        p,n,uv,indices=generator(*args,**kwargs)
        indices=list(indices)
        for k in range(0,len(indices),3):
            a,b,c=indices[k:k+3]
            u=[p[b][j]-p[a][j] for j in range(3)]; v=[p[c][j]-p[a][j] for j in range(3)]
            cross=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
            if sum(cross[j]*(n[a][j]+n[b][j]+n[c][j]) for j in range(3))<0:
                indices[k+1],indices[k+2]=c,b
        return p,n,uv,indices
    return wrapped
for primitive in ('cube','sphere','cylinder','torus'):
    setattr(kit,primitive,outward(getattr(kit,primitive)))
# The shared kit's Rock slot is a noisy sphere. These projects use that slot for
# masonry and floors with box colliders, so author matching closed box geometry.
rounded_sphere=kit.sphere
def project_sphere(noise=0,*args,**kwargs):
    return kit.cube() if noise==.12 else rounded_sphere(noise,*args,**kwargs)
kit.sphere=project_sphere

def project_light(kind,color,intensity,radius=12):
    # v3 with the existing Engine intensity scale and soft spotlight shadows.
    shadow=2 if kind==2 else 0
    return ('astra.render.light',3,' '.join(map(base.f,[kind,1,0,0,shadow,2,*color,6500,intensity,radius,20,35,1,.015,.08,.1])))
base.light_component=project_light
GAMES={'usina-17':('Usina 17',0,['portable_generator','industrial_storage_cart','modular_industrial_pipes_01']),
       'carga-bruta':('Carga Bruta',1,['wooden_crate_01','rock_07','concrete_road_barrier']),
       'sepulcro-calcario':('Sepulcro de Calcário',2,['antique_ceramic_vase_01','stone_01','concrete_cat_statue'])}

def body(mass=10,motion=2):
    return ('astra.physics.body',3,' '.join(map(base.f,[motion,mass,.7,.03,0,0,0,0,0,0,.07,.5,1,0,1])))

def joint(kind,connected,anchor=(0,0,0),other=(0,0,0),axis=(0,1,0),limits=(0,1)):
    return ('astra.physics.joint',1,' '.join(map(base.f,[kind,0,connected,1,*anchor,*other,*axis,*axis,*limits,0,0,1000,2,1])))

def patch_components(s,entity,components):
    e=list(s.entities[entity-1]); e[7]=tuple(e[7])+tuple(components); s.entities[entity-1]=tuple(e)

def visual(s,name,parent,pos,half,mesh='Steel',color=(1,1,1),solid=False,rot=(0,0,0)):
    moving=False; ancestor=parent
    while ancestor:
        node=s.entities[ancestor-1]
        moving |= any(c[0]=='astra.physics.body' and c[2].split()[0] in ('1','2') for c in node[7])
        ancestor=node[1]
    return kit.shape(s,name,mesh,parent,pos,half,color,solid=solid,rot=rot,uv_world=not moving)

def rigid(s,name,pos,half,mass,mesh='Steel',color=(1,1,1),parent=2):
    root=s.add(name,parent,pos=pos,components=(body(mass),base.collider_component(half)))
    if mesh: visual(s,name+' · superfície',root,(0,0,0),half,mesh,color)
    return root

def anchor(s,name,pos):
    return s.add(name,2,pos=pos,components=(body(1,0),base.collider_component((.04,.04,.04))))

def slider(s,name,pos,half,mass,limits,mesh='Steel'):
    offset=half[0]+1
    a=anchor(s,'Trilho · '+name,(pos[0]+offset,pos[1],pos[2]))
    obj=rigid(s,name,pos,half,mass,mesh)
    # Joint displacement is connected body B relative to moving body A.
    patch_components(s,obj,[joint(2,a,other=(-offset,0,0),limits=(-limits[1],-limits[0]))])
    return obj

def prop(s,game,model,label,pos,scale=1,parent=2):
    return kit.real_prop(s,game,model,label,parent,pos,(scale,)*3)

def setup(theme='usina'):
    s=kit.Scene(); s.add('Mundo')
    environment=(ROOT/('authoring/environment-'+theme+'.txt')).read_text().split()
    if theme=='quarry':
        environment[6]='0' # Existing HDRI sky model; lighting is imported by Astra.
        environment[17]=base.guid('fonte:Assets/quarry_03.hdr')
    s.add('Ambiente da missão',2,components=(('astra.render.environment',12,' '.join(environment)),))
    return s

def actor(s,pos):
    return kit.player(s,'Operador',2,pos,'Expedition',speed=4.5,jump=5)

def lamp(s,name,pos,power=18,color=(1,.83,.63),parent=2):
    return kit.light(s,name,parent,pos,color,power,radius=9)

def walls(s,width,zmin,zmax,height=4,mesh='Concrete'):
    visual(s,'Piso',2,(0,-.25,(zmin+zmax)/2),(width,.25,(zmax-zmin)/2),mesh,solid=True)
    for x in (-width,width):
        visual(s,'Parede '+str(x),2,(x,height/2,(zmin+zmax)/2),(.3,height/2,(zmax-zmin)/2),mesh,solid=True)
    for z in (zmin,zmax): visual(s,'Fundo '+str(z),2,(0,height/2,z),(width,height/2,.3),mesh,solid=True)

def usina():
    s=setup(); walls(s,10,-23,24,5)
    # Separate halls and working gallery; open clerestory prevents a falsely enclosed GI claim.
    for z in (-9,8):
        for side in (-1,1): visual(s,'Divisória '+str((z,side)),2,(side*6,2.5,z),(4,2.5,.22),solid=True)
    for z in (-15,1,17): visual(s,'Cobertura '+str(z),2,(0,5.15,z),(10,.2,5),'Concrete')
    for z in range(-20,24,5):
        for x in (-9.5,9.5):
            visual(s,'Pilar '+str((x,z)),2,(x,2.5,z),(.25,2.5,.32),solid=True)
            visual(s,'Viga '+str((x,z)),2,(x/2,4.7,z),(4.8,.18,.18))
            visual(s,'Linha de tubulação '+str((x,z)),2,(x,.8,z),(.12,2.5,.12),'Pipe',rot=(90,0,0))
    for x in (-7,7): visual(s,'Galeria '+str(x),2,(x,3.2,16),(2.7,.2,7),'Concrete',solid=True)
    # Elevator reaches the gallery at x=7; guard rails keep cargo on platform.
    lift=slider(s,'Elevador',(7,.25,6),(1.6,.22,1.5),80,(0,3.4),'Hazard')
    for x in (-1.5,1.5):
        visual(s,'Guarda do elevador '+str(x),lift,(x,.8,0),(.07,.6,1.45))
    rigid(s,'Comando do elevador',(4.9,1,5),(.25,.5,.2),1)
    # Convert controls to static bodies without introducing new components.
    for e in list(s.entities):
        if e[3]=='Comando do elevador':
            v=list(e); v[7]=(body(1,0),base.collider_component((.25,.5,.2))); s.entities[e[0]-1]=tuple(v)
    a=anchor(s,'Batente da oficina',(-2,2,-9))
    door=rigid(s,'Porta da oficina',(0,2,-9),(1.9,2,.15),45,'Steel',(.4,.52,.49))
    patch_components(s,door,[joint(1,a,(-2,0,0),limits=(-100,5))])
    for name,pos in [('Painel do gerador',(-7,1.2,-3)),('Seletor de circuito',(-6,1.2,2)),('Amostra',(7,4,19))]:
        root=s.add(name,2,pos=pos,components=(body(1,0),base.collider_component((.45,.45,.3))))
        visual(s,name+' · caixa',root,(0,0,0),(.45,.45,.3),'Steel',(.7,.72,.68))
        visual(s,name+' · indicador',root,(0,.2,-.32),(.12,.06,.02),'Lamp',(.15,.9,.32))
    for i,pos in enumerate([(-7,1,-17),(7,1,-12)]): rigid(s,'Fusível '+str(i+1),pos,(.12,.28,.12),.7,'Steel',(.8,.5,.18))
    rigid(s,'Bateria',(-6,1,-6),(.32,.4,.25),18,'Steel',(.24,.3,.24))
    for i in range(10): rigid(s,'Caixa móvel '+str(i),((-1 if i%2 else 1)*(3+i%4),.7,-14+i*3),(.4,.45,.5),8+i,'Timber')
    prop(s,'usina-17','portable_generator','Gerador diesel',(-8,0,-2),1.8)
    prop(s,'usina-17','industrial_storage_cart','Carrinho da oficina',(5,0,-5),1.5)
    for z in (-15,0,15): prop(s,'usina-17','modular_industrial_pipes_01','Conjunto de tubos '+str(z),(-9,0,z),1.3)
    group=s.add('Luzes de trabalho',2)
    for z in (-15,0,17):
        kit.light(s,'Luz de trabalho '+str(z),group,(0,4.5,z),(1,.82,.59),8,11,kind=2,rot=(90,0,0))
        lamp(s,'Emergência '+str(z),(0,3.8,z),24,(1,.67,.35))
        visual(s,'Luminária de emergência '+str(z),2,(0,3.95,z),(.6,.06,.15),'Lamp',(.8,.6,.3))
    p=actor(s,(0,0,-20)); kit.light(s,'Lanterna',p+1,(.15,-.1,.3),(1,.92,.8),35,12,kind=2)
    kit.light(s,'Sol',2,(0,20,0),(1,.9,.76),.65,kind=0,rot=(62,28,0))
    return s

def quarry():
    s=setup('quarry')
    visual(s,'Plataforma principal',2,(0,-.5,0),(14,.5,45),'Rock',solid=True)
    for z in range(-40,45,6):
        for side in (-1,1):
            prop(s,'carga-bruta','rock_07','Escarpa '+str((side,z)),(side*(16+z%3),-1,z),25+(z%4)*3)
            if z%12==2: prop(s,'carga-bruta','concrete_road_barrier','Barreira '+str((side,z)),(side*12,0,z),1.7)
    for z,angle in [(-5,-8),(8,8)]: visual(s,'Rampa '+str(z),2,(0,.48,z),(5,.3,5),'Concrete',solid=True,rot=(angle,0,0))
    for x in (-6,6): visual(s,'Desvio de terra '+str(x),2,(x,.12,4),(1.8,.15,15),'Rock',solid=True)
    for i in range(12): visual(s,'Pedra do piso '+str(i),2,((-1 if i%2 else 1)*2,.08,-1+i*.7),(.3,.12,.2),'Rock',solid=True)
    car=rigid(s,'Transportador',(0,1.1,-27),(1,.3,1.8),600,'Steel',(.8,.53,.17))
    for x in (-.8,.8):
        visual(s,'Longarina '+str(x),car,(x,-.25,0),(.08,.1,1.85))
        for z in (-1.65,1.65):
            visual(s,'Para-lama '+str((x,z)),car,(x,.12,z),(.32,.08,.46),'Steel',(.75,.5,.15))
            visual(s,'Coluna de proteção '+str((x,z)),car,(x,1.2,z),(.055,.95,.055),'Pipe',(.18,.2,.17))
    for z in (-1.65,1.65):
        visual(s,'Travessa de proteção '+str(z),car,(0,2.1,z),(.82,.055,.055),'Steel',(.18,.2,.17))
    for i in range(12): visual(s,'Prancha de caçamba '+str(i),car,(0,.32,-1.6+i*.28),(.91,.04,.13),'Timber')
    for x in (-.67,.67):
        visual(s,'Farol '+str(x),car,(x,0,1.84),(.17,.12,.055),'Lamp',(.9,.86,.68))
        visual(s,'Lanterna traseira '+str(x),car,(x,0,-1.84),(.16,.07,.035),'Lamp',(.62,.055,.025))
    visual(s,'Compartimento elétrico',car,(0,.55,1.3),(.9,.25,.25),'Steel',(.75,.5,.15))
    for i in range(9): visual(s,'Grade ventilação '+str(i),car,(-.65+i*.16,.53,1.56),(.025,.14,.014),'Steel',(.12,.14,.12))
    # Rails are colliders explicitly attached to the chassis body, not separate statics.
    for x in (-.97,.97):
        visual(s,'Lateral de carga '+str(x),car,(x,.6,0),(.06,.3,1.7),'Steel',(.8,.53,.17))
        kit.collision_proxy(s,car,'Colisor lateral '+str(x),(x,.6,0),(.06,.3,1.7),body=False)
    for i,(x,z) in enumerate([(-.86,-1.22),(.86,-1.22),(-.86,1.22),(.86,1.22)]):
        wheel=s.add('Roda '+str(i),car,pos=(x,-.6,z))
        visual(s,'Pneu '+str(i),wheel,(0,0,0),(.43,.24,.43),'Pipe',(.075,.07,.06),rot=(0,0,90))
        visual(s,'Cubo '+str(i),wheel,(0,0,0),(.24,.255,.24),'Pipe',(.5,.5,.46),rot=(0,0,90))
    for i,pos in enumerate([(-4,.65,-27),(-4,.65,-25),(-4,.65,-23)]):
        cargo=rigid(s,'Carga '+str(i+1),pos,(.45,.45,.5),20+i*10,None)
        prop(s,'carga-bruta','wooden_crate_01','Caixa escaneada '+str(i),(0,-.45,0),.9,cargo)
    table=s.add('Mesa de controle',2,pos=(4,1,-29),components=(body(1,0),base.collider_component((.8,.45,.5))))
    visual(s,'Mesa de operação',table,(0,0,0),(.8,.45,.5),'Steel',(.33,.4,.37))
    slider(s,'Plataforma de carga',(-5,.25,-25),(1.9,.2,2.5),110,(0,2),'Hazard')
    command=s.add('Comando da plataforma',2,pos=(-7,1,-28),components=(body(1,0),base.collider_component((.25,.6,.3))))
    visual(s,'Interruptor',command,(0,0,0),(.25,.6,.3),'Steel',(.8,.6,.2))
    visual(s,'Zona de entrega',2,(0,.04,33),(3.8,.035,3.8),'Hazard')
    # A second authored inspection mechanism stresses a distance joint without pretending to simulate a cable.
    a=anchor(s,'Cabeça do pórtico',(8,6,29))
    suspended=rigid(s,'Carga suspensa',(8,2,29),(.65,.6,.65),45,'Timber')
    patch_components(s,suspended,[joint(3,a,limits=(4,4))])
    for x in (6,10): visual(s,'Pilar de pórtico '+str(x),2,(x,3,29),(.18,3,.2),solid=True)
    visual(s,'Travessa do pórtico',2,(8,6,29),(2.3,.25,.25))
    actor(s,(4,0,-32))
    drive_camera=base.camera_component(False,5,200)
    drive_camera=(drive_camera[0],drive_camera[1],'0'+drive_camera[2][1:])
    s.add('Câmera de condução',2,3,(0,5,-35),(18,0,0),components=(drive_camera,))
    kit.light(s,'Sol da pedreira',2,(0,30,0),(1,.91,.79),2.2,kind=0,rot=(42,-35,0))
    return s

def tomb():
    s=setup('tomb'); walls(s,10,-24,26,7,mesh='Rock')
    for z in (-20,-12,0,12,22):
        visual(s,'Laje de cobertura '+str(z),2,(0,7.1,z),(10,.3,3.6),'Rock',solid=True)
    # Replace the continuous floor by two ledges with a real gap under the bridge.
    floor=next(e for e in s.entities if e[3]=='Piso')
    e=list(floor); e[4]=(0,-.25,-10); e[6]=(10,.25,14); s.entities[e[0]-1]=tuple(e)
    visual(s,'Piso da câmara',2,(0,-.25,18),(10,.25,8),'Rock',solid=True)
    visual(s,'Fundo do fosso',2,(0,-4.25,7),(10,.25,3),'Rock',solid=True)
    for z in range(-19,25,7):
        for x in (-7.8,7.8):
            visual(s,'Coluna '+str((x,z)),2,(x,3,z),(.75,3,.75),'Pipe',(.82,.74,.58),solid=True)
            visual(s,'Capitel '+str((x,z)),2,(x,5.8,z),(1.05,.32,1.05),'Concrete')
            prop(s,'sepulcro-calcario','stone_01','Pedra escaneada '+str((x,z)),(x,0,z+2),4)
    for z in (-7,7):
        for side in (-1,1): visual(s,'Muro interno '+str((z,side)),2,(side*6,3,z),(4,3,.65),'Rock',solid=True)
        visual(s,'Lintel '+str(z),2,(0,8 if z==-7 else 5.8,z),(2.3,.5,.75),'Rock')
    slider(s,'Balança',(-4,.55,-12),(1.5,.15,1.5),18,(-.32,0),'Concrete')
    slider(s,'Portão do sepulcro',(0,2.2,-7),(1.85,2.2,.5),140,(0,3.2),'Rock')
    slider(s,'Ponte móvel',(0,4.5,7),(1.5,.22,3.25),90,(-4.2,0),'Concrete')
    for i,mass in enumerate([5,10,15,20]): rigid(s,'Peso '+str(mass)+' kg',(-5+i*2,.65,-18),(.38,.42,.38),mass,'Rock')
    statue=rigid(s,'Estátua',(-4,1.1,-12),(.35,.5,.35),20,None)
    prop(s,'sepulcro-calcario','concrete_cat_statue','Escultura', (0,-.5,0),1,statue)
    for i in range(5): prop(s,'sepulcro-calcario','antique_ceramic_vase_01','Vaso '+str(i),((-1 if i%2 else 1)*6,0,-16+i*8),2)
    lever=s.add('Alavanca da ponte',2,pos=(4,1,-1),components=(body(1,0),base.collider_component((.3,.7,.3))))
    visual(s,'Braço da alavanca',lever,(0,.5,0),(.09,.8,.09),'Steel',(.44,.32,.17))
    for i,z in enumerate((12,17)):
        a=anchor(s,'Âncora de pêndulo '+str(i),(0,6,z))
        pendulum=rigid(s,'Pêndulo '+str(i),(0,2,z),(.7,.7,.7),35,'Rock')
        patch_components(s,pendulum,[joint(1,a,(0,4,0),axis=(0,0,1),limits=(-55,55))])
        # Initial angular velocity is physical, and the chain remains inside joint limits.
        entity=list(s.entities[pendulum-1]); comp=list(entity[7]); fields=comp[0][2].split(); fields[9]='0.65'
        comp[0]=(comp[0][0],comp[0][1],' '.join(fields)); entity[7]=tuple(comp); s.entities[pendulum-1]=tuple(entity)
        visual(s,'Haste de pêndulo '+str(i),pendulum,(0,2,0),(.06,2,.06),'Steel')
    rigid(s,'Artefato',(0,1.2,23),(.2,.32,.2),8,'Steel',(.65,.43,.12))
    visual(s,'Pedestal',2,(0,.45,23),(1,.45,1),'Rock',solid=True)
    support=s.add('Suporte de ruína',2,pos=(6,2.5,21),components=(body(1,1),base.collider_component((1.3,.12,1))))
    visual(s,'Trave antiga',support,(0,0,0),(1.3,.12,1),'Timber')
    for i in range(8): rigid(s,'Fragmento preparado '+str(i),(5.2+(i%3)*.65,3+(i//3)*.7,21),(.27,.32,.35),8,'Rock')
    actor(s,(0,0,-21))
    kit.light(s,'Sol do deserto',2,(0,30,0),(1,.86,.66),1.1,kind=0,rot=(52,20,0))
    for z in (-13,0,13,23):
        lamp(s,'Luz de expedição '+str(z),(0,5,z),20,(1,.8,.56))
        visual(s,'Luminária de expedição '+str(z),2,(0,5.15,z),(.24,.1,.24),'Lamp',(.8,.65,.4))
    return s

def produce():
    from PIL import Image
    surfaces=json.loads((SOURCE/'surfaces.json').read_text())
    for game,(title,mode,models) in GAMES.items():
        project=OUT/game; assets=project/'Assets'; assets.mkdir(parents=True,exist_ok=True)
        for model in models: shutil.copyfile(SOURCE/model/(model+'.glb'),assets/(model+'.glb'))
        if mode==1:
            assert json.loads((SOURCE/'quarry_03/exposure.json').read_text())['sceneCompensationEv']==3
            shutil.copyfile(SOURCE/'quarry_03/quarry_03_rgba16f.hdr',assets/'quarry_03.hdr')
        maps=project/'ArtSources'; maps.mkdir(exist_ok=True)
        names={'concrete':surfaces[2] if mode!=2 else surfaces[4], 'steel':surfaces[1], 'wood':surfaces[3], 'rock':surfaces[0] if mode!=2 else surfaces[4]}
        for dest,source in names.items():
            for role,filename in [('base','base.jpg'),('normal','normal.png'),('arm','arm.png')]:
                shutil.copyfile(SOURCE/source/filename,maps/(dest+'-'+filename))
        # Existing production GLB writer consumes these maps without engine changes.
        (assets/'world.glb').write_bytes(kit.world_glb(texture_root=maps))
        scene=[usina,quarry,tomb][mode]()
        instructions=[
            [('Rota','USINA 17',['1  Recolha 2 fusíveis e a bateria.','2  Instale as peças no gerador.','3  Alimente o elevador. Suba à galeria.','4  Recupere a amostra e volte aqui.']),
             ('Controle','OFICINA  /  ENERGIA',['Toque no objeto: pegar ou usar.','Toque novamente: soltar.','Segure a ação: arremessar.','Circuito: ventilação OU elevador.'])],
            [('Rota','CARGA BRUTA',['1  Carregue as três caixas na caçamba.','2  Use a mesa para controlar o veículo.','3  Mover: acelerar / virar; Saltar: freio.','4  Leve as caixas à faixa de entrega.']),
             ('Controle','POSTO DE OPERAÇÃO',['Usar: entrar/sair do controle remoto.','Não ultrapasse as bordas da pedreira.','Reduza a velocidade nas rampas.','A carga se movimenta por contato.'])],
            [('Rota','SEPULCRO DE CALCÁRIO',['1  Coloque 30 kg sobre a balança.','2  Abra a passagem e baixe a ponte.','3  Atravesse os pêndulos.','4  Traga o artefato até esta entrada.']),
             ('Controle','EQUILÍBRIO',['Estátua: 20 kg. Blocos: 5 / 10 / 15 / 20.','Pesos segurados não entram na soma.','Solte o objeto e espere repousar.','O portão fecha quando falta peso.'])]
        ][mode]
        (assets/'signage.glb').write_bytes(make_signs(instructions))
        sign_source=base.guid('fonte:Assets/signage.glb')
        for index,(name,_,_) in enumerate(instructions):
            identity=base.guid('glb:'+sign_source+':'+name+'/'+name+'#0')
            position=[(-2,2.4,-17),(4,2.5,-27),(-2,2.4,-18)][mode]
            position=(position[0]+index*4,position[1],position[2])
            scene.add('Placa '+name,2,1,pos=position,scale=(1.35,1.35,1),components=(kit.source_mesh_component(identity),))
        scenes=project/'scenes'; scenes.mkdir(exist_ok=True)
        (scenes/'editor.aescene').write_text(scene.archive().replace(base.INPUT,kit.input_archive('Interagir')),encoding='utf-8')
        (scenes/'main.ascene').write_text('{"format":"ASTRA-SCENE-1","nodes":[]}',encoding='utf-8')
        scripts=project/'Scripts'; scripts.mkdir(exist_ok=True)
        (scripts/'Expedition.cs').write_text((ROOT/'authoring/Expedition.cs').read_text(encoding='utf-8').replace('SCENARIO_NUMBER',str(mode)),encoding='utf-8')
        descriptor={'format':'ASTRA-PROJECT-1','resourceSource':'independent','mainScene':'scenes/main.ascene','editorScene':'scenes/editor.aescene',
                    'project':{'name':title,'path':str(project),'template':'empty','scenes':1,'assets':len(models)+2+int(mode==1)}}
        (project/'project.json').write_text(json.dumps(descriptor,indent=2,ensure_ascii=False),encoding='utf-8')
        paths=['Assets/world.glb','Assets/signage.glb']+['Assets/'+m+'.glb' for m in models]
        if mode==1: paths.append('Assets/quarry_03.hdr')
        rows=[f'AETHER_ASSETS 1 {len(paths)}']
        for path in paths:
            sha=hashlib.sha256((project/path).read_bytes()).hexdigest()
            kind,recipe=('environment_map','AEM_IMPORT 1 2048 256 128 256 512') if path.endswith('.hdr') else ('mesh','glb')
            rows.append(f'{base.guid("fonte:"+path)} {kind} "{path}" "{path}" "{sha}" 1 "{recipe}" 0 0')
        registry=project/'.astra'; registry.mkdir(exist_ok=True)
        (registry/'assets.astra').write_text('\n'.join(rows)+'\n',encoding='utf-8')
        (project/'FONTES.md').write_text('# Fontes de arte\n\nModelos 2K e mapas PBR 2K originais de Poly Haven (CC0).\n\n'+
            '\n'.join('- https://polyhaven.com/a/'+m for m in models+list(dict.fromkeys(names.values())))+
            ('\n- https://polyhaven.com/a/quarry_03' if mode==1 else '')+
            '\n\nArquitetura e mecanismos são conteúdo autoral. Não há lightmap/GI assada integrada.\n',encoding='utf-8')
        print(game,len(scene.entities),'objects',sum(len(e[7]) for e in scene.entities),'components',flush=True)

if __name__=='__main__': produce()
