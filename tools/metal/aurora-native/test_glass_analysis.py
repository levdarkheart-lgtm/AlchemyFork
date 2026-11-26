import json
from pathlib import Path
import tempfile
import unittest
import numpy as np
import glass_analysis as g


class GlassAnalysisTests(unittest.TestCase):
    def image(self):return np.zeros((g.H,g.W,4),dtype='<f4')

    def test_output_roi_does_not_accept_input_stride(self):
        small=np.zeros((g.IH,g.IW,4),dtype='<f4')
        for call in (lambda:g.rgb_change(small,small),lambda:g.detail(small),
                     lambda:g.rms_change(small,small),lambda:g.gradient_change(small,small)):
            with self.assertRaises(ValueError):call()

    def test_rgb_and_rms_regions_are_separate_from_uncovered_control(self):
        a=self.image();b=self.image();b[g.ROI][...,:3]=.25
        self.assertEqual(g.rgb_change(a,b),.25);self.assertEqual(g.rms_change(a,b),.25)
        self.assertEqual(g.rgb_change(a,b,g.CONTROL),0)

    def test_detail_is_not_uniform_brightness_or_sharpness_assumption(self):
        a=self.image();b=self.image();b[...,:3]=.5
        self.assertEqual(g.detail(a),0);self.assertEqual(g.detail(b),0)
        b[:,::2,:3]=1
        self.assertEqual(g.detail(b),.5);self.assertEqual(g.gradient_change(a,b),.5)

    def test_file_dimension_validation_distinguishes_input_and_output(self):
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'input';np.zeros((g.IH,g.IW,4),dtype='<f4').tofile(p)
            self.assertEqual(g.image(p,g.IW,g.IH).shape,(g.IH,g.IW,4))
            with self.assertRaises(ValueError):g.image(p)

    def test_sequence_counts_and_phase_protocol_are_required(self):
        with tempfile.TemporaryDirectory() as t:
            directory=Path(t)/'arm';directory.mkdir()
            for i in range(g.COUNT):(directory/f'world-{i:03d}-output.rgba32f').touch()
            m=directory.parent/'arm-metrics.json';protocol={'warm':g.WARM,'stationary':g.STABLE,'moving':g.MOVING,'stopped':g.STOPPED,'phase_count':g.PHASES};m.write_text(json.dumps(protocol))
            g.validate_sequence(directory)
            protocol['phase_count']=17;m.write_text(json.dumps(protocol))
            with self.assertRaises(ValueError):g.validate_sequence(directory)
            protocol['phase_count']=g.PHASES;m.write_text(json.dumps(protocol));(directory/'world-003-output.rgba32f').unlink()
            with self.assertRaises(ValueError):g.validate_sequence(directory)

    def test_zero_denominator_is_explicit_not_a_claimed_improvement(self):
        self.assertIsNone(g.relative_change(0,0));self.assertIsNone(g.relative_change(0,1))
        self.assertEqual(g.relative_change(1,.5),-50)
        for x in (-1,float('nan')):
            with self.assertRaises(ValueError):g.relative_change(x,1)


if __name__=='__main__':unittest.main()
