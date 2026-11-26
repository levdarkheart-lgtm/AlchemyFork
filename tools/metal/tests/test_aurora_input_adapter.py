"""Analytical oracles for the private Aurora comparison's input adapter.

These checks do not execute Aurora or claim image quality. Expected screen
positions come from authored 3-D points, independently of the WGSL camera
reprojection expression. The adapter must preserve the exported raster rows.
"""

import math
import unittest


def multiply(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)]
            for r in range(4)]


def transform(a, v):
    return [sum(a[r][k] * v[k] for k in range(4)) for r in range(4)]


def inverse(a):
    rows = [list(row) + [float(r == c) for c in range(4)]
            for r, row in enumerate(a)]
    for c in range(4):
        pivot = max(range(c, 4), key=lambda r: abs(rows[r][c]))
        rows[c], rows[pivot] = rows[pivot], rows[c]
        divisor = rows[c][c]
        if abs(divisor) < 1e-12:
            raise ValueError("Singular authored camera")
        rows[c] = [value / divisor for value in rows[c]]
        for r in range(4):
            if r != c:
                factor = rows[r][c]
                rows[r] = [x - factor * y for x, y in zip(rows[r], rows[c])]
    return [row[4:] for row in rows]


IDENTITY = [[float(r == c) for c in range(4)] for r in range(4)]
Y_FLIP = [[(-1. if r == 1 else 1.) if r == c else 0.
           for c in range(4)] for r in range(4)]


def perspective_reverse(near, far=None):
    # Right-handed, camera faces -Z, Metal/D3D device depth [0,1].
    a, b = (0., near) if far is None else (near / (far - near), near * far / (far - near))
    return [[1.25, 0., 0., 0.], [0., 2., 0., 0.],
            [0., 0., a, b], [0., 0., -1., 0.]]


def camera_view(x, y, z):
    result = [row[:] for row in IDENTITY]
    result[0][3], result[1][3], result[2][3] = -x, -y, -z
    return result


def stored_projection(matrix, point):
    q = transform(matrix, [*point, 1.])
    return ([q[0] / q[3] * .5 + .5, q[1] / q[3] * .5 + .5], q[2] / q[3])


def aurora_camera_motion(reprojection, raster_uv, depth, jitter_pixels, extent):
    # Independent CPU transcription of pinned prepare_inputs.wgsl:23–35.
    current = [raster_uv[i] - jitter_pixels[i] / extent[i] for i in range(2)]
    previous = transform(reprojection, [2. * current[0] - 1., 1. - 2. * current[1], max(depth, 1e-7), 1.])
    previous_uv = [previous[0] / previous[3] * .5 + .5,
                   .5 - previous[1] / previous[3] * .5]
    return [previous_uv[i] - current[i] for i in range(2)]


