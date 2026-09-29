"""Transfer only the three authored projects to the existing APK (no install/build)."""
from pathlib import Path
import subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
ADB=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
TRANSPORT='1'
def adb(*args): subprocess.run([str(ADB),'-t',TRANSPORT,*map(str,args)],check=True)
if __name__=='__main__':
    games=sys.argv[1:] or ['usina-17','carga-bruta','sepulcro-calcario']
    for slug in games:
        if slug not in ['usina-17','carga-bruta','sepulcro-calcario']: raise ValueError(slug)
        project=ROOT/'projects'/slug
        remote='/sdcard/Android/data/dev.aether.editor/files/Projetos/'+slug
        adb('shell','mkdir','-p',remote)
        adb('push','--sync',project/'Assets',project/'Scripts',project/'scenes',project/'project.json',project/'.astra',remote+'/')
        print('Transferred',slug,flush=True)
