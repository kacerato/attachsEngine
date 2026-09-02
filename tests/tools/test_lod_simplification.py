"""Unit tests for the grid-snap LOD simplifier in tools/cook-gltf-map.py.

These exercise the pure functions directly (no glTF ZIP, no astcenc, no
Android device) -- see docs/PLANO-OTIMIZACAO-GLOBAL-GRAFICOS.md: the LOD
system is implemented and native/renderer-tested this cycle, but genuinely
unvalidated on hardware; this suite is what actually runs today.
"""
import importlib.util, pathlib, struct, unittest
import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("cook", ROOT / "tools/cook-gltf-map.py")
cook = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cook)


def flat_grid(count_per_axis, spacing, uv_scale=1e-3):
    """A count x count grid of two-triangle quads on the XY plane, all
    sharing one normal/tangent/color and a UV that grows slowly with
    position (one seamless island) -- deliberately the easiest case for
    clustering to merge. uv_scale defaults small on purpose: real glTF UV
    varies smoothly and slowly across a dense, continuous surface, so
    neighboring vertices typically differ by far less than
    LOD_UV_BUCKET_SIZE; a synthetic grid this coarse (9-20 vertices per
    axis) needs an explicitly shallow UV gradient to be representative,
    or its handful of vertices would each land in their own UV bucket."""
    xs, ys = np.meshgrid(np.arange(count_per_axis, dtype=np.float32) * spacing,
                         np.arange(count_per_axis, dtype=np.float32) * spacing, indexing="ij")
    position = np.stack([xs.ravel(), ys.ravel(), np.zeros(xs.size, dtype=np.float32)], axis=1)
    normal = np.zeros_like(position); normal[:, 2] = 1.0
    tangent = np.zeros((len(position), 4), dtype=np.float32); tangent[:, 0] = 1.0; tangent[:, 3] = 1.0
    uv0 = (position[:, :2] * uv_scale).astype(np.float32)
    uv1 = uv0.copy()
    color = np.ones((len(position), 4), dtype=np.float32)
    indices = []
    for row in range(count_per_axis - 1):
        for col in range(count_per_axis - 1):
            a = row * count_per_axis + col
            b = a + 1
            c = a + count_per_axis
            d = c + 1
            indices += [a, b, c, b, d, c]
    return position, normal, tangent, uv0, uv1, color, np.array(indices, dtype=np.uint32)


