import importlib.util
import json
import pathlib
import tempfile
import unittest
from unittest import mock
from test_gltf_container import make_glb

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("exporter", ROOT / "tools/export-authoring-assets.py")
EXPORTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EXPORTER)


class IndependentResourcesTest(unittest.TestCase):
    def test_project_boat_preserves_all_vertex_attributes_and_indices(self):
        source=ROOT/"samples/boat/Source/boat.glb"
        if not source.exists():
            self.skipTest("boat source asset not present")
        original,binary=EXPORTER.COOK.read_glb(source.read_bytes())
        with tempfile.TemporaryDirectory() as temporary:
            output=pathlib.Path(temporary)
            catalog=EXPORTER.export_assets(source,output,"boat")
            count=0
            for entry in catalog["meshes"]:
                path=output/entry["sourcePath"]
                imported=json.loads(path.read_text())
                imported_binary=(path.parent/imported["buffers"][0]["uri"]).read_bytes()
                before=original["meshes"][entry["sourceMesh"]]["primitives"]
                after=imported["meshes"][0]["primitives"]
                self.assertEqual(len(before),len(after))
                for a,b in zip(before,after):
                    self.assertEqual(a,b)
                    for index in [*a["attributes"].values(),a["indices"]]:
                        self.assertTrue((EXPORTER.COOK.accessor(original,binary,index)==
                                         EXPORTER.COOK.accessor(imported,imported_binary,index)).all())
                    count+=1
            self.assertGreater(count,0)

    def fixture(self, root):
        doc = {"asset":{"version":"2.0"}, "buffers":[{"byteLength":4}],
               "meshes":[{"name":"Tree", "primitives":[{"attributes":{}}]}],
               "nodes":[{"mesh":0}], "scenes":[{"nodes":[0]}]}
        source=root/"source.glb"
        source.write_bytes(make_glb(doc,b"abcd"))
        return source, doc

    def test_reimport_keeps_old_resource_revision_readable(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=pathlib.Path(temporary);source,doc=self.fixture(root)
            output=root/"output"
            old=EXPORTER.export_assets(source,output,"trees")
            old_path=output/old["meshes"][0]["sourcePath"]
            old_bytes=old_path.read_bytes()
            doc["meshes"][0]["name"]="Changed"
            source.write_bytes(make_glb(doc,b"efgh"))
            new=EXPORTER.export_assets(source,output,"trees")
            self.assertEqual(old["meshes"][0]["id"],new["meshes"][0]["id"])
            self.assertNotEqual(old["meshes"][0]["sourcePath"],new["meshes"][0]["sourcePath"])
            self.assertEqual(old_path.read_bytes(),old_bytes)

    def test_invalid_import_does_not_publish_any_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=pathlib.Path(temporary);source,doc=self.fixture(root)
            doc["images"]=[{"uri":"missing.png"}]
            source.write_bytes(make_glb(doc,b"abcd"))
            with self.assertRaises((ValueError, OSError)):
                EXPORTER.export_assets(source,root/"output","trees")
            self.assertFalse((root/"output").exists())

    def test_gltf_external_buffers_and_image_are_self_contained(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=pathlib.Path(temporary);_,doc=self.fixture(root)
            doc["buffers"]=[{"uri":"first.bin","byteLength":3},{"uri":"second.bin","byteLength":4}]
            doc["bufferViews"]=[{"buffer":0,"byteLength":3},{"buffer":1,"byteLength":4}]
            doc["images"]=[{"uri":"color.png"}]
            (root/"first.bin").write_bytes(b"abc")
            (root/"second.bin").write_bytes(b"defg")
            png=b"\x89PNG\r\n\x1a\nexample"
            (root/"color.png").write_bytes(png)
            source=root/"model.gltf";source.write_text(json.dumps(doc))
            output=root/"output";catalog=EXPORTER.export_assets(source,output,"model")
            resource=json.loads((output/catalog["meshes"][0]["sourcePath"]).read_text())
            binary=(output/"resources"/resource["buffers"][0]["uri"]).read_bytes()
            for i,expected in enumerate((b"abc",b"defg",png)):
                self.assertEqual(EXPORTER.COOK.buffer_view_bytes(resource,binary,i),expected)
            self.assertNotIn("uri",resource["images"][0])
            self.assertEqual(len(catalog["dependencies"]),3)
            self.assertEqual(resource["bufferViews"][1]["byteOffset"]%4,0)

    def test_resource_cannot_escape_source_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=pathlib.Path(temporary);source,doc=self.fixture(root)
            doc["buffers"][0]["uri"]="../outside.bin"
            source.write_bytes(make_glb(doc,b"abcd"))
            with self.assertRaisesRegex(ValueError,"escapes"):
                EXPORTER.export_assets(source,root/"output","trees")
            self.assertFalse((root/"output").exists())

    def test_truncated_buffer_is_rejected_before_publication(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=pathlib.Path(temporary);source,doc=self.fixture(root)
            doc["buffers"][0]["byteLength"]=128
            source.write_bytes(make_glb(doc,b"abcd"))
            with self.assertRaisesRegex(ValueError,"byteLength"):
                EXPORTER.export_assets(source,root/"output","trees")
            self.assertFalse((root/"output").exists())

    def test_failed_catalog_commit_preserves_previous_import(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=pathlib.Path(temporary);source,doc=self.fixture(root)
            output=root/"output"
            EXPORTER.export_assets(source,output,"trees")
            old=(output/"scene.authoring.json").read_bytes()
            source.write_bytes(make_glb(doc,b"efgh"))
            replace=EXPORTER.os.replace
            def fail_catalog(src,dst):
                if pathlib.Path(dst).name=="scene.authoring.json":
                    raise OSError("simulated publication failure")
                replace(src,dst)
            with mock.patch.object(EXPORTER.os,"replace",side_effect=fail_catalog):
                with self.assertRaises(OSError):
                    EXPORTER.export_assets(source,output,"trees")
            self.assertEqual((output/"scene.authoring.json").read_bytes(),old)
            self.assertFalse(list(output.rglob(".import-*")))

    def test_objects_share_resource_and_buffer_but_keep_transforms(self):
        doc = {"asset":{"version":"2.0"}, "buffers":[{"byteLength":4}],
               "bufferViews":[{"buffer":0,"byteLength":4}],
               "meshes":[{"name":"Tree", "primitives":[{"attributes":{}}]}],
               "nodes":[{"mesh":0,"translation":[1,2,3]}, {"mesh":0,"translation":[4,5,6]}],
               "scenes":[{"nodes":[0,1]}]}
        with tempfile.TemporaryDirectory() as temporary:
            root=pathlib.Path(temporary)
            source=root/"source.glb"; source.write_bytes(make_glb(doc,b"abcd"))
            catalog=EXPORTER.export_assets(source,root/"output","trees")
            self.assertEqual(len(catalog["meshes"]),1)
            self.assertEqual(catalog["objects"][0]["mesh"],catalog["objects"][1]["mesh"])
            self.assertEqual(catalog["objects"][1]["localMatrix"][12:15],[4,5,6])
            resource=json.loads((root/"output"/catalog["meshes"][0]["sourcePath"]).read_text())
            self.assertEqual(resource["nodes"],[{"name":"Tree","mesh":0}])
            self.assertEqual((root/"output/resources"/resource["buffers"][0]["uri"]).read_bytes(),b"abcd")
            again=EXPORTER.export_assets(source,root/"output","trees")
            self.assertEqual(catalog,again)
            self.assertEqual(len(list((root/"output/resources").glob("*.bin"))),1)
