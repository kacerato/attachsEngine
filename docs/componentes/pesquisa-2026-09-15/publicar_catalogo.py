"""Publish a bounded, inspectable reference atlas. No Astra execution."""
from pathlib import Path
from collections import defaultdict, Counter
import json,re,html

OUT=Path(__file__).resolve().parent
ROOT=OUT.parents[2]
raw=json.loads((ROOT/'build/component-reference-research/tipos-extraidos-20260915.json').read_text(encoding='utf-8'))
api=json.loads((OUT/'api-nucleo.json').read_text(encoding='utf-8'))
old=json.loads((ROOT/'docs/componentes/catalogo.json').read_text(encoding='utf-8'))
core={x['name']:x for x in old['entries'] if x.get('engine')=='Unity'}
types={t['package']+'::'+t['fullName']:dict(t,id=t['package']+'::'+t['fullName']) for t in raw['types']}
names=defaultdict(list)
for t in types.values():
    names[t['name']].append(t['id']);names[t['fullName']].append(t['id'])

def resolve(name,t):
    name=re.sub(r'<.*','',name,flags=re.S).strip().replace('global::','').replace('?','').replace('[]','')
    if not name:return None
    parts=t['fullName'].split('.')[:-1]
    for n in ['.'.join(parts[:i]+[name]) for i in range(len(parts),-1,-1)]:
        c=list(dict.fromkeys(names.get(n,[])))
        same=[k for k in c if types[k]['package']==t['package']]
        if len(same)==1:return same[0]
        if len(c)==1:return c[0]
    imported=list(dict.fromkeys(k for ns in t.get('usingNamespaces',[]) for k in names.get(ns+'.'+name,[])))
    if len(imported)==1:return imported[0]
    c=list(dict.fromkeys(names.get(name.rsplit('.',1)[-1],[])))
    same=[k for k in c if types[k]['package']==t['package']]
    return same[0] if len(same)==1 else c[0] if len(c)==1 else None

base={k:[r for b in t['bases'] if (r:=resolve(b,t)) and r!=k] for k,t in types.items()}
def ancestry(k,seen=None):
    seen=set() if seen is None else seen
    if k in seen:return []
    seen.add(k);result=[]
    for b in base[k]:
        result.append(b);result.extend(ancestry(b,seen))
    return list(dict.fromkeys(result))
anc={k:ancestry(k) for k in types}
def desc(k,name):return types[k]['fullName']==name or any(types[b]['fullName']==name for b in anc[k])
def public(t):return 'public' in t['modifiers'] and not re.search(r'(DocCodeExamples|\.Samples\.|\.Tests\.|\.Examples\.)',t['fullName'])

selected={}
for k,t in types.items():
    if not public(t):continue
    if desc(k,'UnityEngine.Component'):
        if t['package']=='UnityEngine' and t['name'] not in core:continue
        selected[k]='Component'
    elif desc(k,'UnityEngine.Rendering.VolumeComponent'):selected[k]='Volume profile'
    elif desc(k,'UnityEngine.UIElements.VisualElement'):selected[k]='UI Toolkit element'
    elif t['name'] in ['GameObject','Object','Mesh','Material','Texture','Texture2D','Texture2DArray','Texture3D','Cubemap','CubemapArray','RenderTexture','CustomRenderTexture','Shader','ComputeShader','Sprite','AnimationClip','RuntimeAnimatorController','AnimatorOverrideController','Avatar','AvatarMask','AudioClip','AudioMixer','AudioMixerGroup','AudioMixerSnapshot','VideoClip','TerrainData','TerrainLayer','PhysicsMaterial','PhysicMaterial','PhysicsMaterial2D','BillboardAsset','RenderPipelineAsset','VolumeProfile','TimelineAsset','PlayableAsset','InputActionAsset','InputAction','InputActionMap','InputBinding','PanelSettings','VisualTreeAsset','StyleSheet','UniversalRenderPipelineAsset','HDRenderPipelineAsset','ScriptableRendererFeature','DecalRendererFeature','ScreenSpaceAmbientOcclusion','Tile','RuleTile','RuleOverrideTile','TileBase','SpriteLibraryAsset','Spline','SplineData','NavMeshData','NavMeshBuildSettings','NavMeshQueryFilter','Font','TMP_FontAsset','TMP_SpriteAsset']:
        selected[k]='Resource / contract'