class SimplifyByClustering(unittest.TestCase):
    def test_reduces_vertex_and_triangle_count_on_a_flat_grid(self):
        position, normal, tangent, uv0, uv1, color, indices = flat_grid(9, 0.5)
        original_triangles = len(indices) // 3
        result = cook.simplify_by_clustering(position, normal, tangent, uv0, uv1, color, indices,
                                             cell_size=1.1, uv_bucket_size=1.0 / 64.0,
                                             normal_bucket_count=6)
        self.assertIsNotNone(result)
        new_position, new_normal, _, _, _, _, new_indices = result
        self.assertLess(len(new_position), len(position), "clustering must merge some vertices")
        self.assertLess(len(new_indices) // 3, original_triangles, "triangle count must drop")
        self.assertEqual(len(new_indices) % 3, 0)
        np.testing.assert_allclose(np.linalg.norm(new_normal, axis=1), 1.0, atol=1e-5,
                                   err_msg="representative normals stay unit length")

    def test_bounds_never_expand_past_the_original(self):
        position, normal, tangent, uv0, uv1, color, indices = flat_grid(9, 0.5)
        new_position, *_ = cook.simplify_by_clustering(position, normal, tangent, uv0, uv1, color,
                                                        indices, cell_size=1.1,
                                                        uv_bucket_size=1.0 / 64.0, normal_bucket_count=6)
        self.assertTrue(np.all(new_position.min(axis=0) >= position.min(axis=0) - 1e-4))
        self.assertTrue(np.all(new_position.max(axis=0) <= position.max(axis=0) + 1e-4))

    def test_does_not_merge_across_a_uv_island_seam(self):
        # Two triangles at EXACTLY the same three positions (a duplicated
        # seam, as glTF exporters commonly produce at a UV cut) but on
        # opposite sides of a UV island boundary (uv jumps from ~0 to ~10).
        # cell_size is small enough that a triangle's own three corners stay
        # distinct by position alone -- the only question this test asks is
        # whether corner-for-corner-identical positions across the two
        # triangles wrongly weld together despite the UV gap.
        corners = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float32)
        position = np.concatenate([corners, corners])
        normal = np.tile([0, 0, 1], (6, 1)).astype(np.float32)
        tangent = np.zeros((6, 4), dtype=np.float32); tangent[:, 0] = 1.0; tangent[:, 3] = 1.0
        uv0 = np.concatenate([np.tile([0, 0], (3, 1)), np.tile([10, 10], (3, 1))]).astype(np.float32)
        uv1 = uv0.copy()
        color = np.ones((6, 4), dtype=np.float32)
        indices = np.array([0, 1, 2, 3, 4, 5], dtype=np.uint32)
        result = cook.simplify_by_clustering(position, normal, tangent, uv0, uv1, color, indices,
                                             cell_size=0.3, uv_bucket_size=1.0 / 4.0,
                                             normal_bucket_count=6)
        self.assertIsNotNone(result)
        new_position, *_, new_indices = result
        # Without the UV-island guard, every corner pair (identical position
        # AND identical normal) would collapse into one cluster, destroying
        # both triangles as degenerate. With the guard, both must survive.
        self.assertEqual(len(new_indices) // 3, 2, "UV seam prevents cross-island merging")
        self.assertEqual(len(new_position), 6, "no merging at all should happen in this fixture")

    def test_does_not_merge_across_a_sharp_normal_discontinuity(self):
        # Same construction as the UV test, but the seam is a hard
        # 90-degree corner (shared UV, opposite normals) instead of a UV
        # island boundary.
        corners = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0]], dtype=np.float32)
        position = np.concatenate([corners, corners])
        normal = np.concatenate([np.tile([0, 0, 1], (3, 1)), np.tile([1, 0, 0], (3, 1))]).astype(np.float32)
        tangent = np.zeros((6, 4), dtype=np.float32); tangent[:, 0] = 1.0; tangent[:, 3] = 1.0
        uv0 = np.zeros((6, 2), dtype=np.float32)
        uv1 = uv0.copy()
        color = np.ones((6, 4), dtype=np.float32)
        indices = np.array([0, 1, 2, 3, 4, 5], dtype=np.uint32)
        result = cook.simplify_by_clustering(position, normal, tangent, uv0, uv1, color, indices,
                                             cell_size=0.3, uv_bucket_size=1.0 / 4.0,
                                             normal_bucket_count=6)
        self.assertIsNotNone(result)
        _, _, _, _, _, _, new_indices = result
        self.assertEqual(len(new_indices) // 3, 2, "normal discontinuity prevents cross-corner merging")

    def test_continuous_uv_gradient_does_not_block_geometric_clustering(self):
        # Four nearby vertices form a continuous UV chart.  The old global UV
        # bucket key treated all four as unrelated and could never reduce this
        # geometry.  Only duplicate positions with discontinuous UVs are seams.
        position = np.array([[0.00, 0, 0], [0.04, 0, 0],
                             [0.00, 1, 0], [0.04, 1, 0], [1.0, 0.5, 0]], dtype=np.float32)
        normal = np.tile([0, 0, 1], (5, 1)).astype(np.float32)
        tangent = np.zeros((5, 4), dtype=np.float32); tangent[:, 0] = 1.0; tangent[:, 3] = 1.0
        uv0 = np.array([[0.00, 0], [0.04, 0], [0.00, 1], [0.04, 1], [1.0, 0.5]], dtype=np.float32)
        uv1 = uv0.copy()
        color = np.ones((5, 4), dtype=np.float32)
        # Both triangles remain valid after each close vertical pair clusters.
        indices = np.array([0, 2, 4, 1, 3, 4], dtype=np.uint32)
        result = cook.simplify_by_clustering(position, normal, tangent, uv0, uv1, color, indices,
                                             cell_size=0.1, uv_bucket_size=1.0 / 64.0,
                                             normal_bucket_count=6)
        self.assertIsNotNone(result)
        new_position, *_, new_indices = result
        self.assertEqual(len(new_position), 3)
        self.assertEqual(len(new_indices) // 3, 2)

    def test_collapsing_to_zero_triangles_returns_none(self):
        position = np.array([[0, 0, 0], [0.01, 0, 0], [0, 0.01, 0]], dtype=np.float32)
        normal = np.tile([0, 0, 1], (3, 1)).astype(np.float32)
        tangent = np.zeros((3, 4), dtype=np.float32); tangent[:, 0] = 1.0; tangent[:, 3] = 1.0
        uv0 = np.zeros((3, 2), dtype=np.float32)
        uv1 = uv0.copy()
        color = np.ones((3, 4), dtype=np.float32)
        indices = np.array([0, 1, 2], dtype=np.uint32)
        result = cook.simplify_by_clustering(position, normal, tangent, uv0, uv1, color, indices,
                                             cell_size=100.0, uv_bucket_size=1.0 / 4.0,
                                             normal_bucket_count=6)
        self.assertIsNone(result, "a single cell collapsing one triangle to a point must be rejected")


class SimplifyPrimitiveLevels(unittest.TestCase):
    def setUp(self):
        self.position, self.normal, self.tangent, self.uv0, self.uv1, self.color, self.indices = \
            flat_grid(20, 0.4)  # 19*19*2 = 722 triangles, well past LOD_MINIMUM_TRIANGLES.
        self.assertGreaterEqual(len(self.indices) // 3, cook.LOD_MINIMUM_TRIANGLES)

    def call(self, material_flags, bounds_radius=100.0):
        return cook.simplify_primitive_levels(self.position, self.normal, self.tangent, self.uv0,
                                              self.uv1, self.color, self.indices, material_flags,
                                              bounds_radius)

    def test_opaque_large_primitive_produces_multiple_increasing_error_levels(self):
        levels = self.call(material_flags=0)
        self.assertGreaterEqual(len(levels), 2, "a large flat opaque grid should simplify at least once")
        self.assertEqual(levels[0]["level"], 0)
        self.assertEqual(levels[0]["geometricError"], 0.0)
        for i in range(len(levels)):
            self.assertEqual(levels[i]["level"], i)
        errors = [entry["geometricError"] for entry in levels]
        self.assertEqual(errors, sorted(errors), "geometricError must be non-decreasing by level")
        self.assertEqual(len(set(errors)), len(errors), "geometricError must be strictly increasing")
        triangle_counts = [len(entry["indices"]) // 3 for entry in levels]
        self.assertEqual(triangle_counts, sorted(triangle_counts, reverse=True),
                         "triangle count must be strictly decreasing by level")
        self.assertEqual(len(triangle_counts), len(set(triangle_counts)))
        self.assertLessEqual(len(levels), cook.MAX_LOD_LEVELS)

    def test_blend_material_is_never_simplified(self):
        levels = self.call(material_flags=cook.MATERIAL_BLEND)
        self.assertEqual(len(levels), 1, "blend primitives must keep exactly the original level")
        self.assertEqual(levels[0]["level"], 0)

    def test_alpha_mask_material_can_generate_safe_vegetation_lods(self):
        levels = self.call(material_flags=cook.MATERIAL_ALPHA_MASK)
        self.assertGreaterEqual(len(levels), 2)
        self.assertEqual(levels[0]["level"], 0)
        self.assertGreater(levels[1]["geometricError"], 0.0)

    def test_alpha_cards_use_deterministic_component_density_levels(self):
        position, indices = [], []
        for card in range(20):
            base = len(position)
            x = float(card * 3)
            position.extend(((x, 0, 0), (x + 1, 0, 0),
                             (x, 1, 0), (x + 1, 1, 0)))
            indices.extend((base, base + 1, base + 2,
                            base + 1, base + 3, base + 2))
        position = np.asarray(position, dtype=np.float32)
        normal = np.tile([0, 0, 1], (len(position), 1)).astype(np.float32)
        tangent = np.zeros((len(position), 4), dtype=np.float32)
        tangent[:, 0] = tangent[:, 3] = 1.0
        uv0 = np.tile([[0, 0], [1, 0], [0, 1], [1, 1]], (20, 1)).astype(np.float32)
        color = np.ones((len(position), 4), dtype=np.float32)
        levels = cook.simplify_primitive_levels(
            position, normal, tangent, uv0, uv0.copy(), color,
            np.asarray(indices, dtype=np.uint32), cook.MATERIAL_ALPHA_MASK, 100.0,
            reuse_source_vertices=True)
        self.assertEqual(len(levels), 3)
        triangle_counts = [len(level["indices"]) // 3 for level in levels]
        self.assertEqual(triangle_counts[0], 40)
        self.assertLess(triangle_counts[1], triangle_counts[0])
        self.assertLess(triangle_counts[2], triangle_counts[1])
        self.assertEqual(levels[1]["sourceVertexIndices"].tolist(), list(range(80)))

    def test_small_primitive_is_never_simplified_even_when_opaque(self):
        tiny_position, tiny_normal, tiny_tangent, tiny_uv0, tiny_uv1, tiny_color, tiny_indices = \
            flat_grid(4, 0.4)  # 3*3*2 = 18 triangles, well under LOD_MINIMUM_TRIANGLES.
        levels = cook.simplify_primitive_levels(tiny_position, tiny_normal, tiny_tangent, tiny_uv0,
                                                tiny_uv1, tiny_color, tiny_indices, 0, 100.0)
        self.assertEqual(len(levels), 1)

    def test_source_vertex_mode_maps_every_lod_index_back_to_lod_zero(self):
        levels = cook.simplify_primitive_levels(
            self.position, self.normal, self.tangent, self.uv0, self.uv1,
            self.color, self.indices, 0, 100.0, reuse_source_vertices=True)
        self.assertGreaterEqual(len(levels), 2)
        for level in levels:
            representatives = level["sourceVertexIndices"]
            self.assertTrue(np.all(representatives < len(self.position)))
            self.assertTrue(np.all(level["indices"] < len(representatives)))


class SpatialPrimitiveChunks(unittest.TestCase):
    def test_chunks_are_bounded_deterministic_and_preserve_complete_triangles(self):
        position, *_rest, indices = flat_grid(12, 1.0)
        identity = np.identity(4, dtype=np.float32).reshape(16, order="F")
        first = cook.spatial_primitive_chunks(position, indices, identity, target_triangles=17)
        second = cook.spatial_primitive_chunks(position, indices, identity, target_triangles=17)
        self.assertEqual([chunk.tolist() for chunk in first], [chunk.tolist() for chunk in second])
        self.assertTrue(all(len(chunk) // 3 <= 17 for chunk in first))
        source_triangles = sorted(map(tuple, indices.reshape((-1, 3)).tolist()))
        chunk_triangles = sorted(map(tuple, np.concatenate(first).reshape((-1, 3)).tolist()))
        self.assertEqual(chunk_triangles, source_triangles)


class MaterialFlagsByteOffset(unittest.TestCase):
    def test_offset_68_matches_material_record_for_every_alpha_mode(self):
        # Regression test for a real bug caught during review: an earlier
        # draft of the cooker read the flags field from the wrong byte
        # offset (48, the start of the roughness/metallic/normalScale/
        # specular block) and would have silently fed the wrong material
        # flags into simplify_primitive_levels's blend/alpha-mask guard.
        for alpha_mode, expected_flag in (("OPAQUE", 0), ("MASK", cook.MATERIAL_ALPHA_MASK),
                                          ("BLEND", cook.MATERIAL_BLEND)):
            material = {"pbrMetallicRoughness": {}, "alphaMode": alpha_mode}
            record = cook.material_record(material, {}, frozenset())
            self.assertEqual(len(record), 80)
            (flags,) = struct.unpack_from("<I", record, offset=68)
            self.assertEqual(flags, expected_flag, alpha_mode)


if __name__ == "__main__":
    unittest.main()
