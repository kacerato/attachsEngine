#!/usr/bin/env python3
"""Generate GUI concepts using the existing solid mark vocabulary, then repack with pack-icon-atlas.py."""
import importlib.util
import json
from pathlib import Path

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('mark',root/'tools/generate-astra-mark-icons.py')
mark=importlib.util.module_from_spec(spec);spec.loader.exec_module(mark)
drawings={
 'action-sequence': [('circle',((7,7),3)),('circle',((16,16),3)),('circle',((25,25),3)),
                     ('poly',mark.bar((9,9),(14,14),3)),('poly',mark.bar((18,18),(23,23),3)),
                     ('poly',[(22,12),(29,12),(29,19)])],
 'visual-states': [('poly',[(3,8),(12,8),(12,24),(3,24)]),
                   ('poly',[(14,5),(24,5),(24,27),(14,27)]),
                   ('poly',[(26,12),(30,12),(30,20),(26,20)])],
}
named=root/'assets/astra-visual/icons/named'
for name,shapes in drawings.items():mark.render(shapes).save(named/'ui'/f'{name}.png')
path=named/'catalog.json';catalog=json.loads(path.read_text(encoding='utf-8'))
for name in drawings:catalog['icons'][f'ui/{name}']={'category':'ui','name':name,'dark_ui_ready':True,'generation':'gui-behavior-v1','source':'tools/generate-gui-behavior-icons.py; existing solid mark primitives'}
path.write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Generated 2 GUI behavior icons. Run tools/pack-icon-atlas.py.')
