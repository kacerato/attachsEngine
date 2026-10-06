"""Create a traceable inventory; never infer completion from file existence."""
import argparse
import hashlib
import json
import re
from pathlib import Path

root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--source',type=Path,default=root/'docs/planos/ROADMAP-UI-UNIVERSAL-attachsEngine-2026-10-04.md')
parser.add_argument('--output',type=Path,default=root/'docs/planos/ui-universal/PROGRESSO.json')
args=parser.parse_args()
data=args.source.read_bytes();lines=data.decode('utf-8-sig').splitlines()
previous={}
if args.output.exists():
    old=json.loads(args.output.read_text(encoding='utf-8'))
    if old['source']['sha256']!=hashlib.sha256(data).hexdigest():
        raise SystemExit('Source changed: reconcile requirements explicitly before replacing the inventory')
    previous={item['id']:item for item in old['requirements']}
items=[];sections=[]
for number,line in enumerate(lines,1):
    if line.startswith('#'):sections.append({'line':number,'heading':line})
    m=re.match(r'- (R\d+\.\d+) (.+)',line)
    table=re.match(r'\| ((?:SP|T)\d+) \| (.+?) \| (.+?) \|',line)
    if not m and not table:continue
    key=m[1] if m else table[1]
    item={'id':key,'source_line':number,'requirement':m[2] if m else table[2],
          'acceptance':None if m else table[3],'status':'planned','implementation':[],
          'evidence':[],'remaining':[]}
    if key in previous:item.update({k:previous[key][k] for k in ['status','implementation','evidence','remaining']})
    if item['status'] not in {'planned','partial','complete','unsupported'}:
        raise SystemExit(f'{key}: unknown execution status')
    if item['status']=='partial' and not item['remaining']:
        raise SystemExit(f'{key}: partial delivery must state what remains')
    if item['status']=='complete' and (not item['implementation'] or not item['evidence'] or item['remaining']):
        raise SystemExit(f'{key}: completion requires implementation, evidence, and no remaining work')
    items.append(item)
assert len(items)==len(set(i['id'] for i in items)), 'Duplicate IDs'
summary={status:sum(i['status']==status for i in items) for status in ['planned','partial','complete','unsupported']}
out={'format':1,'source':{'path':args.source.relative_to(root).as_posix(),
     'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'lines':len(lines)},
     'scope':'Full source is preserved verbatim, including unnumbered requirements; IDs are execution anchors, not a replacement for the specification.',
     'engine_complete':False,'summary':summary,'sections':sections,'requirements':items}
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'requirements':len(items),'summary':summary,'source_sha256':out['source']['sha256']}))
