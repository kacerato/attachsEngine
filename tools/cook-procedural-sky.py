"""Cook the global procedural sky/IBL environment (AETX + AEEN v2).

This replaces a baked photographic HDRI with a single analytic sky/ground
radiance function, shared between three consumers that must stay coherent:

  1. The visible sky dome (`native/rhi/shaders/dirt_road_sky.frag`) — adds
     clouds and the sun disc on top of the same gradient for display only.
  2. The specular reflection source baked here into `environment.aetex`
     (small mip chain; clouds/sun intentionally excluded, see below).
  3. The diffuse ambient term baked here as 9 SH irradiance coefficients into
     `environment.aeenv`, consumed by `evaluateSkyIrradiance()` in
     `environment_lighting.glsl`.

Root cause this replaces: the previous environment.aetex/.aeenv were cooked
from a real "sunset forest" photograph (tools/cook-hdri-environment.py) that
has nothing to do with the bright procedural blue sky the engine actually
renders. Reflections and ambient light came from a dim, warm, mismatched
source while the visible sky was a bright, saturated blue gradient — this
produced a dark forest interior, desaturated/"washed" color and bright,
disconnected specular blobs at grazing angles (the whitish tree-canopy
backs/edges the forest sample was showing). Baking IBL from the SAME
function the visible sky uses removes the mismatch by construction, for any
scene that loads this environment format — this is a global engine asset
format, not a per-scene hack.

Sun disc/glow is intentionally excluded from both bakes: the sun is already
an explicit analytic directional light (`sunDirectionIntensity`/
`sunColorAngularRadius` in `EnvironmentLightingBlock`, applied via
`directLight()`). Baking it into the IBL too would double-count sun energy
in both diffuse (SH) and specular (mirror-like reflections) terms.

This is a fully offline, network-free, dependency-free (besides numpy)
cooker: no external asset, no license/attribution to track.
"""
import argparse
import hashlib
import json
import math
import pathlib
import struct

import numpy as np

WIDTH, HEIGHT = 128, 64  # specular env mip-chain base resolution (RGBA16F)
SH_GRID_WIDTH, SH_GRID_HEIGHT = 256, 128  # integration grid, finer than the baked texture

HORIZON_BLUE = np.array([.53, .76, .98], dtype=np.float64)
ZENITH_BLUE = np.array([.075, .28, .68], dtype=np.float64)
GROUND_COLOR = np.array([.16, .14, .11], dtype=np.float64)


