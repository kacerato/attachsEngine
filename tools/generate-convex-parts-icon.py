"""Separated solid pieces, using the editor's existing vector mark primitives."""
import importlib.util
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('mark',root/'tools/generate-astra-mark-icons.py')
mark=importlib.util.module_from_spec(spec);spec.loader.exec_module(mark)
shapes=[('poly',[(4,7),(12,3),(12,16),(4,20)]),
        ('poly',[(16,3),(28,8),(28,20),(16,16)]),
        ('poly',[(5,24),(14,20),(26,24),(16,30)])]
named=root/'assets/astra-visual/icons/named'
mark.render(shapes).save(named/'component/convex-parts.png')
path=named/'catalog.json';data=json.loads(path.read_text(encoding='utf-8'))
data['icons']['component/convex-parts']=dict(category='component',name='convex-parts',dark_ui_ready=True,generation='convex-parts-v1',source='tools/generate-convex-parts-icon.py; existing solid mark primitives')
path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
