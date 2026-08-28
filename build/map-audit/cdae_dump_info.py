import argparse
import sys
import struct
import msgpack
import logging
import zstandard as zstd  # You'll need to install this: pip install zstandard

# --------------------------------------------------------------------------------
# Logger setup with optional color formatting for different log levels
# --------------------------------------------------------------------------------

COLOR_RESET  = "\033[0m"
COLOR_RED    = "\033[31m"
COLOR_GREEN  = "\033[32m"
COLOR_YELLOW = "\033[33m"
COLOR_BLUE   = "\033[34m"

class ColorFormatter(logging.Formatter):
    """
    Simple color formatter using ANSI escape codes to highlight log levels.
    """
    LEVEL_COLORS = {
        logging.DEBUG: COLOR_BLUE,
        logging.INFO: COLOR_GREEN,
        logging.WARNING: COLOR_YELLOW,
        logging.ERROR: COLOR_RED,
        logging.CRITICAL: COLOR_RED
    }

    def format(self, record):
        level_color = self.LEVEL_COLORS.get(record.levelno, COLOR_RESET)
        msg = super().format(record)
        return f"{level_color}{msg}{COLOR_RESET}"

logger = logging.getLogger("cdae_dump_info")
logger.setLevel(logging.DEBUG)

handler = logging.StreamHandler()
handler.setLevel(logging.DEBUG)
handler.setFormatter(ColorFormatter("[%(levelname)s] %(message)s"))
logger.addHandler(handler)

# --------------------------------------------------------------------------------
# Material flags decoding helper, from tsMaterialList.h
# --------------------------------------------------------------------------------

def decode_material_flags(flags: int) -> str:
    """
    Decodes the material flags from tsMaterialList.h:

        S_Wrap             = BNG_BIT(0),
        T_Wrap             = BNG_BIT(1),
        Translucent        = BNG_BIT(2),
        Additive           = BNG_BIT(3),
        Subtractive        = BNG_BIT(4),
        SelfIlluminating   = BNG_BIT(5),
        NeverEnvMap        = BNG_BIT(6),
        NoMipMap           = BNG_BIT(7),
        MipMap_ZeroBorder  = BNG_BIT(8),
        AuxiliaryMap       = BNG_BIT(27..31)
    """
    results = []
    if flags & 0x00000001:
        results.append("S_Wrap")
    if flags & 0x00000002:
        results.append("T_Wrap")
    if flags & 0x00000004:
        results.append("Translucent")
    if flags & 0x00000008:
        results.append("Additive")
    if flags & 0x00000010:
        results.append("Subtractive")
    if flags & 0x00000020:
        results.append("SelfIlluminating")
    if flags & 0x00000040:
        results.append("NeverEnvMap")
    if flags & 0x00000080:
        results.append("NoMipMap")
    if flags & 0x00000100:
        results.append("MipMap_ZeroBorder")
    # bits 27..31 => 0xF8000000
    if flags & 0xF8000000:
        results.append("AuxiliaryMap")
    return ", ".join(results) if results else "none"

# --------------------------------------------------------------------------------
# decode_mesh_flags: for reading top bits from tsMesh.h
# --------------------------------------------------------------------------------
def decode_mesh_flags(flags: int) -> str:
    """
    Billboard       = 1<<31,
    HasDetailTexture= 1<<30,
    BillboardZAxis  = 1<<29,
    UseEncodedNormals=1<<28,
    ...
    """
    results = []
    if flags & 0x80000000:
        results.append("Billboard")
    if flags & 0x40000000:
        results.append("HasDetailTexture")
    if flags & 0x20000000:
        results.append("BillboardZAxis")
    if flags & 0x10000000:
        results.append("UseEncodedNormals")
    return ", ".join(results) if results else "none"

# --------------------------------------------------------------------------------
# Main parsing function: parse_v31_file
# --------------------------------------------------------------------------------