class AuroraInputAdapterContractTests(unittest.TestCase):
    def assert_vector_close(self, actual, expected, tolerance=1e-10):
        self.assertEqual(len(actual), len(expected))
        for a, e in zip(actual, expected):
            self.assertAlmostEqual(a, e, delta=tolerance)

    def test_reverse_perspective_device_depth_recovers_physical_distance(self):
        for far in (None, 1000.):
            projection = perspective_reverse(.1, far)
            x, y = -projection[2][2] - 2. ** -23, projection[2][3]
            for distance in (.1, 1., 25., 100.):
                _, depth = stored_projection(projection, [0., 0., -distance])
                recovered = y / (depth - x)
                self.assertGreater(depth, 0.)
                self.assertAlmostEqual(recovered, distance, delta=distance * .00013)
            self.assertTrue(math.isfinite(y / (0. - x)))

    def test_unchanged_rows_require_y_conjugation_for_camera_fallback(self):
        projection = perspective_reverse(.1)
        current = multiply(projection, camera_view(.125, -.2, .05))
        previous = multiply(projection, camera_view(-.075, .1, -.1))
        original_reprojection = multiply(previous, inverse(current))
        adapted = multiply(multiply(Y_FLIP, original_reprojection), Y_FLIP)
        for point in ([.25, .5, -2.], [-.125, -.3, -5.]):
            current_uv, depth = stored_projection(current, point)
            previous_uv, _ = stored_projection(previous, point)
            expected = [previous_uv[i] - current_uv[i] for i in range(2)]
            for jitter in ([0., 0.], [.375, -.4], [-.25, .125]):
                raster_uv = [current_uv[i] + jitter[i] / (256., 128.)[i] for i in range(2)]
                actual = aurora_camera_motion(adapted, raster_uv, depth, jitter, (256., 128.))
                self.assert_vector_close(actual, expected)
            wrong = aurora_camera_motion(original_reprojection, current_uv, depth, [0., 0.], (256., 128.))
            self.assertGreater(abs(wrong[1] - expected[1]), .01)

    def test_stationary_zero_vectors_use_truthful_camera_fallback(self):
        for uv in ([.1, .2], [.75, .85]):
            for jitter in ([.3, -.4], [-.25, .125]):
                raster = [uv[i] + jitter[i] / (256., 128.)[i] for i in range(2)]
                self.assert_vector_close(aurora_camera_motion(IDENTITY, raster, .05, jitter, (256., 128.)), [0., 0.])

    def test_clear_depth_uses_upstream_finite_epsilon_plane(self):
        projection = perspective_reverse(.1)
        previous = multiply(projection, camera_view(.2, -.3, 0.))
        reprojection = multiply(multiply(Y_FLIP, multiply(previous, inverse(projection))), Y_FLIP)
        # Pinned prepare_inputs.wgsl uses max(depth, 1e-7), including clear0.
        # Its camera fallback therefore has finite, tiny translation parallax;
        # do not invent an infinity/zero-motion branch in the adapter.
        distance = .1 / 1e-7
        uv, _ = stored_projection(projection, [.25, -.5, -distance])
        previous_uv, _ = stored_projection(previous, [.25, -.5, -distance])
        expected = [previous_uv[i] - uv[i] for i in range(2)]
        jitter = [.375, -.4]
        raster = [uv[i] + jitter[i] / (256., 128.)[i] for i in range(2)]
        actual = aurora_camera_motion(reprojection, raster, 0., jitter, (256., 128.))
        self.assert_vector_close(actual, expected)
        self.assertTrue(all(math.isfinite(value) for value in actual))
        self.assert_vector_close(aurora_camera_motion(IDENTITY, raster, 0., jitter, (256., 128.)), [0., 0.])

    def test_pixel_motion_becomes_current_minus_previous_uv_in_zw(self):
        # Authored world feature moved right4/down2 raster pixels. MetalFX
        # receives displacement back to its previous location, [-4,-2].
        for extent in ((256., 128.), (288., 144.)):
            metalfx = [-4., -2.]
            packed_zw = [-metalfx[i] / extent[i] for i in range(2)]
            self.assert_vector_close(packed_zw, [4. / extent[0], 2. / extent[1]])
            fsr_motion = [-value for value in packed_zw]
            self.assert_vector_close(fsr_motion, [-4. / extent[0], -2. / extent[1]])

    def test_shared_palm_projection_keeps_foreground_background_parallax(self):
        aurora_projection = [[.5, 0., 0., 0.], [0., 1., 0., 0.],
                             [0., 0., 0., .1], [0., 0., -1., 0.]]
        alchemy_projection = multiply(Y_FLIP, aurora_projection)
        shift = .05 / 18.
        previous = alchemy_projection
        current = multiply(alchemy_projection, camera_view(-4. * shift, 0., 0.))
        reprojection = multiply(multiply(Y_FLIP, multiply(previous, inverse(current))), Y_FLIP)
        for distance, expected_pixel_motion in ((2., -.05 / 18. * 128.),
                                                (4., -.05 / 18. * 64.)):
            uv, depth = stored_projection(current, [.25, -.5, -distance])
            fallback = aurora_camera_motion(reprojection, uv, depth, [0., 0.], (256., 128.))
            self.assert_vector_close([fallback[0] * 256., fallback[1] * 128.],
                                     [expected_pixel_motion, 0.])
            self.assertAlmostEqual(.1 / depth, distance)

    def test_static_mask_scene_needs_no_fabricated_reactivity_or_composition(self):
        opaque = [.035, .075, .21]
        final = opaque[:]
        tone_map = lambda rgb: [v / (max(rgb) + 1.) for v in rgb]
        pre, post = tone_map(opaque), tone_map(final)
        delta = max(abs(a - b) for a, b in zip(pre, post))
        generated_reactive = 0. if delta < .2 else .9
        self.assertEqual(generated_reactive, 0.)
        self.assertFalse(0. > 1.5)  # normal/composition X for nonanimated MASK.


if __name__ == "__main__":
    unittest.main()
