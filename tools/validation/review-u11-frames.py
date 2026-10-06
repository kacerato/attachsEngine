"""Extract every decoded frame; produce indexed contact sheets and per-frame evidence.
Sheets deliberately retain repeated frames: no temporal subsampling is used.
"""
import argparse, csv, hashlib, json, math, subprocess
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw

p=argparse.ArgumentParser()
p.add_argument("video",type=Path);p.add_argument("output",type=Path)
p.add_argument("--ffmpeg",required=True);p.add_argument("--ffprobe",required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
frames=a.output/"frames";frames.mkdir(exist_ok=True)
probe=json.loads(subprocess.check_output([a.ffprobe,"-v","error","-select_streams","v:0",
    "-show_frames","-show_entries","frame=best_effort_timestamp_time","-of","json",str(a.video)]))
pts=[float(f["best_effort_timestamp_time"]) for f in probe["frames"]]
subprocess.run([a.ffmpeg,"-v","error","-y","-i",str(a.video),"-fps_mode","passthrough",str(frames/"%05d.png")],check=True)
paths=sorted(frames.glob("*.png"))
assert len(paths)==len(pts)>0,(len(paths),len(pts))
first=Image.open(paths[0]).convert("RGB")
rois={"hierarchy":(0,250,660,725),"coordinates":(2040,680,2760,780),"viewport":(800,490,1600,820)}
rows=[];sheets=[]
for start in range(0,len(paths),30):
    sheet=Image.new("RGB",(2000,1260),(16,18,22));draw=ImageDraw.Draw(sheet)
    for j,path in enumerate(paths[start:start+30]):
        i=start+j;im=Image.open(path).convert("RGB")
        row={"frame":i,"pts_seconds":pts[i],"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
        for name,rect in rois.items():
            diff=ImageChops.difference(im.crop(rect),first.crop(rect))
            red,green,blue=diff.split()
            strongest=ImageChops.lighter(ImageChops.lighter(red,green),blue)
            row[name+"_changed_pixels"]=sum(strongest.histogram()[13:])
        rows.append(row)
        x=(j%5)*400;y=(j//5)*210
        sheet.paste(im.crop(rois["viewport"]).resize((390,130)),(x,y+20))
        sheet.paste(im.crop(rois["coordinates"]).resize((390,50)),(x,y+150))
        draw.text((x+5,y+3),f"frame {i:03d} | {pts[i]:.3f}s",fill="white")
    out=a.output/f"sheet-{len(sheets):02d}.png";sheet.save(out);sheets.append(str(out))
with (a.output/"frames.csv").open("w",newline="",encoding="utf-8") as f:
    w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
(a.output/"extraction.json").write_text(json.dumps({"video_sha256":hashlib.sha256(a.video.read_bytes()).hexdigest(),
    "decoded_frames":len(paths),"extracted_frames":len(paths),"skipped_frames":0,"duration_last_pts":pts[-1],
    "sheets":sheets,"review_status":"extracted; visual review still required"},indent=2),encoding="utf-8")
print(json.dumps({"frames":len(paths),"sheets":len(sheets),"last_pts":pts[-1]}))
