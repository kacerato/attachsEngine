import subprocess,json,hashlib,sys
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
video=Path(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-show_frames','-show_entries','frame=best_effort_timestamp_time','-of','json',str(video)]))['frames']
subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-i',str(video),'-fps_mode','passthrough','-q:v','2',str(out/'frame-%06d.jpg')],check=True)
frames=sorted(out.glob('frame-*.jpg'));assert len(frames)==len(probe),(len(frames),len(probe))
font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',20)
entries=[];pages=[]
for start in range(0,len(frames),8):
    sheet=Image.new('RGB',(1386,4*344),(15,19,24));draw=ImageDraw.Draw(sheet)
    for i,f in enumerate(frames[start:start+8]):
        n=start+i;ts=probe[n]['best_effort_timestamp_time'];x=(i%2)*693;y=(i//2)*344
        with Image.open(f) as im:sheet.paste(im.resize((693,320)),(x,y+24))
        draw.text((x+8,y+1),f'{n+1:04d}  t={float(ts):.3f}s',font=font,fill='white')
        entries.append({'frame':n+1,'time':ts,'file':f.name,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
    page=out/f'contact-{start//8+1:03d}.jpg';sheet.save(page,quality=92);pages.append({'file':page.name,'from':start+1,'to':min(start+8,len(frames))})
manifest={'video':str(video.resolve()),'videoSha256':hashlib.sha256(video.read_bytes()).hexdigest(),'decodedFrames':len(frames),'sampling':'none: every encoded video frame decoded','pages':pages,'frames':entries,'review':'pending visual review of every contact page'}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print(json.dumps({'frames':len(frames),'pages':len(pages),'manifest':str((out/'manifest.json').resolve())}))
