"""Body + directed force: named icon consumed by the actual schema/creation atlas."""
import importlib.util
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('mark',root/'tools/generate-astra-mark-icons.py')
mark=importlib.util.module_from_spec(spec);spec.loader.exec_module(mark)
shapes=mark.ring(12,16,9,6)+[('poly',mark.bar((12,16),(25,16),3)),('poly',[(22,10),(30,16),(22,22)]),('poly',mark.bar((5,27),(20,27),2))]
named=root/'assets/astra-visual/icons/named'
mark.render(shapes).save(named/'component/dynamic-body-motor.png')
path=named/'catalog.json';data=json.loads(path.read_text(encoding='utf-8'))
data['icons']['component/dynamic-body-motor']=dict(category='component',name='dynamic-body-motor',dark_ui_ready=True,generation='dynamic-motor-v1',source='tools/generate-dynamic-motor-icon.py; existing solid mark primitives')
path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