# Include bases and the declared member types, including particle modules,
# rig generic data and enum values. This is a structural closure, not a claim
# that every field is an Inspector control or required resource.
queue=list(selected)
while queue:
    k=queue.pop();t=types[k]
    referenced=list(base[k])
    for s in t['bases']+[m['type'] for m in t['members']]:
        for token in re.findall(r'[A-Za-z_]\w*(?:\.\w+)*',s):
            r=resolve(token,t)
            if r:referenced.append(r)
    for r in referenced:
        if r not in selected:
            selected[r]='Supporting type';queue.append(r)

families=[
 ('com.unity.cinemachine','Câmeras procedurais','Criar a câmera de saída e o controlador de câmeras; configurar alvo, enquadramento, prioridade e transições no tipo escolhido. Modificadores operam no estágio de posição, orientação ou lente indicado pela fonte.','RenderView, pose final, targets, relógio e colisão quando solicitada','P03/P12'),
 ('com.unity.animation.rigging','Rig procedural','Criar Animator e RigBuilder na raiz animada, uma hierarquia Rig e constraints; vincular ossos e alvos e ajustar pesos. Os dados concretos do constraint estão nos argumentos genéricos e tipos de apoio.','Esqueleto, animation stream, ordem de avaliação, transform handles','P11'),
 ('com.unity.ai.navigation','Navegação','Definir superfície coletora e geometria, gerar NavMeshData; configurar agente ou obstáculo e ligações. Bake, consulta e movimento são etapas distintas.','Geometria, navegação, identificação de agentes, autoridade de pose','P14'),
 ('com.unity.ugui','UI e texto','Criar Canvas com modo de renderização, hierarquia RectTransform e widgets; para interação configurar EventSystem, input e raycaster pertinentes. Tipografia referencia fonte e material.','Layout, clipping, fontes, eventos, acessibilidade e renderização UI','P13'),
 ('com.unity.inputsystem','Input por ações','Criar InputActionAsset, mapas, ações e bindings; conectar PlayerInput ou o adaptador de UI. Selecionar dispositivos, esquemas e modo de notificação.','Dispositivos, ciclo de input, foco, ações e destinos de eventos','P10'),
 ('com.unity.netcode.gameobjects','Rede de objetos','Configurar transporte e NetworkManager, registrar prefabs com NetworkObject; componentes de sincronização operam sob autoridade e regras de ownership.','Transporte externo, serialização, spawn, IDs, relógio e autoridade','P17'),
 ('com.unity.splines','Curvas espaciais','Criar SplineContainer, editar nós e tangentes; vincular consumidores de movimento, instanciação ou extrusão. Não confundir spline de caminho com curva de animação.','Transform, geometria gerada, recurso de curva e orientação ao longo do caminho','P14'),
 ('com.unity.xr.interaction.toolkit','Interação XR','Configurar origem/rastreio e input; relacionar interactors, interactables e gerenciadores. Selecionar movimento e feedback compatíveis com o dispositivo.','XR Core, Input System, provedores XR, seleção, locomoção e eventos','P17'),
 ('com.unity.xr.arfoundation','Realidade aumentada','Configurar sessão, origem, managers e provedores XR específicos da plataforma; consumers usam trackables disponíveis. Um manager não implementa o provedor de hardware.','XR subsystems, plugin Android, permissões e suporte real de dispositivo','P17'),
 ('com.unity.xr.core-utils','Origem XR','Configurar origem, câmera rastreada e offsets; conectar o provedor e os consumidores de interação.','Rastreio, convenções de espaço, câmera e input','P17'),
 ('com.unity.timeline','Sequenciamento','Criar TimelineAsset e PlayableDirector, adicionar tracks/clips e resolver bindings de objetos; escolher relógio e ciclo de reprodução.','Playable graph, animação, áudio, eventos e referências estáveis','P11'),
 ('com.unity.localization','Localização','Criar tabelas e identificadores de locale; vincular o componente à string ou asset e ao evento de destino. Resolver carregamento e fallback.','Tabelas, Addressables, formatação, fontes e locale','P13/P17'),
 ('com.unity.2d.animation','Animação 2D','Preparar sprite, ossos/pesos ou biblioteca de sprites; vincular SpriteSkin/Resolver ao renderer e à hierarquia correspondente.','Sprites, skinning 2D, biblioteca e autoridade de pose','P15'),
 ('com.unity.2d.spriteshape','Contornos 2D','Configurar perfil, spline e SpriteShapeController; gerar malha e colisão conforme configuração.','Sprites, tesselação, renderer 2D e física opcional','P15'),
 ('com.unity.2d.tilemap.extras','Tiles por regras','Criar tile/regras e paleta; pintar uma Tilemap dentro de Grid e configurar consumidores de renderização/colisão.','Tilemap, vizinhança, sprites e recursos de tile','P15'),
 ('com.unity.addressables','Carregamento de assets','Definir endereços/grupos e catálogo; carregar por handle e liberar referências ao terminar. É sistema de recursos, não componente universal para anexar.','Catálogo, dependências, armazenamento, handles e ciclo de vida','P07'),
 ('com.unity.render-pipelines.universal','Renderização URP','Configurar pipeline e renderer assets; dados adicionais estendem câmera/luz. Efeitos Volume usam perfil; renderer features pertencem ao renderer.','Render graph, render targets, materiais/shaders e orçamento GPU','P12/P16'),
 ('com.unity.render-pipelines.high-definition','Renderização HDRP','Escolher o tipo de câmera/luz/volume ou recurso do pipeline e configurar seus consumidores; cada propriedade exige implementação gráfica equivalente na Astra.','Render graph, HDR, múltiplas vistas, iluminação e orçamento GPU','P12/P16'),
 ('com.unity.render-pipelines.core','Infraestrutura gráfica','Vincular volumes, perfis e recursos ao pipeline ativo; avaliar regiões, pesos e parâmetros sobrescritos.','Pipeline de renderização, volumes, câmera e gerenciamento de recursos','P12/P16'),
 ('com.unity.visualeffectgraph','VFX em grafo','Criar VisualEffectAsset, expor parâmetros e atribuir ao componente VisualEffect; fornecer eventos e recursos e configurar bounds.','Graph compiler, compute, buffers, renderer e relógio','P16'),
 ('com.unity.shadergraph','Shaders em grafo','Criar grafo compatível com o pipeline, propriedades e material; compilar variantes e conectar o recurso ao renderer. Não é componente de objeto.','Compilador de shaders, materiais, render pipeline e cache','P16')]

