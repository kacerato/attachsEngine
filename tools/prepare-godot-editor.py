"""Prepara o editor Godot fixado, preservando a engine e os projetos Aether.
O núcleo oficial é verificado por hash; a camada Android é compilada do fonte.
As adaptações são reaplicáveis e não dependem de um checkout modificado à mão.
"""
from pathlib import Path
import hashlib,json,subprocess,urllib.request,zipfile
ROOT=Path(__file__).resolve().parents[1]
PIN=json.loads((ROOT/'integrations/godot/upstream.json').read_text(encoding='utf-8-sig'))
if PIN['application_id'] != 'dev.aether.godotlab':
    raise RuntimeError('O laboratório Godot não pode substituir o aplicativo Aether')
SOURCE=ROOT/'external/godot'
CACHE=ROOT/'build/godot-bootstrap'
CACHE.mkdir(parents=True,exist_ok=True)
if not (SOURCE/'.git').is_dir():
    subprocess.run(['git','clone','--depth','1','--branch',PIN['version'],PIN['source'],str(SOURCE)],check=True)
commit=subprocess.check_output(['git','-C',str(SOURCE),'rev-parse','HEAD'],text=True).strip()
if commit!=PIN['commit']: raise RuntimeError('Checkout Godot diferente da versão fixada')
apk=CACHE/'godot-editor.apk'
if not apk.is_file():
    temporary=apk.with_suffix('.download')
    urllib.request.urlretrieve(PIN['android_editor_url'],temporary)
    temporary.replace(apk)
with apk.open('rb') as stream: digest=hashlib.file_digest(stream,'sha512').hexdigest()
if digest!=PIN['android_editor_sha512']: raise RuntimeError('SHA512 do editor oficial divergente')
java=SOURCE/'platform/android/java'
def upstream(relative):
    return subprocess.check_output(['git','-C',str(SOURCE),'show',f'HEAD:{relative}']).decode('utf-8')
relative='platform/android/java/editor/build.gradle'
s=upstream(relative)
s=s.replace('applicationId "org.godotengine.editor.v4"',f'applicationId "{PIN["application_id"]}"')
s=s.replace('versionCode generateVersionCode()',f'versionCode {PIN["version_code"]}')
s=s.replace('versionName generateVersionName()',f'versionName "Astra-Godot-{PIN["version"]}"')
s=s.replace('editorAppName: "Godot Engine 4"','editorAppName: "Astra - Laboratorio Godot"')
s=s.replace('applicationIdSuffix ".debug"','applicationIdSuffix ""').replace('applicationIdSuffix ".release"','applicationIdSuffix ""')
s=s.replace('editorBuildSuffix: " (debug)"','editorBuildSuffix: ""').replace('editorBuildSuffix: " (release)"','editorBuildSuffix: ""')
s=s.replace("ndk { debugSymbolLevel 'NONE' }","ndk { debugSymbolLevel 'NONE'; abiFilters 'arm64-v8a' }")
(java/'editor/build.gradle').write_text(s,encoding='utf-8')
with zipfile.ZipFile(apk) as archive:
    for configuration in ['release','debug']:
        output=java/f'lib/libs/tools/{configuration}/arm64-v8a'
        output.mkdir(parents=True,exist_ok=True)
        for name in ['libgodot_android.so','libc++_shared.so']:
            target=output/name
            data=archive.read('lib/arm64-v8a/'+name)
            if not target.is_file() or hashlib.sha256(target.read_bytes()).digest()!=hashlib.sha256(data).digest(): target.write_bytes(data)
# Caminho local, não versionado. Não altera configurações globais do SDK.
import os
sdk=Path(os.environ.get('ANDROID_HOME',str(Path.home()/'AppData/Local/Android/Sdk')))
(java/'local.properties').write_text('sdk.dir='+sdk.as_posix()+'\n',encoding='utf-8')
print('Editor preparado:',PIN['version'],commit)
print('Gradle:',java)

# Identidade Astra no lançador; controles, tema e disposição permanecem upstream.
import shutil
resource_source=ROOT/'android/app/src/main/res'
resource_target=java/'editor/src/main/res'
for image in resource_source.glob('mipmap-*/*.png'):
    if image.name.startswith('ic_astra'):
        target=resource_target/image.relative_to(resource_source)
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(image,target)
relative='platform/android/java/editor/src/main/AndroidManifest.xml'
manifest=upstream(relative).replace('@mipmap/themed_icon','@mipmap/ic_astra')
(java/'editor/src/main/AndroidManifest.xml').write_text(manifest,encoding='utf-8')
relative='platform/android/java/editor/src/android/java/org/godotengine/editor/GodotEditor.kt'
kotlin=upstream(relative)
marker='open class GodotEditor : BaseGodotEditor() {'
assert marker in kotlin
kotlin=kotlin.replace(marker,marker+"""
    // Padrão da distribuição Astra; argumentos explícitos continuam válidos.
    override fun getCommandLine(): MutableList<String> {
        val args = super.getCommandLine()
        if (!args.contains("--language") && !args.contains("-l")) {
            args.addAll(listOf("--language", "pt"))
        }
        return args
    }
""")
(SOURCE/relative).write_text(kotlin,encoding='utf-8')
