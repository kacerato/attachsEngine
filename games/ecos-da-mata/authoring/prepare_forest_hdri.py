from pathlib import Path
import urllib.request,json,re
root=Path('games/ecos-da-mata');h={'User-Agent':'Astra-content-production/1.0'}
def get(url):return urllib.request.urlopen(urllib.request.Request(url,headers=h)).read()
meta=json.loads(get('https://api.polyhaven.com/files/forest_slope'));item=meta['hdri']['1k']['hdr'];data=get(item['url']);p=root/'sources/forest_slope.hdr';p.write_bytes(data)
m=re.search(rb'-Y (\d+) \+X (\d+)\n',data);height,width=map(int,m.groups());cursor=m.end();rows=[]
for y in range(height):
    assert data[cursor:cursor+4]==bytes((2,2,width>>8,width&255));cursor+=4;channels=[]
    for c in range(4):
        values=bytearray()
        while len(values)<width:
            n=data[cursor];cursor+=1
            if n>128:values.extend([data[cursor]]*(n-128));cursor+=1
            else:values.extend(data[cursor:cursor+n]);cursor+=n
        channels.append(values)
    rows.append(channels)
shift=max(0,max(max(row[3]) for row in rows)-143);out=bytearray(data[:m.end()])
for row in rows:
    out.extend(bytes((2,2,width>>8,width&255)));row[3]=bytearray(max(0,v-shift) if v else 0 for v in row[3])
    for channel in row:
        for x in range(0,width,127):chunk=channel[x:x+127];out.append(len(chunk));out.extend(chunk)
(root/'project/Assets/forest_slope.hdr').write_bytes(out);(root/'sources/hdri-exposure.json').write_text(json.dumps({'source':item['url'],'license':'CC0','compensationEv':shift}));print('HDRI',width,height,'shift',shift)
