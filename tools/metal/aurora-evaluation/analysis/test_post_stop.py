import unittest
import numpy as np
import post_stop as p

class PostStopTests(unittest.TestCase):
    def test_mature_jitter_pattern_is_removed_from_residual_change(self):
        a=np.zeros((4,4,4),np.float32);b=np.ones_like(a)*.4
        roi=(slice(None),slice(None))
        self.assertAlmostEqual(p.residual_change(b,a,b,a,roi),0.)
        self.assertGreater(float(np.abs(b[...,:3]-a[...,:3]).mean()),.3)

    def test_gradual_signal_recovery_is_not_counted_as_static_periodic_pattern(self):
        mature=np.ones((4,4,4),np.float32);a=mature*.5;b=mature*.75
        self.assertAlmostEqual(p.residual_change(b,a,mature,mature,(slice(None),slice(None))),.25)

    def test_matched_phase_cycle_check_rejects_unmatched_phases(self):
        a=np.zeros((4,4,4),np.float32)
        with self.assertRaisesRegex(ValueError,'matching jitter'):
            p.phase_pair(a,a,0,1,(slice(None),slice(None)))
        self.assertEqual(p.phase_pair(a,a,0,0,(slice(None),slice(None))),0.)

    def test_cycle_summary_does_not_pool_pixels_as_independent_trials(self):
        keys=('phase_matched_error','native_quality_error','old_history_fraction',
              'adjacent_rgb_change','adjacent_edge_change','phase_residual_change',
              'green_excess','contrast_over_actual_input','edge_energy')
        a={key:1. for key in keys};b={key:3. for key in keys}
        self.assertEqual(p.summary([a,b]),{key:2. for key in keys})

if __name__=='__main__':unittest.main()
