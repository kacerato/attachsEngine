"""Publica o atlas offline a partir das pesquisas locais; não cadastra APIs Astra."""
from pathlib import Path
import json

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[2]
unity = json.loads((ROOT/'docs/componentes/pesquisa-2026-09-15/catalogo-unity.json').read_text(encoding='utf-8'))
godot = json.loads((OUT/'godot-api.json').read_text(encoding='utf-8'))
records=[]
for item in unity['types']:
    api=item.get('apiDocumentation') or {}
    records.append({
        'id':'U:'+item['id'],'name':item['name'],'fullName':item['fullName'],
        'engine':'Unity','version':item['version'],'package':item['package'],
        'category':item['category'],'baseIds':['U:'+x for x in item.get('baseIds',[])],
        'properties':item.get('members',[]), 'methods':item.get('methods',[]),
        'signals':[], 'constants':item.get('enumValues',[]), 'themeProperties':[],
        'requirements':item.get('requiresResolved',[]),
        'inheritedRequirements':item.get('inheritedRequirements',[]),
        'references':item.get('typedReferences',[]),
        'documentation':api.get('url'), 'sources':item.get('sources',[]),
        'flags':{'abstract':item.get('abstract'),'obsolete':item.get('obsolete'),'menuStatus':item.get('menuStatus')},
        'warning':item.get('sourceRecoveryWarning'),
    })
for item in godot['types']:
    records.append({
        'id':'G:'+item['name'],'name':item['name'],'fullName':item['name'],
        'engine':'Godot','version':'4.5-stable','package':item['path'].split('/')[1] if item['path'].startswith('modules/') else 'core/scene/editor',
        'category':item['category'],'baseIds':['G:'+item['base']] if item['base'] else [],
        'properties':item['properties'],'methods':item['methods'],'signals':item['signals'],
        'constants':item['constants'],'themeProperties':item['themeProperties'],
        'requirements':[],'inheritedRequirements':[],
        'references':[{'id':'G:'+x,'relation':'Tipo de propriedade; não comprova obrigação'} for x in item['propertyTypeReferences']],
        'documentation':item['documentation'],'sources':[{'url':item['source'],'path':item['path']}],
        'flags':item['flags'],'warning':None,
    })
records.sort(key=lambda r:(r['name'].casefold(),r['engine'],r['package']))
payload={'types':records,'godot':godot['manifest'],'unity':{'date':unity['date'],'stats':unity['stats'],'warnings':len(unity['parserWarnings'])}}
data=json.dumps(payload,ensure_ascii=False,separators=(',',':')).replace('<','\\u003c').replace('>','\\u003e').replace('&','\\u0026')
template=(OUT/'atlas.template.html').read_text(encoding='utf-8')
(OUT/'ATLAS.html').write_text(template.replace('__ATLAS_DATA__',data),encoding='utf-8')
print(f'{len(records)} tipos publicados; {len(data.encode())} bytes de metadados; sem dependências de rede para consulta')