def parse_v31_file(filepath: str, generate_html=True):
    """
    Parses a v31 TSShape file with separate uncompressed header and compressed body.

    File layout:
    a) i32 version number
    b) u32 headerSize
    c) bytes[] header (MsgPack)
    d) bytes[] body (possibly compressed with Zstd)

    Args:
        filepath: Path to the shape file
        generate_html: Whether to generate an HTML report
    """
    # Create HTML report data structure if needed
    html_data = {}
    if generate_html:
        html_data = {
            "filepath": filepath,
            "header_info": {},
            "shape_info": {
                "bounds": {},
                "nodes": [],
                "objects": [],
                "subshapes": {},
                "details": [],
                "meshes": [],
                "sequences": [],
                "materials": []
            }
        }

    # 1) Open the file and read the initial 4 bytes to check shape version.
    with open(filepath, "rb") as f:
        version_bytes = f.read(4)
        if len(version_bytes) < 4:
            logger.error("File is too small to contain version data.")
            return
        version_val = struct.unpack("<I", version_bytes)[0]
        exporter_version = version_val >> 16
        shape_version = version_val & 0xFF

        if generate_html:
            html_data["version"] = shape_version
            html_data["exporter_version"] = exporter_version

        if shape_version != 31:
            logger.error(f"Not a v31 shape (found shape_version={shape_version}, exporter={exporter_version}). Stopping.")
            return

        # 2) Read the header size
        header_size_bytes = f.read(4)
        if len(header_size_bytes) < 4:
            logger.error("File is too small to contain header size.")
            return
        header_size = struct.unpack("<I", header_size_bytes)[0]

        # 3) Read the uncompressed header
        header_data = f.read(header_size)
        if len(header_data) < header_size:
            logger.error(f"Could not read full header (expected {header_size} bytes, got {len(header_data)}).")
            return

        # 4) Parse the header with msgpack
        header_info = {}
        try:
            unpacker = msgpack.Unpacker()
            unpacker.feed(header_data)
            header_obj = next(unpacker)

            if isinstance(header_obj, dict):
                for key, value in header_obj.items():
                    header_info[key] = value
            else:
                logger.error(f"Header is not a dictionary: {type(header_obj)}")
                return
        except Exception as e:
            logger.error(f"Error parsing header: {e}")
            return

        logger.info(f"Header info: {header_info}")
        if generate_html:
            html_data["header_info"] = header_info

        # Check if body is compressed
        is_compressed = header_info.get('compression', False)
        body_size = header_info.get('bodysize', 0)

        # 5) Read the body
        body_data = f.read()
        if len(body_data) < body_size:
            logger.error(f"Could not read full body (expected {body_size} bytes, got {len(body_data)}).")
            return

        # 6) Decompress the body if needed
        if is_compressed:
            logger.info(f"Body is compressed with Zstd, decompressing...")
            try:
                dctx = zstd.ZstdDecompressor()
                body_data = dctx.decompress(body_data)
            except Exception as e:
                logger.error(f"Error decompressing body: {e}")
                return

        # 7) Parse the body with msgpack
        unpacker = msgpack.Unpacker()
        unpacker.feed(body_data)

    # --------------------------------------------------------------------------------
    # Helpers to read basic data from the msgpack stream
    # --------------------------------------------------------------------------------

    def next_obj():
        """
        Reads the next object from the msgpack.Unpacker.
        Returns None if we run out of data.
        """
        try:
            return next(unpacker)
        except StopIteration:
            return None

    def read_int(label="(int)"):
        """
        Reads either an int or a float that is integral (which we'll convert to int).
        Exits with error if the value is not integral.
        """
        val = next_obj()
        if isinstance(val, int):
            return val
        if isinstance(val, float):
            if abs(val - round(val)) < 1e-9:
                return int(round(val))
            logger.error(f"read_int('{label}'): got float={val} but not integral. Exiting.")
            sys.exit(1)
        logger.error(f"read_int('{label}'): expected int, got {type(val)} => {val}. Exiting.")
        sys.exit(1)

    def read_uint32(label="(uint32)"):
        """
        Same as read_int but must be non-negative.
        """
        val = read_int(label)
        if val < 0:
            logger.error(f"read_uint32('{label}'): got negative {val}. Exiting.")
            sys.exit(1)
        return val

    def read_float(label="(float)"):
        """
        Reads a float or an int -> float, else errors.
        """
        val = next_obj()
        if isinstance(val, float):
            return val
        if isinstance(val, int):
            return float(val)
        logger.error(f"read_float('{label}'): expected float/int, got {type(val)} => {val}. Exiting.")
        sys.exit(1)

    def read_str(label="(string)"):
        """
        Reads a string from the stream.
        """
        val = next_obj()
        if not isinstance(val, str):
            logger.error(f"read_str('{label}'): expected string, got {type(val)} => {val}. Exiting.")
            sys.exit(1)
        return val

    def read_point3f(label="(Point3F)"):
        """
        Reads either a 3-element list [x, y, z] or a 12-byte buffer -> <3f
        """
        val = next_obj()
        if isinstance(val, list):
            if len(val) != 3:
                logger.error(f"read_point3f('{label}'): list length {len(val)} != 3. Exiting.")
                sys.exit(1)
            return [float(x) for x in val]
        elif isinstance(val, bytes):
            if len(val) != 12:
                logger.error(f"read_point3f('{label}'): need 12 bytes, got {len(val)}. Exiting.")
                sys.exit(1)
            return list(struct.unpack("<3f", val))
        logger.error(f"read_point3f('{label}'): got {type(val)}, not list/bytes. Exiting.")
        sys.exit(1)

    def read_box6f(label="(Box3F)"):
        """
        Reads either a 6-element list [minX, minY, minZ, maxX, maxY, maxZ]
        or a 24-byte buffer -> <6f
        """
        val = next_obj()
        if isinstance(val, list):
            if len(val) != 6:
                logger.error(f"read_box6f('{label}'): list length {len(val)} != 6. Exiting.")
                sys.exit(1)
            return [float(x) for x in val]
        elif isinstance(val, bytes):
            if len(val) != 24:
                logger.error(f"read_box6f('{label}'): need 24 bytes, got {len(val)}. Exiting.")
                sys.exit(1)
            return list(struct.unpack("<6f", val))
        logger.error(f"read_box6f('{label}'): got {type(val)}, not list/bytes. Exiting.")
        sys.exit(1)

    def read_bytes(label="(bytes)"):
        """
        Reads a bytes object. Exits if we don't get 'bytes'.
        """
        val = next_obj()
        if not isinstance(val, bytes):
            logger.error(f"read_bytes('{label}'): expected bytes, got {type(val)} => {val}. Exiting.")
            sys.exit(1)
        return val

    # --------------------------------------------------------------------------------
    # Vector reading logic: 'pack_vector(...)' in C++ => read_vector() here
    # We also keep a dictionary of "expected" element sizes for debugging.
    # --------------------------------------------------------------------------------

    vector_type_expectations = {
        "nodes":                      ("Node data?",               20),
        "objects":                    ("TSShape::Object",          24),
        "subShapeFirstNode":          ("int",                      4),
        "subShapeFirstObject":        ("int",                      4),
        "subShapeNumNodes":           ("int",                      4),
        "subShapeNumObjects":         ("int",                      4),
        "defaultRotations":           ("Quat16",                   8),
        "defaultTranslations":        ("Point3F",                  12),
        "nodeRotations":              ("Quat16",                   8),
        "nodeTranslations":           ("Point3F",                  12),
        "nodeUniformScales":          ("float",                    4),
        "nodeAlignedScales":          ("Point3F",                  12),
        "nodeArbitraryScaleFactors":  ("Point3F",                  12),
        "nodeArbitraryScaleRots":     ("Quat16",                   8),
        "groundTranslations":         ("Point3F",                  12),
        "groundRotations":            ("Quat16",                   8),
        "objectStates":               ("TSShape::ObjectState",     12),  # 1 float + 2 int
        "triggers":                   ("TSShape::Trigger",         8),   # 1 u32 + 1 float
        "details":                    ("TSShape::Detail",          52),  # 13 fields total
        "mesh.verts":                 ("Point3F?",                 12),
        "mesh.tverts":                ("Point2F",                  8),
        "mesh.tverts2":               ("Point2F",                  8),
        "mesh.colors":                ("RGBA",                     4),
        "mesh.norms":                 ("Point3F",                  12),
        "mesh.encodedNorms":          ("PackedNorm",               1),
        "mesh.primitives":            ("TSDrawPrimitive",          12),
        "mesh.indices":               ("uint32?",                  4),
        "mesh.tangents":              ("Tangent?",                 16),
        "(vector)":                   ("Unknown",                  None),
    }

    def get_expected_info(label):
        """
        Returns (typeHint, expectedElemSize) from dictionary, or a fallback.
        Used only for debug warnings; doesn't kill program if mismatch.
        """
        return vector_type_expectations.get(label, vector_type_expectations["(vector)"])

    def read_vector(label="(vector)"):
        """
        Reads:
          1) size_val   (number of elements)
          2) elem_val   (bytes per element)
          3) data_buf
        Usually matches the usage 'packer.pack_vector(...)' in the C++ code.
        """
        size_val = read_int(label + ".size")
        elem_val = read_int(label + ".elemSize")
        data_buf = read_bytes(label + ".data")

        (type_hint, expected_size) = get_expected_info(label)
        if expected_size is not None and elem_val != expected_size:
            logger.warning(f"For '{label}', expected elemSize={expected_size} but got {elem_val}.")
        return (size_val, elem_val, data_buf)

    # --------------------------------------------------------------------------------
    # decode_integerset: used for reading TSIntegerSet bits (like rotationMatters, etc.)
    # --------------------------------------------------------------------------------
    def decode_integerset():
        val = next_obj()
        if isinstance(val, bytes):
            return val
        elif isinstance(val, list):
            return val
        else:
            logger.error(f"decode_integerset: got {type(val)} => {val}, not bytes/list. Exiting.")
            sys.exit(1)

    # --------------------------------------------------------------------------------
    # 2) Reading data from the body
    # --------------------------------------------------------------------------------

    # a) Shape-level floats, bounding data
    smallest_visible_size = read_float("mSmallestVisibleSize")
    smallest_visible_dl   = read_int("mSmallestVisibleDL")
    radius                = read_float("radius")
    tube_radius           = read_float("tubeRadius")
    center                = read_point3f("center")
    bounds                = read_box6f("bounds")

    logger.info(f"smallest_visible_size={smallest_visible_size}, smallest_visible_dl={smallest_visible_dl}")
    logger.info(f"radius={radius}, tube_radius={tube_radius}")
    logger.info(f"center={center}, bounds={bounds}")

    if generate_html:
        html_data["shape_info"]["smallest_visible_size"] = smallest_visible_size
        html_data["shape_info"]["smallest_visible_dl"] = smallest_visible_dl
        html_data["shape_info"]["radius"] = radius
        html_data["shape_info"]["tube_radius"] = tube_radius
        html_data["shape_info"]["center"] = center
        html_data["shape_info"]["bounds"] = {
            "min": bounds[:3],
            "max": bounds[3:]
        }

    # b) parse vectors in the order
    nodes_vec = read_vector("nodes")
    logger.debug(f"nodes_vec => size={nodes_vec[0]}, elemSize={nodes_vec[1]}, data_len={len(nodes_vec[2])}")

    # parse Node array -> 5 i32
    num_nodes = nodes_vec[0]
    node_elem_size = nodes_vec[1]
    node_data_buf = nodes_vec[2]
    if node_elem_size != 20:
        logger.warning(f"For 'nodes', expected 20 bytes each, got {node_elem_size}!")

    node_list = []
    for i in range(num_nodes):
        offset = i * 20
        chunk = node_data_buf[offset : offset + 20]
        (nNameIdx, nParentIdx, firstObj, firstChild, nextSibling) = struct.unpack("<5i", chunk)
        node_data = {
            "nameIndex":   nNameIdx,
            "parentIndex": nParentIdx,
            "firstObject": firstObj,
            "firstChild":  firstChild,
            "nextSibling": nextSibling
        }
        node_list.append(node_data)
        if generate_html:
            html_data["shape_info"]["nodes"].append(node_data)

    logger.debug("Parsed nodes:")
    for i, nd in enumerate(node_list[:10]):
        logger.debug(f"  Node[{i}] = {nd}")
    if len(node_list) > 10:
        logger.debug(f"  (Truncated {len(node_list) - 10} more nodes...)")

    # parse objects -> each is 6 i32
    objects_vec = read_vector("objects")
    logger.debug(f"objects_vec => size={objects_vec[0]}, elemSize={objects_vec[1]}, data_len={len(objects_vec[2])}")

    (num_objects_vec, object_elem_size, object_data_buf) = objects_vec
    object_list = []
    if object_elem_size == 24:
        for i in range(num_objects_vec):
            offset = i * 24
            chunk = object_data_buf[offset : offset + 24]
            (oNameIdx, oNumMeshes, oStartMesh, oNodeIdx, oNextSibling, oFirstDecal) = struct.unpack("<6i", chunk)
            obj_data = {
                "nameIndex":      oNameIdx,
                "numMeshes":      oNumMeshes,
                "startMeshIndex": oStartMesh,
                "nodeIndex":      oNodeIdx,
                "nextSibling":    oNextSibling,
                "firstDecal":     oFirstDecal  # DEPRECATED
            }
            object_list.append(obj_data)
            if generate_html:
                html_data["shape_info"]["objects"].append(obj_data)

        logger.debug("Parsed objects:")
        for i, obj in enumerate(object_list[:10]):
            logger.debug(f"  Object[{i}] = {obj}")
        if len(object_list) > 10:
            logger.debug(f"  (Truncated {len(object_list) - 10} more objects...)")

    subshapefn_vec = read_vector("subShapeFirstNode")
    logger.debug(f"subShapeFirstNode => size={subshapefn_vec[0]}, elemSize={subshapefn_vec[1]}, data_len={len(subshapefn_vec[2])}")

    subshapefobj_vec = read_vector("subShapeFirstObject")
    logger.debug(f"subShapeFirstObject => size={subshapefobj_vec[0]}, elemSize={subshapefobj_vec[1]}, data_len={len(subshapefobj_vec[2])}")

    subshapenumN_vec = read_vector("subShapeNumNodes")
    logger.debug(f"subShapeNumNodes => size={subshapenumN_vec[0]}, elemSize={subshapenumN_vec[1]}, data_len={len(subshapenumN_vec[2])}")

    # parse subShapeNumNodes -> array of i32
    (count_subshapeN, elemsize_subshapeN, data_subshapeN) = subshapenumN_vec
    subshapenumN_list = []
    for i in range(count_subshapeN):
        offset = i * 4
        chunk = data_subshapeN[offset : offset + 4]
        (val_i,) = struct.unpack("<i", chunk)
        subshapenumN_list.append(val_i)
    logger.debug(f"subshapenumN_list={subshapenumN_list}")

    total_subshape_nodes = sum(subshapenumN_list)
    if total_subshape_nodes != len(node_list):
        logger.warning(f"Mismatch: subshapes say {total_subshape_nodes} nodes, but read {len(node_list)} total.")

    subshapenumO_vec = read_vector("subShapeNumObjects")
    logger.debug(f"subShapeNumObjects => size={subshapenumO_vec[0]}, elemSize={subshapenumO_vec[1]}, data_len={len(subshapenumO_vec[2])}")

    # Default rotations, translations, etc...
    defrots_vec   = read_vector("defaultRotations")
    deftrans_vec  = read_vector("defaultTranslations")
    noderots_vec  = read_vector("nodeRotations")
    nodetrans_vec = read_vector("nodeTranslations")
    nodeUnif_vec  = read_vector("nodeUniformScales")
    nodeAlign_vec = read_vector("nodeAlignedScales")
    nodeArbFactors_vec = read_vector("nodeArbitraryScaleFactors")
    nodeArbRots_vec    = read_vector("nodeArbitraryScaleRots")
    groundTrans_vec    = read_vector("groundTranslations")
    groundRots_vec     = read_vector("groundRotations")

    logger.debug(f"defaultRotations => size={defrots_vec[0]}, elemSize={defrots_vec[1]}, data_len={len(defrots_vec[2])}")
    logger.debug(f"defaultTranslations => size={deftrans_vec[0]}, elemSize={deftrans_vec[1]}, data_len={len(deftrans_vec[2])}")
    logger.debug(f"nodeRotations => size={noderots_vec[0]}, elemSize={noderots_vec[1]}, data_len={len(noderots_vec[2])}")
    logger.debug(f"nodeTranslations => size={nodetrans_vec[0]}, elemSize={nodetrans_vec[1]}, data_len={len(nodetrans_vec[2])}")
    logger.debug(f"nodeUniformScales => size={nodeUnif_vec[0]}, elemSize={nodeUnif_vec[1]}, data_len={len(nodeUnif_vec[2])}")
    logger.debug(f"nodeAlignedScales => size={nodeAlign_vec[0]}, elemSize={nodeAlign_vec[1]}, data_len={len(nodeAlign_vec[2])}")
    logger.debug(f"nodeArbitraryScaleFactors => size={nodeArbFactors_vec[0]}, elemSize={nodeArbFactors_vec[1]}, data_len={len(nodeArbFactors_vec[2])}")
    logger.debug(f"nodeArbitraryScaleRots => size={nodeArbRots_vec[0]}, elemSize={nodeArbRots_vec[1]}, data_len={len(nodeArbRots_vec[2])}")
    logger.debug(f"groundTranslations => size={groundTrans_vec[0]}, elemSize={groundTrans_vec[1]}, data_len={len(groundTrans_vec[2])}")
    logger.debug(f"groundRotations => size={groundRots_vec[0]}, elemSize={groundRots_vec[1]}, data_len={len(groundRots_vec[2])}")

    # objectStates -> e.g. TSShape::ObjectState (12 bytes each)
    objectStates_vec = read_vector("objectStates")
    logger.debug(f"objectStates => size={objectStates_vec[0]}, elemSize={objectStates_vec[1]}, data_len={len(objectStates_vec[2])}")
    (num_objstates, objstate_elem_size, objstate_data) = objectStates_vec
    objstate_list = []
    if objstate_elem_size == 12:
        for i in range(num_objstates):
            offset = i * 12
            chunk = objstate_data[offset : offset + 12]
            (vis_f, frameIndex, matFrameIndex) = struct.unpack("<fii", chunk)
            objstate_list.append({
                "vis":           vis_f,
                "frameIndex":    frameIndex,
                "matFrameIndex": matFrameIndex
            })
        logger.debug("Parsed objectStates:")
        for i, st in enumerate(objstate_list[:10]):
            logger.debug(f"  ObjectState[{i}] = {st}")
        if len(objstate_list) > 10:
            logger.debug(f"  (Truncated {len(objstate_list) - 10} more objectStates...)")

    # triggers -> we consider them 1 u32 + 1 float => 8 bytes each
    triggers_vec = read_vector("triggers")
    logger.debug(f"triggers => size={triggers_vec[0]}, elemSize={triggers_vec[1]}, data_len={len(triggers_vec[2])}")
    (num_triggers, trigger_elem_size, trigger_data) = triggers_vec
    if trigger_elem_size == 8:
        trigger_list = []
        for i in range(num_triggers):
            offset = i * 8
            chunk = trigger_data[offset : offset + 8]
            (state_u32, pos_f) = struct.unpack("<If", chunk)
            trigger_list.append({
                "state": state_u32,
                "pos":   pos_f
            })
        logger.debug("Parsed triggers:")
        for i, t in enumerate(trigger_list[:10]):
            logger.debug(f"  Trigger[{i}] = {t}")
        if len(trigger_list) > 10:
            logger.debug(f"  (Truncated {len(trigger_list) - 10} more triggers...)")

    # details -> 13 fields => 52 bytes
    details_vec = read_vector("details")
    logger.debug(f"details => size={details_vec[0]}, elemSize={details_vec[1]}, data_len={len(details_vec[2])}")
    (num_details, detail_elem_size, detail_data) = details_vec

    logger.debug(f"detail_data type: {type(detail_data)}")

    detail_list = []
    if detail_elem_size == 52:
        # Make sure detail_data is bytes (sliceable) before trying to access it with a slice
        if not isinstance(detail_data, bytes):
            logger.error(f"Expected bytes for detail_data, got {type(detail_data)}. Cannot parse details.")
            # Try to convert if it's a list or other iterable
            try:
                if hasattr(detail_data, '__iter__'):
                    detail_data = bytes(detail_data)
                    logger.info(f"Converted detail_data to bytes, continuing...")
                else:
                    logger.error(f"Cannot convert detail_data to bytes. Skipping details parsing.")
                    detail_list = []  # Empty list as fallback
            except Exception as e:
                logger.error(f"Error converting detail_data: {e}")
                detail_list = []  # Empty list as fallback

        # Only proceed if we have bytes
        if isinstance(detail_data, bytes):
            for i in range(num_details):
                offset = i * 52
                chunk = detail_data[offset : offset + 52]
                (nmIdx, subShapeNum, objDetNum, sizeF,
                 avgErr, maxErr, polyCount,
                 bbDim, bbDetLvl, bbEqSteps,
                 bbPolSteps, bbPolAngle, bbIncludePoles) = struct.unpack("<3i3f i 2i 2I f I", chunk)
                detail_data_obj = {
                    "nameIndex":       nmIdx,
                    "subShapeNum":     subShapeNum,
                    "objectDetailNum": objDetNum,
                    "size":            sizeF,
                    "averageError":    avgErr,
                    "maxError":        maxErr,
                    "polyCount":       polyCount,
                    "bbDimension":     bbDim,
                    "bbDetailLevel":   bbDetLvl,
                    "bbEquatorSteps":  bbEqSteps,
                    "bbPolarSteps":    bbPolSteps,
                    "bbPolarAngle":    bbPolAngle,
                    "bbIncludePoles":  bbIncludePoles
                }
                detail_list.append(detail_data_obj)
                if generate_html:
                    html_data["shape_info"]["details"].append(detail_data_obj)

        logger.debug("Parsed details:")
        for i, dt in enumerate(detail_list[:10]):
            logger.debug(f"  Detail[{i}] = {dt}")
        if len(detail_list) > 10:
            logger.debug(f"  (Truncated {len(detail_list) - 10} more details...)")

    # e) read # of 'names', then the names themselves
    num_names = read_uint32("names.size")
    all_names = []
    for _ in range(num_names):
        s = read_str("name")
        all_names.append(s)
    logger.debug(f"num_names={num_names}, e.g. first few={all_names[:5]}")

    if generate_html:
        html_data["names"] = all_names

    # f) read total # of meshes
    total_meshes = read_uint32("meshes.size")
    logger.debug(f"total_meshes={total_meshes}")

    # A dictionary mapping mesh type -> name
    mesh_type_map = {
        0: "StandardMesh",
        1: "SkinMesh",
        2: "DecalMesh",
        3: "SortedMesh",
        4: "NullMesh"
    }

    # One for-loop per mesh
    for mesh_i in range(total_meshes):
        mesh_type_val = read_uint32("mesh_type")
        mesh_type_name = mesh_type_map.get(mesh_type_val, f"Unknown({mesh_type_val})")
        logger.info(f"Mesh {mesh_i}: type={mesh_type_val} => {mesh_type_name}")

        mesh_data = {
            "index": mesh_i,
            "type": mesh_type_val,
            "type_name": mesh_type_name
        }

        if mesh_type_val == 4:  # NullMesh
            if generate_html:
                html_data["shape_info"]["meshes"].append(mesh_data)
            continue

        mesh_numFrames    = read_int("mesh_numFrames")
        mesh_numMatFrames = read_int("mesh_numMatFrames")
        mesh_parentMesh   = read_int("mesh_parentMesh")
        mesh_bounds       = read_box6f("mesh_bounds")
        mesh_center       = read_point3f("mesh_center")
        mesh_radius       = read_float("mesh_radius")

        mesh_data.update({
            "numFrames": mesh_numFrames,
            "numMatFrames": mesh_numMatFrames,
            "parentMesh": mesh_parentMesh,
            "bounds": {
                "min": mesh_bounds[:3],
                "max": mesh_bounds[3:]
            },
            "center": mesh_center,
            "radius": mesh_radius
        })

        logger.debug(f"    frames={mesh_numFrames}, matframes={mesh_numMatFrames}, radius={mesh_radius}")

        verts_vec       = read_vector("mesh.verts")
        logger.debug(f"    mesh.verts => size={verts_vec[0]}, elemSize={verts_vec[1]}, data_len={len(verts_vec[2])}")

        tverts_vec      = read_vector("mesh.tverts")
        logger.debug(f"    mesh.tverts => size={tverts_vec[0]}, elemSize={tverts_vec[1]}, data_len={len(tverts_vec[2])}")

        tverts2_vec     = read_vector("mesh.tverts2")
        logger.debug(f"    mesh.tverts2 => size={tverts2_vec[0]}, elemSize={tverts2_vec[1]}, data_len={len(tverts2_vec[2])}")

        colors_vec      = read_vector("mesh.colors")
        logger.debug(f"    mesh.colors => size={colors_vec[0]}, elemSize={colors_vec[1]}, data_len={len(colors_vec[2])}")
        # Possibly parse them as 4 bytes => RGBA
        (color_count, color_elem_size, color_data_buf) = colors_vec
        if color_elem_size == 4:
            color_list = []
            for i in range(color_count):
                offset = i * 4
                chunk = color_data_buf[offset : offset + 4]
                (r, g, b, a) = struct.unpack("<4B", chunk)
                color_list.append({"r": r, "g": g, "b": b, "a": a})
            # Show only first 2 for brevity
            for i, c in enumerate(color_list[:2]):
                hex_rgba = f"#{c['r']:02X}{c['g']:02X}{c['b']:02X}{c['a']:02X}"
                logger.debug(f"     colors[{i}] = {c}  => {hex_rgba}")
            if color_count > 2:
                logger.debug(f"     (Truncated {color_count - 2} more colors...)")

        norms_vec       = read_vector("mesh.norms")
        logger.debug(f"    mesh.norms => size={norms_vec[0]}, elemSize={norms_vec[1]}, data_len={len(norms_vec[2])}")

        encnorms_vec    = read_vector("mesh.encodedNorms")
        logger.debug(f"    mesh.encodedNorms => size={encnorms_vec[0]}, elemSize={encnorms_vec[1]}, data_len={len(encnorms_vec[2])}")

        prims_vec       = read_vector("mesh.primitives")
        logger.debug(f"    mesh.primitives => size={prims_vec[0]}, elemSize={prims_vec[1]}, data_len={len(prims_vec[2])}")
        (prim_count, prim_elem_size, prim_data_buf) = prims_vec
        if prim_elem_size == 12:
            # parse TSDrawPrimitive => 3 i32
            primitive_list = []
            for i in range(prim_count):
                offset = i * 12
                chunk = prim_data_buf[offset : offset + 12]
                (start_i, numElems_i, matIndex_i) = struct.unpack("<3i", chunk)

                # decode matIndex bits
                TYPE_MASK     = 0xC0000000
                INDEXED       = 0x20000000
                NO_MATERIAL   = 0x10000000
                MATERIAL_MASK = 0x0FFFFFFF

                actual_mat_idx = matIndex_i & MATERIAL_MASK
                raw_type_bits  = matIndex_i & TYPE_MASK
                is_indexed     = bool(matIndex_i & INDEXED)
                has_no_mat     = bool(matIndex_i & NO_MATERIAL)

                if raw_type_bits == 0x00000000:
                    draw_type_str = "Triangles"
                elif raw_type_bits == 0x40000000:
                    draw_type_str = "Strip"
                elif raw_type_bits == 0x80000000:
                    draw_type_str = "Fan"
                else:
                    draw_type_str = f"Unknown(0x{raw_type_bits:X})"

                primitive_list.append({
                    "start":       start_i,
                    "numElements": numElems_i,
                    "matIndex":    matIndex_i,
                    "decoded": {
                        "materialIndex": actual_mat_idx,
                        "type":          draw_type_str,
                        "isIndexed":     is_indexed,
                        "noMaterial":    has_no_mat
                    }
                })

            for i, p in enumerate(primitive_list[:10]):
                base_info = f"     Primitive[{i}] => start={p['start']} numElem={p['numElements']}"
                decode_info= p["decoded"]
                logger.debug(f"{base_info}, matIndex={p['matIndex']} => "
                             f"{{ materialIndex={decode_info['materialIndex']}, "
                             f"type={decode_info['type']}, isIndexed={decode_info['isIndexed']}, "
                             f"noMaterial={decode_info['noMaterial']} }}")
            if len(primitive_list) > 10:
                logger.debug(f"     (Truncated {len(primitive_list) - 10} more primitives...)")

        indices_vec = read_vector("mesh.indices")
        logger.debug(f"    mesh.indices => size={indices_vec[0]}, elemSize={indices_vec[1]}, data_len={len(indices_vec[2])}")

        tangents_vec = read_vector("mesh.tangents")
        logger.debug(f"    mesh.tangents => size={tangents_vec[0]}, elemSize={tangents_vec[1]}, data_len={len(tangents_vec[2])}")

        mesh_vertsPerFrame = read_int("mesh.vertsPerFrame")
        logger.debug(f"    mesh.vertsPerFrame={mesh_vertsPerFrame}")

        mesh_flags = read_uint32("mesh.flags")
        decoded_flags = decode_mesh_flags(mesh_flags)
        logger.debug(f"    mesh.flags=0x{mesh_flags:X} => {decoded_flags}")

        mesh_data.update({
            "vertsPerFrame": mesh_vertsPerFrame,
            "flags": mesh_flags,
            "decoded_flags": decoded_flags
        })

        # now the animation things...
        if mesh_type_val == 1:  # SkinMeshType (1)
            logger.info(f"    Reading SkinMesh specific data")

            # Check if this is a parent mesh
            is_parent_mesh = mesh_parentMesh < 0

            if is_parent_mesh:
                initial_verts_vec = read_vector("skinMesh.initialVerts")
                logger.debug(f"    skinMesh.initialVerts => size={initial_verts_vec[0]}, elemSize={initial_verts_vec[1]}")

                initial_norms_vec = read_vector("skinMesh.initialNorms")
                logger.debug(f"    skinMesh.initialNorms => size={initial_norms_vec[0]}, elemSize={initial_norms_vec[1]}")

            initial_transforms_vec = read_vector("skinMesh.initialTransforms")
            logger.debug(f"    skinMesh.initialTransforms => size={initial_transforms_vec[0]}, elemSize={initial_transforms_vec[1]}")

            if is_parent_mesh:
                vertex_index_vec = read_vector("skinMesh.vertexIndex")
                logger.debug(f"    skinMesh.vertexIndex => size={vertex_index_vec[0]}, elemSize={vertex_index_vec[1]}")

                bone_index_vec = read_vector("skinMesh.boneIndex")
                logger.debug(f"    skinMesh.boneIndex => size={bone_index_vec[0]}, elemSize={bone_index_vec[1]}")

                weight_vec = read_vector("skinMesh.weight")
                logger.debug(f"    skinMesh.weight => size={weight_vec[0]}, elemSize={weight_vec[1]}")

                node_index_vec = read_vector("skinMesh.nodeIndex")
                logger.debug(f"    skinMesh.nodeIndex => size={node_index_vec[0]}, elemSize={node_index_vec[1]}")

            mesh_data.update({
                "isSkinMesh": True,
                "isParentMesh": is_parent_mesh
            })

        if generate_html:
            html_data["shape_info"]["meshes"].append(mesh_data)

    # g) sequences
    tot_sequences = read_uint32("sequences.size")
    sequences_info = []
    for seq_i in range(tot_sequences):
        seq_nameIndex      = read_int("seq.nameIndex")
        seq_flags          = read_int("seq.flags")
        seq_numKeyframes   = read_int("seq.numKeyframes")
        seq_duration       = read_float("seq.duration")
        seq_priority       = read_int("seq.priority")
        seq_firstGroundFrame = read_int("seq.firstGroundFrame")
        seq_numGroundFrames  = read_int("seq.numGroundFrames")
        seq_baseRotation     = read_int("seq.baseRotation")
        seq_baseTranslation  = read_int("seq.baseTranslation")
        seq_baseScale        = read_int("seq.baseScale")
        seq_baseObjectState  = read_int("seq.baseObjectState")
        seq_baseDecalState   = read_int("seq.baseDecalState")
        seq_firstTrigger     = read_int("seq.firstTrigger")
        seq_numTriggers      = read_int("seq.numTriggers")
        seq_toolBegin        = read_float("seq.toolBegin")

        # reading TSIntegerSets
        seq_rotationMatters    = decode_integerset()
        seq_translationMatters = decode_integerset()
        seq_scaleMatters       = decode_integerset()
        seq_visMatters         = decode_integerset()
        seq_frameMatters       = decode_integerset()
        seq_matFrameMatters    = decode_integerset()

        seq_data = {
            "nameIndex": seq_nameIndex,
            "flags": seq_flags,
            "numKeyframes": seq_numKeyframes,
            "duration": seq_duration,
            "priority": seq_priority,
            "firstGroundFrame": seq_firstGroundFrame,
            "numGroundFrames": seq_numGroundFrames,
            "baseRotation": seq_baseRotation,
            "baseTranslation": seq_baseTranslation,
            "baseScale": seq_baseScale,
            "baseObjectState": seq_baseObjectState,
            "baseDecalState": seq_baseDecalState,
            "firstTrigger": seq_firstTrigger,
            "numTriggers": seq_numTriggers,
            "toolBegin": seq_toolBegin
        }

        sequences_info.append(seq_data)
        if generate_html:
            html_data["shape_info"]["sequences"].append(seq_data)

    logger.info(f"[INFO] read {tot_sequences} sequences")

    # h) material list (if present)
    leftover = True
    mat_list_size = None
    try:
        mat_list_size = read_uint32("materialList.size")
    except:
        leftover = False

    if leftover and mat_list_size is not None:
        materials_data = []
        for mat_i in range(mat_list_size):
            mat_name = read_str("material.name")
            mat_flags = read_uint32("material.flags")
            mat_reflect = read_uint32("material.reflectMap")
            mat_bump    = read_uint32("material.bumpMap")
            mat_detail  = read_uint32("material.detailMap")
            mat_detailScale = read_float("material.detailScale")
            mat_reflectionAmt = read_float("material.reflectionAmount")

            decoded_flags = decode_material_flags(mat_flags)

            mat_data = {
                "name": mat_name,
                "flags": mat_flags,
                "decoded_flags": decoded_flags,
                "reflectMap": mat_reflect,
                "bumpMap": mat_bump,
                "detailMap": mat_detail,
                "detailScale": mat_detailScale,
                "reflectionAmount": mat_reflectionAmt
            }

            materials_data.append(mat_data)
            if generate_html:
                html_data["shape_info"]["materials"].append(mat_data)

        logger.info(f"[INFO] read {mat_list_size} materials from materialList.")
    else:
        logger.info("[INFO] No material list found or no leftover data to parse for materials.")

    logger.info("[DONE] Successfully read shape data in the order of _write_v31.")

    # Generate HTML report if requested
    if generate_html:
        generate_html_report(html_data, filepath + ".report.html", all_names)

