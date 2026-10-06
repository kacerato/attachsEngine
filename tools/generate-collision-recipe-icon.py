"""Persistent physical revision mark, built with the existing native atlas primitives."""
import importlib.util
import json
from pathlib import Path
root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('mark', root/'tools/generate-astra-mark-icons.py')
mark = importlib.util.module_from_spec(spec); spec.loader.exec_module(mark)
edge = lambda a,b: ('poly',mark.bar(a,b,2))
shapes = [edge((8,10),(16,6)),edge((16,6),(24,10)),edge((24,10),(16,15)),edge((16,15),(8,10)),edge((8,10),(8,19)),edge((8,19),(16,24)),edge((16,24),(24,19)),edge((24,19),(24,10)),edge((16,15),(16,24)),edge((3,22),(3,9)),edge((3,9),(6,6)),edge((29,10),(29,23)),edge((29,23),(26,26)),('poly',[(1,9),(6,9),(3,5)]),('poly',[(26,23),(31,23),(29,27)])]
named=root/'assets/astra-visual/icons/named'
mark.render(shapes).save(named/'physics/collision-recipe.png')
path=named/'catalog.json'; data=json.loads(path.read_text(encoding='utf-8'))
data['icons']['physics/collision-recipe']=dict(category='physics',name='collision-recipe',dark_ui_ready=True,generation='collision-recipe-v1',source='tools/generate-collision-recipe-icon.py; existing solid mark primitives')
path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
