"""World-space instruction signs, exported as ordinary textured GLB geometry."""
from PIL import Image, ImageDraw, ImageFont
from io import BytesIO
import json,struct

def make_signs(definitions):
    data=bytearray(); views=[]; accessors=[]; images=[]; textures=[]; materials=[]; meshes=[]; nodes=[]
    def add(payload,target=None):
        data.extend(b'\0'*(-len(data)%4)); view={'buffer':0,'byteOffset':len(data),'byteLength':len(payload)}
        if target: view['target']=target
        views.append(view);data.extend(payload);return len(views)-1
    font=ImageFont.truetype('C:/Windows/Fonts/arialbd.ttf',48)
    small=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',32)
    for i,(name,title,lines) in enumerate(definitions):
        image=Image.new('RGB',(1024,512),(31,35,31)); d=ImageDraw.Draw(image)
        d.rectangle((0,0,1024,94),fill=(205,172,96)); d.text((32,18),title,font=font,fill=(25,27,22))
        for j,line in enumerate(lines): d.text((32,130+j*65),line,font=small,fill=(225,224,208))
        d.text((32,445),'ASTRA  /  EXPEDIÇÕES  /  2026',font=small,fill=(144,151,135))
        encoded=BytesIO();image.save(encoded,format='PNG')
        images.append({'bufferView':add(encoded.getvalue()),'mimeType':'image/png'})
        textures.append({'source':i})
        materials.append({'name':name,'pbrMetallicRoughness':{'baseColorTexture':{'index':i},'metallicFactor':0,'roughnessFactor':.9},'doubleSided':True})
        vertices=[(-1,-.5,0),(1,-.5,0),(1,.5,0),(-1,.5,0)]
        attrs={}
        for semantic,values,size in [('POSITION',vertices,3),('NORMAL',[(0,0,-1)]*4,3),('TEXCOORD_0',[(1,1),(0,1),(0,0),(1,0)],2)]:
            accessor={'bufferView':add(b''.join(struct.pack('<'+'f'*size,*v) for v in values),34962),'componentType':5126,'count':4,'type':'VEC'+str(size)}
            if semantic=='POSITION': accessor.update(min=[-1,-.5,0],max=[1,.5,0])
            accessors.append(accessor);attrs[semantic]=len(accessors)-1
        # Facing the approaching player (+Z forward), with text UV horizontally corrected.
        accessors.append({'bufferView':add(struct.pack('<6H',0,2,1,0,3,2),34963),'componentType':5123,'count':6,'type':'SCALAR'})
        meshes.append({'name':name,'primitives':[{'attributes':attrs,'indices':len(accessors)-1,'material':i}]})
        nodes.append({'name':name,'mesh':i})
    data.extend(b'\0'*(-len(data)%4))
    doc={'asset':{'version':'2.0'},'buffers':[{'byteLength':len(data)}],'bufferViews':views,'accessors':accessors,
         'images':images,'textures':textures,'materials':materials,'meshes':meshes,'nodes':nodes,'scenes':[{'nodes':list(range(len(nodes)))}],'scene':0}
    encoded=json.dumps(doc,separators=(',',':')).encode();encoded+=b' '*(-len(encoded)%4)
    return struct.pack('<4sII',b'glTF',2,28+len(encoded)+len(data))+struct.pack('<I4s',len(encoded),b'JSON')+encoded+struct.pack('<I4s',len(data),b'BIN\0')+data
