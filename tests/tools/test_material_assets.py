"""Host-side verification of cooked assets and offline mip/lighting contracts."""
import importlib.util, pathlib, unittest, hashlib, json, struct
import numpy as np
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location("cook",ROOT/"tools/cook-material-preview.py")
cook=importlib.util.module_from_spec(spec);spec.loader.exec_module(cook)

class MaterialAssets(unittest.TestCase):
    def test_all_payloads_match_manifest_and_exact_mip_lengths(self):
        folder=ROOT/"samples/material-preview"
        manifest=json.loads((folder/"manifest.json").read_text())
        self.assertEqual(manifest["license"],"CC0-1.0")
        self.assertEqual(len(manifest["outputs"]),8)
        for name,expected in manifest["outputs"].items():
            data=(folder/"Imported"/name).read_bytes()
            self.assertEqual(hashlib.sha256(data).hexdigest(),expected,name)
            magic,version,w,h,encoding,mips,length=struct.unpack("<6IQ",data[:32])
            self.assertEqual((magic,version),(0x58544541,1))
            self.assertEqual(length,len(data)-32)
            expected_size=0
            for _ in range(mips):
                expected_size+=((w+5)//6)*((h+5)//6)*16 if encoding in (1,2) else w*h*(8 if encoding==5 else 4)
                w=max(1,w//2);h=max(1,h//2)
            self.assertEqual(expected_size,length,name)

    def test_8k_is_actual_source_resolution_with_complete_mips(self):
        for name,encoding in (("albedo",1),("normal",2),("arm",2)):
            data=(ROOT/f"samples/material-preview/Imported/{name}.aetex").read_bytes()[:32]
            self.assertEqual(struct.unpack("<6IQ",data)[2:6],(8192,8192,encoding,14))

    def test_linear_light_mips_not_gamma_averaging(self):
        x=np.array([[[0.,0,0],[1.,1,1]],[[0.,0,0],[1.,1,1]]],np.float32)
        result=cook.srgb_encode(cook.resize_linear(cook.srgb_decode(x)))
        np.testing.assert_allclose(result,.73535698,atol=1e-6)

    def test_normal_mips_are_unit_vectors(self):
        normals=np.array([[[1.,0,1],[0.,1,1]],[[0.,0,1],[0.,0,1]]],np.float32)
        result=cook.normalize(cook.resize_linear(normals))
        self.assertAlmostEqual(float(np.linalg.norm(result)),1)

    def test_hdr_environment_and_brdf_are_finite_not_clipped_ldr(self):
        folder=ROOT/"samples/material-preview/Imported"
        env=np.frombuffer((folder/"studio.aetex").read_bytes()[32:],dtype="<f2")
        self.assertTrue(np.isfinite(env).all());self.assertGreater(float(env.max()),1)
        brdf=np.frombuffer((folder/"brdf.aetex").read_bytes()[32:],dtype="<f2").reshape(-1,4)
        self.assertTrue(np.isfinite(brdf).all());self.assertGreaterEqual(float(brdf.min()),0)
        self.assertLessEqual(float(brdf[:,:2].sum(axis=1).max()),1.01)

    def test_studio_direction_and_latlong_mapping_agree(self):
        # At +X, phi=0 -> u=.5; light values retain HDR dynamic range.
        direction=cook.normalize(np.array([-1.,1.,-1.]))
        self.assertGreater(float(cook.studio(direction).max()),5)

if __name__=="__main__":unittest.main()
