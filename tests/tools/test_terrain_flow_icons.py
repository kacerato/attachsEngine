import hashlib,json,re,subprocess,sys,tempfile,unittest
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
class FlowIcons(unittest.TestCase):
 def test_staged_vectors_full_masters_and_noop(self):
  active=[ROOT/'native/ui/ui_icon_id.h',ROOT/'assets/astra-visual/icons/named/catalog.json']
  before={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in active}
  with tempfile.TemporaryDirectory() as temp:
   output=Path(temp)/'icons'
   command=[sys.executable,str(ROOT/'tools/generate-terrain-flow-icons.py'),'--output',str(output)]
   subprocess.run(command,check=True,capture_output=True)
   manifest=json.loads((output/'manifest.json').read_text(encoding='utf-8'))
   self.assertEqual(len(manifest['icons']),6)
   header=active[0].read_text(encoding='utf-8')
   for offset,item in enumerate(manifest['icons'],1):
    existing=re.search(r'\b'+item['enum']+r'\s*=\s*(\d+)',header)
    self.assertEqual(item['predicted_id'],int(existing.group(1)) if existing else manifest['published_count']+offset)
   self.assertEqual({i['concept'] for i in manifest['icons']},{'TerrainWetness','TerrainPuddle','TerrainStream','TerrainLava','TerrainRain','TerrainSpecularHeight'})
   for item in manifest['icons']:
    png=output/f"named/{item['key']}.png";svg=output/f"vector/{item['key']}.svg"
    with Image.open(png) as image:
     self.assertEqual(image.size,(384,384));self.assertEqual(image.mode,'RGBA');self.assertIsNotNone(image.getbbox());self.assertEqual(image.getpixel((0,0))[3],0)
    self.assertIn('width="384" height="384"',svg.read_text(encoding='utf-8'))
    self.assertEqual(hashlib.sha256(svg.read_bytes()).hexdigest(),item['svg_sha256'])
   snapshots={p:(p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in output.rglob('*') if p.is_file()}
   subprocess.run(command,check=True,capture_output=True)
   self.assertEqual(snapshots,{p:(p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in output.rglob('*') if p.is_file()})
   self.assertFalse(list(output.rglob('*.aeui')))
  self.assertEqual(before,{p:hashlib.sha256(p.read_bytes()).hexdigest() for p in active})
if __name__=='__main__':unittest.main()
