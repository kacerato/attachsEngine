"""Offline camera-relative water grid contract, independent of any sample.

Returns axis positions and conservative incident-edge spacing in metres.
The central half has constant spacing; a C2 cubic transition extends the outer
half without separate patches or duplicated boundary vertices.
"""
import math


def graded_water_axis(segments, near_extent, far_extent):
    if (not isinstance(segments, int) or segments < 8 or segments % 4 or
            not math.isfinite(near_extent) or not math.isfinite(far_extent) or
            near_extent <= 0 or far_extent < near_extent):
        raise ValueError("water grid requires 4-aligned segments and finite ordered extents")
    axis = []
    for index in range(segments + 1):
        coordinate = 2.0 * index / segments - 1.0
        outer = max(0.0, (abs(coordinate) - .5) * 2.0)
        offset = near_extent * abs(coordinate) + (far_extent - near_extent) * outer ** 3
        axis.append(math.copysign(offset, coordinate))
    spacing = [max(axis[i] - axis[max(0, i - 1)],
                   axis[min(segments, i + 1)] - axis[i]) for i in range(segments + 1)]
    return axis, spacing