def family(t):
    for p,n,w,d,pack in families:
        if t['package']==p:return dict(name=n,workflow=w,dependencies=d,roadmap=pack,review='Roteiro da família; validar condições específicas na ficha e fonte do tipo')
    n=t['name']
    if '2D' in n or n in ['Grid','GridLayout','SpriteRenderer','SpriteMask','SpriteShapeRenderer','Tilemap','TilemapRenderer','TilemapCollider2D']:f='Física/objetos 2D';p='P15'
    elif any(x in n for x in ['Collider','Rigidbody','Joint','Articulation','ConstantForce','CharacterController','Cloth']):f='Física 3D';p='P10'
    elif any(x in n for x in ['Audio','Video']):f='Áudio e vídeo';p='P12'
    elif any(x in n for x in ['Animation','Animator','Constraint','Skinned','Playable']):f='Animação';p='P11'
    elif any(x in n for x in ['Camera','Skybox','Light','Probe','Occlusion','LensFlare','Projector']):f='Câmera e iluminação';p='P03/P12'
    elif n in ['Transform','Component','Behaviour','MonoBehaviour','GameObject','Object']:f='Objeto e composição';p='P01/P02'
    elif any(x in n for x in ['Canvas','RectTransform','TextMesh','UIDocument','UIRenderer']):f='Interface';p='P13'
    elif any(x in n for x in ['Particle','VisualEffect','VFX','Wind','Terrain','Tree']):f='Ambiente e efeitos';p='P16'
    elif 'NavMesh' in n or n=='OffMeshLink':f='Navegação';p='P14'
    else:f='Geometria e recursos';p='P07/P09'
    return dict(name=f,workflow='Consultar o roteiro específico desta família no plano; configurar propriedades e referências abaixo e seguir as dependências obrigatórias, funcionais e de recurso separadamente.',dependencies='Ver relações tipadas desta ficha e composição de objeto no plano.',roadmap=p,review='Metadados extraídos; intenção de uso descrita por família no plano')

