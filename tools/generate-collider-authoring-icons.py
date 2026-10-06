"""Native collision element/solver marks, integrated into the shipped icon atlas."""
import importlib.util
import json
from pathlib import Path
root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('mark', root/'tools/generate-astra-mark-icons.py')
mark = importlib.util.module_from_spec(spec); spec.loader.exec_module(mark)
edge = lambda a,b: ('poly',mark.bar(a,b,2))
icons = {
 'editor/collider-vertex': [edge((7,25),(16,6)),edge((16,6),(26,25)),edge((26,25),(7,25))]+mark.ring(16,6,5,0)+mark.ring(7,25,3,0)+mark.ring(26,25,3,0),
 'editor/collider-face': [('poly',[(5,11),(16,5),(27,11),(27,24),(16,29),(5,24)])]+[('poly',mark.bar((7,12),(16,17),2)),('poly',mark.bar((16,17),(25,12),2)),('poly',mark.bar((16,17),(16,27),2))],
 'physics/diagnostic': mark.ring(16,16,10,7)+[edge((2,16),(30,16)),edge((16,2),(16,30))]+mark.ring(16,16,3,0),
}
named=root/'assets/astra-visual/icons/named'
path=named/'catalog.json';data=json.loads(path.read_text(encoding='utf-8'))
for key,shapes in icons.items():
    target=named/(key+'.png');target.parent.mkdir(parents=True,exist_ok=True);mark.render(shapes).save(target)
    category,name=key.split('/')
    data['icons'][key]=dict(category=category,name=name,dark_ui_ready=True,generation='collider-authoring-v1',source='tools/generate-collider-authoring-icons.py; existing solid mark primitives')
path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