def generate_html_report(data, output_path, names):
    """
    Generate an HTML report from the parsed shape data

    Args:
        data: Dictionary containing all the parsed shape data
        output_path: Path to save the HTML report
        names: List of all names in the shape
    """
    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Shape Report: {data['filepath']}</title>
    <style>
        body {{
            font-family: Arial, sans-serif;
            line-height: 1.6;
            color: #333;
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
        }}
        h1, h2, h3, h4 {{
            color: #2c3e50;
        }}
        .section {{
            margin-bottom: 30px;
            border: 1px solid #ddd;
            border-radius: 5px;
            padding: 15px;
            background-color: #f9f9f9;
        }}
        .subsection {{
            margin: 10px 0;
            padding: 10px;
            background-color: #fff;
            border-radius: 3px;
            box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }}
        table {{
            width: 100%;
            border-collapse: collapse;
            margin: 10px 0;
        }}
        th, td {{
            padding: 8px;
            text-align: left;
            border-bottom: 1px solid #ddd;
        }}
        th {{
            background-color: #f2f2f2;
        }}
        tr:hover {{
            background-color: #f5f5f5;
        }}
        .collapsible {{
            background-color: #eee;
            color: #444;
            cursor: pointer;
            padding: 10px;
            width: 100%;
            border: none;
            text-align: left;
            outline: none;
            font-size: 15px;
            border-radius: 3px;
        }}
        .active, .collapsible:hover {{
            background-color: #ddd;
        }}
        .content {{
            padding: 0 18px;
            display: none;
            overflow: hidden;
            background-color: #f1f1f1;
        }}
        .mesh-type-0 {{ color: #2980b9; }}
        .mesh-type-1 {{ color: #8e44ad; }}
        .mesh-type-2 {{ color: #c0392b; }}
        .mesh-type-3 {{ color: #27ae60; }}
        .mesh-type-4 {{ color: #7f8c8d; }}
    </style>
</head>
<body>
    <h1>Shape Report: {data['filepath']}</h1>

    <div class="section">
        <h2>File Information</h2>
        <p><strong>Version:</strong> {data.get('version', 'Unknown')}</p>
        <p><strong>Exporter Version:</strong> {data.get('exporter_version', 'Unknown')}</p>

        <h3>Header Information</h3>
        <div class="subsection">
            <table>
                <tr>
                    <th>Key</th>
                    <th>Value</th>
                </tr>
    """

    # Add header info
    for key, value in data.get('header_info', {}).items():
        if key == 'objectNames' and isinstance(value, list):
            html += f"""
                <tr>
                    <td>{key}</td>
                    <td>{len(value)} object names</td>
                </tr>
            """
        else:
            html += f"""
                <tr>
                    <td>{key}</td>
                    <td>{value}</td>
                </tr>
            """

    html += """
            </table>
        </div>
    </div>
    """

    # Shape basic info
    shape_info = data.get('shape_info', {})
    html += f"""
    <div class="section">
        <h2>Shape Information</h2>
        <div class="subsection">
            <p><strong>Smallest Visible Size:</strong> {shape_info.get('smallest_visible_size', 'N/A')}</p>
            <p><strong>Smallest Visible Detail Level:</strong> {shape_info.get('smallest_visible_dl', 'N/A')}</p>
            <p><strong>Radius:</strong> {shape_info.get('radius', 'N/A')}</p>
            <p><strong>Tube Radius:</strong> {shape_info.get('tube_radius', 'N/A')}</p>
            <p><strong>Center:</strong> {shape_info.get('center', 'N/A')}</p>

            <h3>Bounds</h3>
            <p><strong>Min:</strong> {shape_info.get('bounds', {}).get('min', 'N/A')}</p>
            <p><strong>Max:</strong> {shape_info.get('bounds', {}).get('max', 'N/A')}</p>
        </div>
    </div>
    """

    # Nodes
    nodes = shape_info.get('nodes', [])
    html += f"""
    <div class="section">
        <h2>Nodes ({len(nodes)})</h2>
        <button type="button" class="collapsible">Show/Hide Nodes</button>
        <div class="content">
            <table>
                <tr>
                    <th>Index</th>
                    <th>Name</th>
                    <th>Parent</th>
                    <th>First Object</th>
                    <th>First Child</th>
                    <th>Next Sibling</th>
                </tr>
    """

    for i, node in enumerate(nodes):
        name_idx = node.get('nameIndex', -1)
        name = names[name_idx] if 0 <= name_idx < len(names) else f"Unknown ({name_idx})"
        parent_idx = node.get('parentIndex', -1)
        parent_name = names[parent_idx] if 0 <= parent_idx < len(names) else "None"

        html += f"""
                <tr>
                    <td>{i}</td>
                    <td>{name}</td>
                    <td>{parent_name}</td>
                    <td>{node.get('firstObject', 'N/A')}</td>
                    <td>{node.get('firstChild', 'N/A')}</td>
                    <td>{node.get('nextSibling', 'N/A')}</td>
                </tr>
            """

    html += """
            </table>
        </div>
    </div>
    """

    # Objects
    objects = shape_info.get('objects', [])
    html += f"""
    <div class="section">
        <h2>Objects ({len(objects)})</h2>
        <button type="button" class="collapsible">Show/Hide Objects</button>
        <div class="content">
            <table>
                <tr>
                    <th>Index</th>
                    <th>Name</th>
                    <th>Node</th>
                    <th>Mesh Start</th>
                    <th>Num Meshes</th>
                    <th>Next Sibling</th>
                </tr>
    """

    for i, obj in enumerate(objects):
        name_idx = obj.get('nameIndex', -1)
        name = names[name_idx] if 0 <= name_idx < len(names) else f"Unknown ({name_idx})"
        node_idx = obj.get('nodeIndex', -1)
        node_name = names[node_idx] if 0 <= node_idx < len(names) else "None"

        html += f"""
                <tr>
                    <td>{i}</td>
                    <td>{name}</td>
                    <td>{node_name}</td>
                    <td>{obj.get('startMeshIndex', 'N/A')}</td>
                    <td>{obj.get('numMeshes', 'N/A')}</td>
                    <td>{obj.get('nextSibling', 'N/A')}</td>
                </tr>
            """

    html += """
            </table>
        </div>
    </div>
    """

    # Subshapes
    subshapes = shape_info.get('subshapes', {})
    if subshapes:
        html += f"""
        <div class="section">
            <h2>Subshapes</h2>
            <div class="subsection">
                <table>
                    <tr>
                        <th>Index</th>
                        <th>First Node</th>
                        <th>Num Nodes</th>
                        <th>First Object</th>
                        <th>Num Objects</th>
                    </tr>
        """

        first_nodes = subshapes.get('firstNode', [])
        first_objects = subshapes.get('firstObject', [])
        num_nodes = subshapes.get('numNodes', [])
        num_objects = subshapes.get('numObjects', [])

        for i in range(len(first_nodes)):
            html += f"""
                    <tr>
                        <td>{i}</td>
                        <td>{first_nodes[i] if i < len(first_nodes) else 'N/A'}</td>
                        <td>{num_nodes[i] if i < len(num_nodes) else 'N/A'}</td>
                        <td>{first_objects[i] if i < len(first_objects) else 'N/A'}</td>
                        <td>{num_objects[i] if i < len(num_objects) else 'N/A'}</td>
                    </tr>
                """

        html += """
                </table>
            </div>
        </div>
        """

    # Details
    details = shape_info.get('details', [])
    html += f"""
    <div class="section">
        <h2>Details ({len(details)})</h2>
        <button type="button" class="collapsible">Show/Hide Details</button>
        <div class="content">
            <table>
                <tr>
                    <th>Index</th>
                    <th>Name</th>
                    <th>Size</th>
                    <th>SubShape</th>
                    <th>Object Detail</th>
                    <th>Poly Count</th>
                    <th>Avg/Max Error</th>
                </tr>
    """

    for i, detail in enumerate(details):
        name_idx = detail.get('nameIndex', -1)
        name = names[name_idx] if 0 <= name_idx < len(names) else f"Unknown ({name_idx})"

        html += f"""
                <tr>
                    <td>{i}</td>
                    <td>{name}</td>
                    <td>{detail.get('size', 'N/A')}</td>
                    <td>{detail.get('subShapeNum', 'N/A')}</td>
                    <td>{detail.get('objectDetailNum', 'N/A')}</td>
                    <td>{detail.get('polyCount', 'N/A')}</td>
                    <td>{detail.get('averageError', 'N/A')} / {detail.get('maxError', 'N/A')}</td>
                </tr>
            """

    html += """
            </table>
        </div>
    </div>
    """

    # Meshes
    meshes = shape_info.get('meshes', [])
    html += f"""
    <div class="section">
        <h2>Meshes ({len(meshes)})</h2>
        <button type="button" class="collapsible">Show/Hide Meshes</button>
        <div class="content">
            <table>
                <tr>
                    <th>Index</th>
                    <th>Type</th>
                    <th>Verts</th>
                    <th>TVerts</th>
                    <th>Primitives</th>
                    <th>Flags</th>
                </tr>
    """

    for mesh in meshes:
        mesh_idx = mesh.get('index', 'N/A')
        mesh_type = mesh.get('type', 0)
        mesh_type_name = mesh.get('type_name', 'Unknown')

        html += f"""
                <tr>
                    <td>{mesh_idx}</td>
                    <td class="mesh-type-{mesh_type}">{mesh_type_name}</td>
                    <td>{mesh.get('vertCount', 'N/A')}</td>
                    <td>{mesh.get('tvertCount', 'N/A')}</td>
                    <td>{mesh.get('primitiveCount', 'N/A')}</td>
                    <td>{mesh.get('decoded_flags', 'N/A')}</td>
                </tr>
            """

    html += """
            </table>
        </div>
    </div>
    """

    # Sequences
    sequences = shape_info.get('sequences', [])
    html += f"""
    <div class="section">
        <h2>Sequences ({len(sequences)})</h2>
        <button type="button" class="collapsible">Show/Hide Sequences</button>
        <div class="content">
            <table>
                <tr>
                    <th>Index</th>
                    <th>Name</th>
                    <th>Keyframes</th>
                    <th>Duration</th>
                    <th>Priority</th>
                </tr>
    """

    for i, seq in enumerate(sequences):
        name_idx = seq.get('nameIndex', -1)
        name = names[name_idx] if 0 <= name_idx < len(names) else f"Unknown ({name_idx})"

        html += f"""
                <tr>
                    <td>{i}</td>
                    <td>{name}</td>
                    <td>{seq.get('numKeyframes', 'N/A')}</td>
                    <td>{seq.get('duration', 'N/A')}</td>
                    <td>{seq.get('priority', 'N/A')}</td>
                </tr>
            """

    html += """
            </table>
        </div>
    </div>
    """

    # Materials
    materials = shape_info.get('materials', [])
    html += f"""
    <div class="section">
        <h2>Materials ({len(materials)})</h2>
        <button type="button" class="collapsible">Show/Hide Materials</button>
        <div class="content">
            <table>
                <tr>
                    <th>Index</th>
                    <th>Name</th>
                    <th>Flags</th>
                    <th>Detail Map</th>
                    <th>Detail Scale</th>
                    <th>Reflection</th>
                </tr>
    """

    for i, mat in enumerate(materials):
        html += f"""
                <tr>
                    <td>{i}</td>
                    <td>{mat.get('name', 'N/A')}</td>
                    <td>{mat.get('decoded_flags', 'N/A')}</td>
                    <td>{mat.get('detailMap', 'N/A')}</td>
                    <td>{mat.get('detailScale', 'N/A')}</td>
                    <td>{mat.get('reflectionAmount', 'N/A')}</td>
                </tr>
            """

    html += """
            </table>
        </div>
    </div>
    """

    # Add JavaScript for collapsible sections
    html += """
    <script>
    var coll = document.getElementsByClassName("collapsible");
    var i;

    for (i = 0; i < coll.length; i++) {
        coll[i].addEventListener("click", function() {
            this.classList.toggle("active");
            var content = this.nextElementSibling;
            if (content.style.display === "block") {
                content.style.display = "none";
            } else {
                content.style.display = "block";
            }
        });
    }
    </script>
    </body>
    </html>
    """

    # Write the HTML to file
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(html)

    print(f"HTML report generated: {output_path}")

# --------------------------------------------------------------------------------
# Main script entry point
# --------------------------------------------------------------------------------

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="TSShape parser for v31 format")
    parser.add_argument("filepath", help="Path to the shape file")
    parser.add_argument("--no-html", action="store_true", help="Don't generate an HTML report")
    args = parser.parse_args()
    parse_v31_file(args.filepath, not args.no_html)