warnings={(x['package'],x['path']) for x in raw['parserWarnings']}
records=[]
for k,kind in selected.items():
    t=types[k];r=dict(t);r['category']=kind;r['baseIds']=base[k];r['ancestorIds']=anc[k]
    r['abstract']='abstract' in t['modifiers'];r['obsolete']='Obsolete' in t['attributes']
    r['menuStatus']='explicit' if t.get('addMenu') else 'hidden' if 'addMenu' in t else 'not declared here; verify editor registration'
    r['requiresResolved']=[{'name':n,'id':resolve(n,t),'relation':'RequireComponent attribute'} for n in t['requires']]
    r['inheritedRequirements']=[{'declaredBy':b,'name':n,'id':resolve(n,types[b])} for b in anc[k] for n in types[b]['requires']]
    r['typedReferences']=[]
    for m in t['members']:
        for token in re.findall(r'[A-Za-z_]\w*(?:\.\w+)*',m['type']):
            target=resolve(token,t)
            if target in selected and target!=k:r['typedReferences'].append({'property':m['name'],'id':target,'relation':'declared type; not necessarily mandatory'})
    r['sourceRecoveryWarning']=any((t['package'],s['path']) in warnings for s in t['sources'])
    r['guide']=family(t)
    if t['package']=='UnityEngine' and t['name'] in core:
        r['apiDocumentation']=next((a for a in api if a['name']==t['name']),None)
        # Preserve only clean function labels from the historical catalog.
        f=core[t['name']].get('function','')
        if '\ufffd' not in f:r['purpose']=f
    r['astraStatus']='Referência; não equivale a suporte Astra. Ver matriz atual no plano.'
    records.append(r)
byid={r['id']:r for r in records}
for r in records:r['requiredBy']=[];r['referencedBy']=[];r['derivedBy']=[]
for r in records:
    for d in r['requiresResolved']+r['inheritedRequirements']:
        if d['id'] in byid:byid[d['id']]['requiredBy'].append(r['id'])
    for d in r['typedReferences']:byid[d['id']]['referencedBy'].append({'id':r['id'],'property':d['property']})
    for d in r['baseIds']:
        if d in byid:byid[d]['derivedBy'].append(r['id'])
for r in records:r['requiredBy']=sorted(set(r['requiredBy']))
records.sort(key=lambda r:(r['category'],r['package'],r['fullName']))
stats={'types':len(records),'categories':dict(Counter(r['category'] for r in records)),
       'componentPackages':dict(Counter(r['package'] for r in records if r['category']=='Component')),
       'declaredMembers':sum(len(r['members']) for r in records),
       'componentMembers':sum(len(r['members']) for r in records if r['category']=='Component'),
       'coreApiPages':len(api),'sourceRecoveryTypes':sum(r['sourceRecoveryWarning'] for r in records)}
result=dict(date='2026-09-15',reference='Unity 6000.0 + pacotes fixados; inventário de código, não auditoria de paridade executada',stats=stats,manifests=raw['manifests'],types=records,parserWarnings=raw['parserWarnings'],excludedCore=old['excludedUnitySourceCandidates'],limits=[
 'O ecossistema inclui pacotes e scripts arbitrários. A completude é relativa às fontes fixadas listadas.',
 'Extrator não executa compilador C#: diretivas condicionais, obsoletos e APIs experimentais podem coexistir no inventário.',
 'Propriedade pública não implica campo no Inspector; campo serializável candidato ainda depende de tipo suportado/custom inspector.',
 'Não foram extraídos todos os CustomEditors/PropertyDrawers nem defaults/ranges de cada propriedade. Paridade visual/semântica por campo permanece a revisar.',
 'As 117 páginas do núcleo foram consultadas. Pacotes foram consultados por fontes/manifestos; seu link API index não significa leitura de cada página de propriedade.',
 'Relações tipadas não descobrem dependências dinâmicas via string/GetComponent/reflection/shader. Grafo funcional requer curadoria.',
 'Dados de suporte incluem tipos internos e valores de execução para permitir resolver propriedades; não são comandos Add.',
 'Pacotes adicionais/DOTS/serviços e Asset Store estão fora deste snapshot. Versões fixadas não comprovam compatibilidade conjunta.'])
(OUT/'catalogo-unity.json').write_text(json.dumps(result,ensure_ascii=False,separators=(',',':')),encoding='utf-8')
(OUT/'resumo-cobertura.json').write_text(json.dumps({k:v for k,v in result.items() if k!='types'},ensure_ascii=False,indent=2),encoding='utf-8')

