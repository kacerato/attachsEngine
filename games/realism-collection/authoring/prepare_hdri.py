"""Lossless power-of-two radiance scaling for Astra's RGBA16F import range.

Preserves the original download. The scene applies the inverse exposure gain.
Only RGBE exponents change; RGB mantissas and spatial detail remain intact.
"""
from pathlib import Path
import json,re
ROOT=Path(__file__).resolve().parents[1]
path=ROOT/'sources/quarry_03/quarry_03.hdr'
data=path.read_bytes()
match=re.search(rb'-Y (\d+) \+X (\d+)\n',data)
height,width=map(int,match.groups());cursor=match.end();rows=[]
for y in range(height):
    assert data[cursor:cursor+4]==bytes((2,2,width>>8,width&255));cursor+=4
    channels=[]
    for channel in range(4):
        values=bytearray()
        while len(values)<width:
            count=data[cursor];cursor+=1
            if count>128:
                values.extend([data[cursor]]*(count-128));cursor+=1
            else:
                values.extend(data[cursor:cursor+count]);cursor+=count
        assert len(values)==width
        channels.append(values)
    rows.append(channels)
shift=max(0,max(max(row[3]) for row in rows)-143)
output=bytearray(data[:match.end()])
for row in rows:
    output.extend(bytes((2,2,width>>8,width&255)))
    row[3]=bytearray(max(0,value-shift) if value else 0 for value in row[3])
    for channel in row:
        for x in range(0,width,127):
            chunk=channel[x:x+127];output.append(len(chunk));output.extend(chunk)
(path.parent/'quarry_03_rgba16f.hdr').write_bytes(output)
(path.parent/'exposure.json').write_text(json.dumps({'preExposureEv':-shift,'sceneCompensationEv':shift,'source':path.name}))
print('HDRI prepared:',width,height,'compensation EV',shift)
