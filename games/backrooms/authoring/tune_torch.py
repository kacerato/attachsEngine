"""Change only authored torch payloads; avoid reimporting unchanged GLB resources."""
from pathlib import Path
import re,json
ROOT=Path(__file__).resolve().parents[1]
for project in (ROOT/'projects').iterdir():
    scene=project/'scenes/editor.aescene'
    lines=scene.read_text(encoding='utf-8').splitlines()
    for i,line in enumerate(lines):
        if '"Lanterna móvel"' not in line:continue
        match=re.search(r'("astra.render.light" 3 )("(?:[^"\\]|\\.)*")',line)
        values=json.loads(match[2]).split()
        values[10]='1200' if project.name=='reservatorio-04' else '400'
        values[11]='40'
        lines[i]=line[:match.start(2)]+json.dumps(' '.join(values))+line[match.end(2):]
    scene.write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print('Torch updated',project.name,flush=True)
