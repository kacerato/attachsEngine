import copy
import json
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from terrain_material_contracts import resolve_catalog, resolve_material_contract

class MaterialContracts(unittest.TestCase):
    def setUp(self):
        self.catalog=json.loads((ROOT/'assets/terrain/library/catalog.json').read_text(encoding='utf-8'))
        self.material=copy.deepcopy(next(m for m in self.catalog['materials'] if m['kind']=='MicroSplat' and 'height' in m['maps']))

    def test_global_channel_overrides_layer_and_custom_restores_it(self):
        m=self.material;m['packing']['allTextureChannelHeight']=3
        resolve_material_contract(m);self.assertEqual(m['resolved_maps']['height']['channel'],3)
        m['packing']['allTextureChannelHeight']=4;m['source_properties']['heightChannel']=2
        resolve_material_contract(m);self.assertEqual(m['resolved_maps']['height']['channel'],2)

    def test_linear_albedo_is_not_forced_to_srgb(self):
        m=self.material;m['packing']['diffuseIsLinear']=1
        resolve_material_contract(m);self.assertEqual(m['resolved_maps']['albedo']['color_space'],'linear')

    def test_roughness_map_does_not_get_inverted_twice(self):
        m=self.material;m['source_properties']['isRoughness']=1
        resolve_material_contract(m);self.assertFalse(m['resolved_maps']['smoothness']['roughness_invert'])
        m['source_properties']['isRoughness']=0
        resolve_material_contract(m);self.assertTrue(m['resolved_maps']['smoothness']['roughness_invert'])

    def test_terrain_mask_blue_is_height(self):
        m=copy.deepcopy(next(m for m in self.catalog['materials'] if m['kind']=='TerrainLayer'))
        m['maps']['mask']={'texture':'test-mask','color_space':'linear'}
        resolve_material_contract(m)
        for channel,role in enumerate(('metallic','ao','height','smoothness')):
            self.assertEqual(m['resolved_maps'][role]['channel'],channel)
            self.assertEqual(m['resolved_maps'][role]['texture'],'test-mask')

    def test_reject_invalid_channel_without_guessing(self):
        m=self.material;m['packing']['allTextureChannelHeight']=9
        with self.assertRaises(ValueError):resolve_material_contract(m)

    def test_catalog_references_and_source_pixels_are_unchanged(self):
        raw=copy.deepcopy(self.catalog['textures']);resolve_catalog(self.catalog)
        self.assertEqual(raw,self.catalog['textures'])
        ids={t['id'] for t in raw}
        self.assertTrue(all(v['texture'] in ids for m in self.catalog['materials'] for v in m['resolved_maps'].values()))
        material_ids={m['id'] for m in self.catalog['materials']}
        self.assertTrue(all(v in material_ids for m in self.catalog['materials'] for v in m['cluster_variants']))

if __name__=='__main__':unittest.main()