def smoothstep(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def sky_radiance(direction):
    """Linear, pre-exposure, pre-tonemap sky+ground radiance for a batch of
    unit directions (..., 3). Matches the gradient in dirt_road_sky.frag
    exactly for direction.y >= 0 (the part that is ever directly visible);
    clouds and the sun disc are display-only additions, not part of this
    shared radiance function. Below the horizon this fades to a muted ground
    bounce color instead of repeating sky blue, so undersides of geometry
    (trunks, the underside of foliage cards) do not pick up an unrealistic
    bright-sky reflection/irradiance from "below".
    """
    y = direction[..., 1]
    horizon = smoothstep(-.12, .62, y)
    sky = horizon[..., None] * ZENITH_BLUE + (1.0 - horizon[..., None]) * HORIZON_BLUE
    below = smoothstep(0.0, 0.3, -y)
    return below[..., None] * GROUND_COLOR + (1.0 - below[..., None]) * sky


def direction_from_pixel(x, y, width, height):
    """Inverse of environmentUv() in environment_lighting.glsl (rotation=0
    baked at cook time; the runtime rotation uniform still applies on top).
    """
    phi = ((x + .5) / width - .5) * 2 * math.pi
    theta = ((y + .5) / height) * math.pi
    return np.array([math.sin(theta) * math.cos(phi), math.cos(theta),
                     math.sin(theta) * math.sin(phi)], dtype=np.float64)


def direction_grid(width, height):
    xs = (np.arange(width, dtype=np.float64) + .5) / width - .5
    ys = (np.arange(height, dtype=np.float64) + .5) / height
    phi = xs * 2 * math.pi
    theta = ys * math.pi
    sin_t, cos_t = np.sin(theta), np.cos(theta)
    cos_p, sin_p = np.cos(phi), np.sin(phi)
    dx = sin_t[:, None] * cos_p[None, :]
    dy = np.repeat(cos_t[:, None], width, axis=1)
    dz = sin_t[:, None] * sin_p[None, :]
    return np.stack([dx, dy, dz], axis=-1)  # (height, width, 3)


def sh_basis(direction):
    """9 real SH basis functions (bands l=0,1,2), self-consistent pole == +Y.
    Order: [00, 1(y), 1(z), 1(x), 2(xy), 2(yz), 2(3y^2-1), 2(xz), 2(x^2-z^2)].
    """
    x, y, z = direction[..., 0], direction[..., 1], direction[..., 2]
    return np.stack([
        np.full_like(x, 0.282095),
        0.488603 * y,
        0.488603 * z,
        0.488603 * x,
        1.092548 * x * y,
        1.092548 * y * z,
        0.315392 * (3.0 * y * y - 1.0),
        1.092548 * x * z,
        0.546274 * (x * x - z * z),
    ], axis=-1)  # (..., 9)


# Cosine-lobe convolution factors (Ramamoorthi & Hanrahan), one per band l.
COSINE_LOBE_A = [math.pi, 2.0 * math.pi / 3.0, math.pi / 4.0]
BAND_OF_COEFFICIENT = [0, 1, 1, 1, 2, 2, 2, 2, 2]


def project_irradiance_sh(radiance_fn, width, height):
    """Numerically integrate radiance_fn over the sphere against the 9 SH
    basis functions (raw radiance coefficients), then apply the cosine-lobe
    convolution so the result can be evaluated directly as irradiance:
    E(n) = sum(coefficients[i] * Y_i(n)).
    """
    directions = direction_grid(width, height)
    theta = (np.arange(height, dtype=np.float64) + .5) / height * math.pi
    d_theta = math.pi / height
    d_phi = 2.0 * math.pi / width
    solid_angle = np.broadcast_to((np.sin(theta) * d_theta * d_phi)[:, None], (height, width))

    radiance = radiance_fn(directions)  # (height, width, 3)
    basis = sh_basis(directions)  # (height, width, 9)

    # radiance_lm[i, channel] = sum over sphere of radiance*basis_i*dOmega
    radiance_lm = np.einsum('hwc,hwi,hw->ic', radiance, basis, solid_angle)
    total_solid_angle = float(np.sum(solid_angle))
    assert abs(total_solid_angle - 4.0 * math.pi) < 1e-2, total_solid_angle

    lobe = np.array([COSINE_LOBE_A[BAND_OF_COEFFICIENT[i]] for i in range(9)], dtype=np.float64)
    return radiance_lm * lobe[:, None]  # (9, 3) irradiance coefficients


def self_test():
    """Uniform unit-radiance sky must integrate to a flat pi irradiance in
    every direction (textbook Lambertian hemisphere result) and every band
    above l=0 must vanish by orthogonality. Catches a broken quadrature or a
    wrong cosine-lobe factor before it ships into the renderer.
    """
    coefficients = project_irradiance_sh(lambda d: np.ones(d.shape[:-1] + (3,)), 64, 32)
    assert np.allclose(coefficients[1:], 0.0, atol=5e-3), coefficients
    sample_dirs = direction_grid(17, 9).reshape(-1, 3)
    basis = sh_basis(sample_dirs)
    irradiance = basis @ coefficients  # (N, 3)
    assert np.allclose(irradiance, math.pi, atol=5e-2), (irradiance.min(), irradiance.max())


def resize_linear(data):
    height, width, channels = data.shape
    if height == 1 and width == 1:
        return data
    if height == 1:
        return data.reshape(1, width // 2, 2, channels).mean(axis=2)
    if width == 1:
        return data.reshape(height // 2, 2, 1, channels).mean(axis=1)
    return data.reshape(height // 2, 2, width // 2, 2, channels).mean(axis=(1, 3))


def write_texture(path, levels, width, height):
    payload = b"".join(levels)
    path.write_bytes(struct.pack("<6IQ", 0x58544541, 1, width, height, 5,
                                 len(levels), len(payload)) + payload)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=pathlib.Path,
                        default=pathlib.Path("samples/dirt-road/Imported"))
    parser.add_argument("--sun-elevation-deg", type=float, default=58.0)
    parser.add_argument("--sun-azimuth-deg", type=float, default=40.0)
    parser.add_argument("--sun-color", type=float, nargs=3, default=(1.0, .97, .90))
    parser.add_argument("--sun-intensity", type=float, default=2.6)
    parser.add_argument("--sun-angular-radius-deg", type=float, default=.27)
    parser.add_argument("--ambient-strength", type=float, default=1.0)
    parser.add_argument("--exposure", type=float, default=1.0)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    self_test()

    elevation = math.radians(args.sun_elevation_deg)
    azimuth = math.radians(args.sun_azimuth_deg)
    sun_direction = np.array([math.cos(elevation) * math.cos(azimuth), math.sin(elevation),
                              math.cos(elevation) * math.sin(azimuth)], dtype=np.float64)
    sun_direction /= np.linalg.norm(sun_direction)

    # 1) Specular/reflection mip chain: same radiance function as the visible
    #    dome, sun/clouds excluded (see module docstring).
    directions = direction_grid(WIDTH, HEIGHT)
    base = sky_radiance(directions).astype(np.float32)
    levels = []
    mip = base
    while True:
        alpha = np.ones((*mip.shape[:2], 1), dtype=np.float32)
        levels.append(np.concatenate([mip, alpha], axis=-1).astype("<f2").tobytes())
        if mip.shape[0] == 1 and mip.shape[1] == 1:
            break
        mip = resize_linear(mip)
    environment_tex = args.out / "environment.aetex"
    write_texture(environment_tex, levels, WIDTH, HEIGHT)

    # 2) Diffuse irradiance: 9 SH coefficients projected from the same
    #    radiance function at higher quadrature resolution.
    sh = project_irradiance_sh(sky_radiance, SH_GRID_WIDTH, SH_GRID_HEIGHT)  # (9, 3)
    assert np.isfinite(sh).all()

    metadata = struct.pack(
        "<4I16f36f", 0x4E454541, 2, 224, 0,
        *sun_direction, args.sun_intensity,
        *args.sun_color, math.radians(args.sun_angular_radius_deg),
        1.0, 1.0, 1.0, args.ambient_strength,
        args.exposure, 0.0, float(len(levels) - 1), 0.0,
        *[value for band in sh for value in (*band, 0.0)])
    environment_env = args.out / "environment.aeenv"
    environment_env.write_bytes(metadata)

    manifest_path = args.out.parent / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf8"))
    manifest["environment"] = {
        "asset": "procedural_sky_v1",
        "description": "Analytic sky/ground gradient, no external source; "
                        "shared by the visible sky dome, the specular env mip "
                        "chain and the SH9 diffuse irradiance below.",
        "resolution": [WIDTH, HEIGHT],
        "encoding": "RGBA16F linear, full mip chain, sun/clouds excluded",
        "format": "AEEN-2",
        "sunDirection": sun_direction.tolist(),
        "sunColor": list(args.sun_color),
        "sunIntensity": args.sun_intensity,
        "sunAngularRadiusDeg": args.sun_angular_radius_deg,
        "ambientStrength": args.ambient_strength,
        "exposure": args.exposure,
        "skyIrradianceSH": sh.tolist(),
    }
    manifest["outputs"] = {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(args.out.iterdir()) if path.suffix in (".aetex", ".aemap", ".aeenv")
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf8")
    print(json.dumps(manifest["environment"], indent=2))


if __name__ == "__main__":
    main()
