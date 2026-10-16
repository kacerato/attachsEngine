"""Resolve source channel semantics without changing image pixels.

MicroSplat 3.9.0 TextureArrayConfig/TextureArrayConfigEditor and Unity 6000.0
TerrainLayer are the source contracts. Raw source_properties remain retained.
"""


def resolve_material_contract(material):
    props = material['source_properties']
    maps = material['maps']
    if material['kind'] == 'TerrainLayer':
        material['mask_contract'] = 'R metallic; G AO; B height; A smoothness'
        material['resolved_maps'] = {key: dict(value) for key, value in maps.items() if key != 'mask'}
        if 'mask' in maps:
            for channel, role in enumerate(('metallic', 'ao', 'height', 'smoothness')):
                material['resolved_maps'][role] = dict(maps['mask'], channel=channel, color_space='linear')
        material['defaults'] = {
            'tile_size': props.get('m_TileSize', {'x': 1, 'y': 1}),
            'tile_offset': props.get('m_TileOffset', {'x': 0, 'y': 0}),
            'normal_strength': props.get('m_NormalScale', 1),
            'metallic': props.get('m_Metallic', 0),
            'roughness': 1 - props.get('m_Smoothness', 0),
            'albedo_remap_min': props.get('m_DiffuseRemapMin'),
            'albedo_remap_max': props.get('m_DiffuseRemapMax'),
            'mask_remap_min': props.get('m_MaskMapRemapMin'),
            'mask_remap_max': props.get('m_MaskMapRemapMax'),
        }
        return
    packing = material['packing']
    aliases = {'diffuse': 'albedo', 'emis': 'emission', 'metal': 'metallic'}
    overrides = {'height': 'allTextureChannelHeight', 'ao': 'allTextureChannelAO',
                 'smoothness': 'allTextureChannelSmoothness',
                 'traxHeight': 'allTextureChannelHeight', 'traxAO': 'allTextureChannelAO',
                 'traxSmoothness': 'allTextureChannelSmoothness'}
    property_names = {'detailNoise': 'detailChannel', 'distanceNoise': 'distanceChannel'}
    resolved = {}
    for key, value in maps.items():
        item = dict(value)
        channel = props.get(property_names.get(key, key + 'Channel'))
        override = packing.get(overrides.get(key, ''))
        if override is not None and override != 4:  # Custom = 4
            channel = override
        if channel is not None:
            if channel not in (0, 1, 2, 3):
                raise ValueError(f'Invalid channel in {material["id"]}: {key}')
            item['channel'] = channel
        if key in ('diffuse', 'traxDiffuse') and packing.get('diffuseIsLinear'):
            item['color_space'] = 'linear'
        role = aliases.get(key, key)
        if key in ('smoothness', 'traxSmoothness'):
            # Preserve the authored map: the consumer knows whether inversion
            # is needed to obtain physical roughness, never inferred by filename.
            rough = bool(props.get('isRoughness' if key == 'smoothness' else 'traxIsRoughness'))
            item['roughness_invert'] = not rough
        resolved[role] = item
    material['resolved_maps'] = resolved
    material['defaults'] = {'tile_size': {'x': 1, 'y': 1}, 'tile_offset': {'x': 0, 'y': 0},
                            'normal_strength': 1, 'metallic': 0, 'roughness': 1}


def resolve_catalog(catalog):
    for material in catalog['materials']:
        resolve_material_contract(material)
        material['cluster_variants'] = []
        if material['kind'] == 'MicroSplat':
            material['cluster_variants'] = [other['id'] for other in sorted(
                (other for other in catalog['materials'] if other.get('cluster_set') == material['cluster_set']
                 and other.get('cluster_layer') == material['cluster_layer']),
                key=lambda other: other['cluster_variant'])]
    catalog['material_contract_version'] = 1
    catalog['material_reference'] = 'https://docs.unity.com/en-us/engine/6000.0/manual/creating-environments/script-terrain/class-terrain-layer'
    return catalog