def esc(s):return str(s).replace('|','\\|').replace('\n',' ')
def anchor(k):return re.sub('[^a-z0-9]+','-',k.lower()).strip('-')
lines=['# Atlas de tipos e propriedades Unity — 15/09/2026','',
 '**Leia o plano de universalidade e o resumo de cobertura antes de usar as fichas como requisito de implementação.**',
 '',f"{stats['categories'].get('Component',0)} tipos Component; {stats['types']} tipos com apoio; {stats['declaredMembers']} propriedades/campos declarados; 117 páginas API do núcleo consultadas.",'',
 'Nomes são identificadores da referência. A quantidade inclui bases/obsoletos; não representa botões que podem ser anexados nem funções entregues na Astra.',
 '', 'As fichas da interface pesquisável incluem métodos, propriedades herdadas por ligação à base, enums, dependentes e fontes. Este documento lista todas as propriedades declaradas; bases devem ser consultadas também.','',
 '## Fontes fixadas','', '| Fonte | Versão |', '|---|---|']
for m in raw['manifests']:lines.append(f"| [{m['package']}]({m['source']}) | {m['version']} |")
for cat in ['Component','Volume profile','UI Toolkit element','Resource / contract','Supporting type']:
    lines+=['',f'## {cat}','']
    for r in records:
        if r['category']!=cat:continue
        lines += [f'<a id="{anchor(r["id"])}"></a>',f'### {r["fullName"]} — {r["package"]}', '',
          f"Base: {esc(', '.join(r['bases'])) or '—'}. Abstrato: {r['abstract']}. Obsoleto: {r['obsolete']}. Menu: {esc(r.get('addMenu',r['menuStatus']))}.", '',
          f"Exigências declaradas: {', '.join(x['name'] for x in r['requiresResolved']) or 'nenhuma nesta declaração; isso não elimina necessidades funcionais'}.",
          'Herdadas: '+(', '.join(x['name'] for x in r['inheritedRequirements']) or '—')+'.',
          'Exigido por: '+(', '.join(types[x]['fullName'] for x in r['requiredBy']) or 'nenhum atributo resolvido neste recorte')+'.',
          'Bases consultáveis: '+('; '.join(f'[{types[x]["fullName"]}](#{anchor(x)})' for x in r['baseIds'] if x in byid) or '—')+'.','']
        if r.get('purpose'):lines += ['Função: '+r['purpose']+'.','']
        if cat!='Supporting type':lines += [r['guide']['workflow'],'','Roadmap: '+r['guide']['roadmap']+'.','']
        lines += ['Fonte: '+'; '.join(f"[{s['path']}:{s['line']}]({s['url']}{'#L'+str(s['line']) if 'github.com' in s['url'] else ''})" for s in r['sources'][:4])+'.','']
        if r['sourceRecoveryWarning']:lines += ['**Extração com recuperação sintática/preprocessador: confrontar a fonte antes de implementar este contrato.**','']
        if r['members']:
            lines += ['| Propriedade / campo | Tipo | Acesso público | Exposição |','|---|---|---|---|']
            for m in r['members']:
                access=('R' if m['read'] else '')+('W' if m['write'] else '') if m['public'] else 'privado serializado'
                lines.append(f"| `{m['name']}` | `{esc(m['type'])}` | {access}{' estático' if m['static'] else ''} | {m['exposure']}{'; hidden' if m['hidden'] else ''} |")
        else:lines += ['Sem propriedades/campos próprios extraídos; consultar herança, dados genéricos e métodos na versão pesquisável.']
        if r['enumValues']:lines += ['', 'Valores: '+', '.join('`'+x+'`' for x in dict.fromkeys(r['enumValues']))+'.']
        a=r.get('apiDocumentation')
        if a:
            own=[s for s in a['sections'] if 'Properties' in s['section'] and not s['inherited']]
            lines += ['',f"API consultada: [{a['name']}]({a['url']})."]
            for s in own:lines += [s['section']+': '+', '.join(f"[{m['name']}]({m['url']})" for m in s['members'])+'.']
(OUT/'CATALOGO-UNITY.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(OUT/'ATLAS-UNITY.html').write_text((OUT/'atlas.template.html').read_text(encoding='utf-8').replace('__ATLAS_JSON__',json.dumps(result,ensure_ascii=False,separators=(',',':')).replace('</','<\\/')),encoding='utf-8')
print(json.dumps(stats,ensure_ascii=False,indent=2))
