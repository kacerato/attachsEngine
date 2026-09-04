"""Generate a deterministic closed camera route for distance/rotation regression.

Uses the existing AERT v1 runtime contract; does not alter the scene or simulate
touch input. Frame ordinal (not wall time) is the A/B comparison coordinate.
"""
import argparse
import math
import pathlib
import struct


def fingerprint(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & 0xffffffffffffffff
    return value


def encode_sweep(poses, frames_per_segment, tick_rate, scene_fingerprint):
    count = len(poses) * frames_per_segment
    if len(poses) < 2 or frames_per_segment < 2 or count > 262144:
        raise ValueError('Need >=2 poses, >=2 frames/segment, <=262144 frames')
    if not 1 <= tick_rate <= 240:
        raise ValueError('Tick rate must be in [1,240]')
    if any(len(pose) != 5 or not all(math.isfinite(x) for x in pose) for pose in poses):
        raise ValueError('Each pose must contain finite x,y,z,yaw,pitch')
    result = bytearray(struct.pack('<4IQ2I', 0x54524541, 1, 32, 0,
                                   scene_fingerprint, tick_rate, count))
    for index, start in enumerate(poses):
        end = poses[(index + 1) % len(poses)]
        yaw_delta = math.atan2(math.sin(end[3] - start[3]), math.cos(end[3] - start[3]))
        for frame in range(frames_per_segment):
            t = frame / frames_per_segment
            t = t * t * (3 - 2 * t)
            sample = [start[i] + (end[i] - start[i]) * t for i in range(5)]
            sample[3] = start[3] + yaw_delta * t
            result.extend(struct.pack('<5f', *sample))
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=pathlib.Path, required=True)
    parser.add_argument('--pose', action='append', required=True,
                        help='x,y,z,yaw,pitch in world units and radians; use --pose=-1,...')
    parser.add_argument('--frames-per-segment', type=int, default=1200)
    parser.add_argument('--hz', type=int, default=120)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    package = args.package.read_bytes()
    if len(package) < 144 or struct.unpack_from('<I', package)[0] != 0x504d4541:
        parser.error('Not an AEMAP package')
    try:
        poses = [[float(part) for part in pose.split(',')] for pose in args.pose]
        route = encode_sweep(poses, args.frames_per_segment, args.hz, fingerprint(package))
    except (ValueError, OverflowError, struct.error) as error:
        parser.error(str(error))
    # QA evidence is immutable: an existing capture must never be overwritten.
    with args.output.open('xb') as stream:
        stream.write(route)
    print(f'{args.output}: {len(poses) * args.frames_per_segment} frames; closed loop')


if __name__ == '__main__':
    main()
