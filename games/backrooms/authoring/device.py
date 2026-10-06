"""Deploy only game content and capture the existing Android renderer."""
from pathlib import Path
import argparse, json, shlex, subprocess, time

ROOT=Path(__file__).resolve().parents[1]
ADB=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
REMOTE='/sdcard/Android/data/dev.aether.editor/files/Projetos/'

def adb(*args, binary=False):
    result=subprocess.run([str(ADB),*map(str,args)],check=True,capture_output=True)
    return result.stdout if binary else result.stdout.decode('utf-8',errors='replace')

def deploy(slug):
    project=ROOT/'projects'/slug
    if not (project/'project.json').is_file():raise ValueError('Missing project '+slug)
    adb('shell','mkdir','-p',REMOTE+slug)
    for sub in ['Assets','Audio','Scripts','scenes','.astra','project.json']:
        adb('push','--sync',project/sub,REMOTE+slug+'/')
    print('Deployed',slug,flush=True)

def launch(slug,probe=False):
    project=ROOT/'projects'/slug
    data=json.loads((project/'project.json').read_text(encoding='utf-8'))
    if probe:
        # Dedicated validation copy on device; never replaces the user's authored main scene.
        probe_slug=slug+'-lighting-lab'
        data['project']['name']=slug+' Lighting Lab'
        data['project']['path']=REMOTE+probe_slug
        scene=(project/'scenes/editor.aescene').read_text(encoding='utf-8')
        behavior={'nivel-0':'CircuitoAmarelo','reservatorio-04':'Reservatorio04','ultimo-turno':'UltimoTurno'}[slug]
        scene=scene.replace('project.'+behavior,'project.LightProbe').replace('Scripts/'+behavior+'.cs','Scripts/LightProbe.cs')
        lab=ROOT/'evidence/lab'
        lab.mkdir(parents=True,exist_ok=True)
        (lab/'project.json').write_text(json.dumps(data,ensure_ascii=False),encoding='utf-8')
        (lab/'editor.aescene').write_text(scene,encoding='utf-8')
        adb('shell','mkdir','-p',REMOTE+probe_slug+'/scenes')
        for sub in ['Assets','Audio','Scripts','.astra']:adb('push','--sync',project/sub,REMOTE+probe_slug+'/')
        adb('push',lab/'project.json',REMOTE+probe_slug+'/project.json')
        adb('push',lab/'editor.aescene',REMOTE+probe_slug+'/scenes/editor.aescene')
    adb('shell','am','force-stop','dev.aether.editor')
    adb('logcat','-c')
    command='am start -n dev.aether.editor/.shell.AstraShellActivity --es astra.open_project '+shlex.quote(data['project']['name'])+' --ez aether.start_play true --ez aether.profile_frames true'
    print(adb('shell',command),flush=True)

def capture(name):
    folder=ROOT/'evidence'
    folder.mkdir(parents=True,exist_ok=True)
    (folder/(name+'.png')).write_bytes(adb('exec-out','screencap','-p',binary=True))
    (folder/(name+'.log')).write_text(adb('logcat','-d','-s','Astra.Script:I','Aether:I','Astra.Managed:I','AndroidRuntime:E','*:S'),encoding='utf-8')
    print('Captured',name,flush=True)

def probe_run(slug):
    launch(slug,True)
    stages=['spots','dark','flashlight','restored','directional']
    deadline=time.monotonic()+90
    for stage,name in enumerate(stages):
        while True:
            log=adb('logcat','-d','-s','Astra.Script:I','*:S')
            if 'LIGHTPROBE STAGE='+str(stage) in log:break
            if time.monotonic()>deadline:raise RuntimeError('Probe did not reach stage '+str(stage))
            time.sleep(.5)
        time.sleep(2)
        capture(slug+'-'+name)
    from PIL import Image
    import numpy as np
    metrics={}
    for name in stages:
        im=np.asarray(Image.open(ROOT/'evidence'/f'{slug}-{name}.png').convert('RGB'),dtype=float)
        h,w=im.shape[:2]
        # Actual interior viewport, excluding toolbars, HUD and corner input buttons.
        roi=im[int(h*.17):int(h*.78),int(w*.08):int(w*.85)]
        luminance=roi@np.array([.2126,.7152,.0722])
        metrics[name]={'meanSrgbLuma':float(luminance.mean()),'p95SrgbLuma':float(np.percentile(luminance,95)),
                       'pixelsAtOrBelow3of255':float((luminance<=3).mean())}
    (ROOT/'evidence'/f'{slug}-light-metrics.json').write_text(json.dumps(metrics,indent=2),encoding='utf-8')
    print(json.dumps(metrics,indent=2),flush=True)

def verify_dark(slug):
    launch(slug)
    deadline=time.monotonic()+60
    while 'BACKROOMS READY' not in adb('logcat','-d','-s','Astra.Script:I','*:S'):
        if time.monotonic()>deadline:raise RuntimeError('Game did not enter Play')
        time.sleep(.5)
    time.sleep(4)
    capture(slug+'-forward-lit-final')
    # Actual native touch input: short release toggles the same Hold action used by gameplay.
    size=adb('shell','wm','size').strip().split()[-1]
    dims=list(map(int,size.split('x')))
    width,height=max(dims),min(dims)
    x,y=round(width*.956),round(height*.936)
    for name in ['forward-dark-final','forward-restored-final']:
        adb('shell','input','swipe',x,y,x,y,150)
        time.sleep(2)
        capture(slug+'-'+name)
    from PIL import Image
    import numpy as np
    metrics={}
    for name in ['forward-lit-final','forward-dark-final','forward-restored-final']:
        im=np.asarray(Image.open(ROOT/'evidence'/f'{slug}-{name}.png').convert('RGB'),dtype=float)
        h,w=im.shape[:2]
        roi=im[int(h*.17):int(h*.78),int(w*.08):int(w*.85)]
        luminance=roi@np.array([.2126,.7152,.0722])
        metrics[name]={'meanSrgbLuma':float(luminance.mean()),'p95SrgbLuma':float(np.percentile(luminance,95)),
                       'pixelsAtOrBelow3of255':float((luminance<=3).mean())}
    log=adb('logcat','-d','-s','Astra.Script:I','*:S')
    if 'FLASHLIGHT enabled=False' not in log or 'FLASHLIGHT enabled=True' not in log:
        raise RuntimeError('Native touch did not toggle flashlight twice')
    metrics['darkCriterionPassed']=metrics['forward-dark-final']['pixelsAtOrBelow3of255']>.99
    (ROOT/'evidence'/f'{slug}-final-darkness.json').write_text(json.dumps(metrics,indent=2),encoding='utf-8')
    print(json.dumps(metrics,indent=2),flush=True)
    if not metrics['darkCriterionPassed']:raise RuntimeError('Unpowered viewport still visible')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('command',choices=['deploy','launch','probe','capture','probe-run','verify-dark'])
    parser.add_argument('slug')
    args=parser.parse_args()
    if args.command=='deploy':deploy(args.slug)
    elif args.command=='capture':capture(args.slug)
    elif args.command=='probe-run':probe_run(args.slug)
    elif args.command=='verify-dark':verify_dark(args.slug)
    else:launch(args.slug,args.command=='probe')
