"""Fetch official API metadata and additional pinned package references only."""
from pathlib import Path
import concurrent.futures as cf
import hashlib,html,json,re,urllib.request
ROOT=Path(__file__).resolve().parents[3]
CACHE=ROOT/'build/component-reference-research'
OUT=Path(__file__).resolve().parent
def get(url,p):
 p.parent.mkdir(parents=True,exist_ok=True)
 if p.exists():return p.read_bytes()
 b=urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Astra-Reference-Catalog/1.0'}),timeout=40).read();p.write_bytes(b);return b
def docs():
 old=json.load(open(ROOT/'docs/componentes/catalogo.json',encoding='utf-8'))
 rows=[x for x in old['entries'] if x['engine']=='Unity']
 def one(r):
  u=r['sources'][0]
  try:
   s=get(u,CACHE/'api-pages'/(u.rsplit('/',1)[-1])).decode('utf-8')
   sections=[];inherited=False
   for title,body in re.findall(r'<h3[^>]*>(.*?)</h3>(.*?)(?=<h3|\Z)',s,re.S):
    title=html.unescape(re.sub('<[^>]+>','',title)).strip()
    if title=='Inherited Members':inherited=True
    if title in ['Properties','Static Properties','Public Methods','Static Methods','Messages','Events','Protected Methods']:
     vals=[dict(name=html.unescape(re.sub('<[^>]+>','',n)),url=urllib.request.urljoin(u,l)) for l,n in re.findall(r'<td class="lbl">\s*<a href="([^"]+)">(.*?)</a>',body,re.S)]
     sections.append(dict(section=title,inherited=inherited,members=vals))
   return dict(name=r['name'],url=u,sections=sections)
  except Exception as e:return dict(name=r['name'],url=u,error=str(e))
 result=list(cf.ThreadPoolExecutor(max_workers=6).map(one,rows))
 (OUT/'api-nucleo.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
 print('API pages',len(result),'failed',sum('error'in x for x in result),flush=True)
def packages():
 specs={'com.unity.timeline':'1.8','com.unity.localization':'1.5','com.unity.xr.core-utils':'2.4','com.unity.xr.arfoundation':'6.0','com.unity.2d.animation':'10.0','com.unity.2d.spriteshape':'10.0','com.unity.2d.tilemap.extras':'4.0','com.unity.addressables':'2.3'}
 for pkg,prefix in specs.items():
  try:
   d=json.loads(get('https://packages.unity.com/'+pkg,CACHE/'packages'/(pkg+'.json')))
   candidates=[v for v in d['versions'] if v.startswith(prefix+'.') and re.fullmatch(r'\d+\.\d+\.\d+',v)]
   ver=max(candidates,key=lambda v:tuple(map(int,v.split('.'))));m=d['versions'][ver]
   u=m['dist']['tarball'];b=get(u,CACHE/'packages'/f'{pkg}-{ver}.tgz')
   expected=m['dist'].get('shasum');actual=hashlib.sha1(b).hexdigest()
   if expected and expected!=actual:raise RuntimeError('archive hash mismatch')
   print('package',pkg,ver,'bytes',len(b),flush=True)
  except Exception as e:print('PACKAGE FAILURE',pkg,str(e),flush=True)
if __name__=='__main__':
 with cf.ThreadPoolExecutor(max_workers=2) as ex:
  for result in ex.map(lambda f:f(),[docs,packages]):pass
