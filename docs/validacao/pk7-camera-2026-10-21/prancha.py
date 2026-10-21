# Pranchas Unity × POCO do PK-7. Uso: python prancha.py (gera pranchas/*.jpg ao lado).
from PIL import Image, ImageDraw, ImageFont
import os
D=os.path.dirname(os.path.abspath(__file__))
FONT='C:/Windows/Fonts/segoeui.ttf'
def font(n):
    try: return ImageFont.truetype(FONT,n)
    except OSError: return ImageFont.load_default()
PAIRS=[
 ('fundo','Background: cor sólida no lugar do céu',
  [('referencia-unity/dvAcLqUuwEg_t176s.jpg','Unity · Clear Flags Solid Color · youtu.be/dvAcLqUuwEg?t=176')],
  [('poco/00-original.jpg','POCO F7 · Vila Astra · Fundo Céu (padrão)'),('poco/01-fundo.jpg','POCO F7 · Fundo Cor sólida (0,02 0,25 0,30)')]),
 ('mascara','Culling Mask: camada fora da máscara some da câmera',
  [('referencia-unity/iYaOkbC58W4_t168s.jpg','Unity · Culling Mask "Mixed" · youtu.be/iYaOkbC58W4?t=168'),('referencia-unity/iYaOkbC58W4_t175s.jpg','Unity · Culling Mask Nothing · t=175')],
  [('poco/00-original.jpg','POCO F7 · Camadas visíveis: Tudo'),('poco/02-mascara.jpg','POCO F7 · Só "Padrão": casas (camada Casas) e sombras somem')]),
 ('fisica','Physical Camera: sensor e focal mudam o campo',
  [('referencia-unity/loddo4XcYng_t99s.jpg','Unity · Super 35 (24,89×18,66) · 18 mm · youtu.be/loddo4XcYng?t=99'),('referencia-unity/loddo4XcYng_t110s.jpg','Unity · sensor 70 mm · 18 mm · t=110')],
  [('poco/04-fisica85.jpg','POCO F7 · 36×24 · 85 mm · Gate Fit Vertical'),('poco/05-fisica14.jpg','POCO F7 · 36×24 · 14 mm · Gate Fit Vertical')]),
 ('dividida','Viewport Rect: duas câmeras lado a lado',
  [('referencia-unity/8iaRrewHQuw_t66s.jpg','Unity 6 URP · Viewport Rect W 0,5 / X 0,5 · youtu.be/8iaRrewHQuw?t=66')],
  [('poco/20-dividida.jpg','POCO F7 · Jogador (MainCamera) X0 L0,5 · Câmera 2 X0,5 L0,5 prioridade 1')]),
]
W=900
def tile(path,caption):
    im=Image.open(os.path.join(D,path)).convert('RGB')
    im=im.resize((W,int(im.height*W/im.width)))
    out=Image.new('RGB',(W,im.height+40),(24,26,30))
    out.paste(im,(0,40));ImageDraw.Draw(out).text((10,8),caption,fill=(230,230,230),font=font(20))
    return out
os.makedirs(os.path.join(D,'pranchas'),exist_ok=True)
for name,title,unity,poco in PAIRS:
    if not all(os.path.exists(os.path.join(D,p)) for p,_ in unity+poco): print('pula',name);continue
    left=[tile(*u) for u in unity];right=[tile(*p) for p in poco]
    hl=sum(t.height for t in left);hr=sum(t.height for t in right)
    board=Image.new('RGB',(W*2+30,max(hl,hr)+70),(14,15,18))
    ImageDraw.Draw(board).text((10,14),title+'  —  esquerda: Unity (frame de vídeo)  ·  direita: Astra no aparelho',fill=(180,240,90),font=font(26))
    y=60
    for t in left: board.paste(t,(10,y));y+=t.height
    y=60
    for t in right: board.paste(t,(W+20,y));y+=t.height
    board.save(os.path.join(D,'pranchas',f'{name}.jpg'),quality=88);print('ok',name)
