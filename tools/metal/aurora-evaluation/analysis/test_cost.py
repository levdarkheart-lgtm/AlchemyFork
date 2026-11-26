import math
import tempfile
from pathlib import Path
import json
import unittest
import cost

class CostTests(unittest.TestCase):
    def test_common_midpoint_median_and_nearest_rank_p95(self):
        result=cost.stats(range(360))
        self.assertEqual(result['median_ms'],179.5)
        self.assertEqual(result['p95_ms'],341)
        with self.assertRaises(ValueError):cost.stats([math.nan])

    def test_overlapping_stages_use_envelope_not_additive_sum(self):
        sample={'adapter_start_tick':0,'adapter_end_tick':4_000_000,'first_fsr_begin_tick':2_000_000,
                'color_end_tick':8_000_000,'display_depth_end_tick':7_000_000}
        self.assertEqual(cost.wgpu_envelope(sample,1.),8.)
        self.assertNotEqual(cost.wgpu_envelope(sample,1.),4.+6.)

    def test_duration_preserves_recorded_counter_period(self):
        sample={'adapter_start_tick':100,'first_fsr_begin_tick':150,'color_end_tick':600,'display_depth_end_tick':550}
        self.assertEqual(cost.wgpu_envelope(sample,2.),.001)

    def test_bad_clock_and_empty_samples_fail_closed(self):
        sample={'adapter_start_tick':10,'first_fsr_begin_tick':20,'color_end_tick':9,'display_depth_end_tick':8}
        with self.assertRaises(ValueError):cost.wgpu_envelope(sample,1.)
        valid={'adapter_start_tick':0,'first_fsr_begin_tick':2,'color_end_tick':9,'display_depth_end_tick':8}
        with self.assertRaises(ValueError):cost.wgpu_envelope(valid,math.nan)
        with self.assertRaises(ValueError):cost.wgpu_envelope({**valid,'color_end_tick':math.nan},1.)
        with self.assertRaises(ValueError):cost.stats([])

    def test_validation_or_wrong_dimensions_reject_measurement(self):
        samples=[{'sample':i,'phase':i%18,'gpu_start_seconds':i,'gpu_end_seconds':i+.001,'gpu_total_ms':1.} for i in range(360)]
        v={'device':{'name':'Apple M1 Max'},'measured_worlds':360,'samples':samples,'dimensions':{'input_width':2304,'input_height':1446,'output_width':3456,'output_height':2168,'phases':18},
           'warmup_worlds':360,'phase_input_fingerprints_fnv64':['x']*18,'validation_run':True,'environment':{},
           'quiescent_component_mode':True,'maximum_pending_worlds':1}
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'arm.json';path.write_text(json.dumps(v))
            with self.assertRaisesRegex(ValueError,'validation'):cost.load_arm(path,'native',True)
            v['validation_run']=False;v['dimensions']['output_width']=384;path.write_text(json.dumps(v))
            with self.assertRaisesRegex(ValueError,'dimensions'):cost.load_arm(path,'native',True)

if __name__=='__main__':unittest.main()
