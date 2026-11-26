import csv
from pathlib import Path
import tempfile
import unittest
import numpy as np
import quality as q

class QualityMathTests(unittest.TestCase):
    def test_original_frame_percentile_uses_nearest_rank(self):
        self.assertEqual(q.p95(list(range(20))),18)
        self.assertEqual(q.p95([7]),7)

    def test_fractional_color_coverage_not_binary_leaf_count(self):
        fg=np.broadcast_to(np.array([.1,.8,.2,1],np.float32),(4,4,4)).copy()
        bg=np.array([.03,.07,.21,1],np.float32)
        alpha=np.tile(np.array([0,.25,.5,1],np.float32),(4,1))
        pixels=bg+(fg-bg)*alpha[...,None]
        np.testing.assert_allclose(q.coverage(pixels,fg,bg),alpha,atol=1e-7,rtol=0)

    def test_coverage_area_detects_lost_contrast(self):
        truth=np.full((8,8),.5,np.float32);actual=np.full((8,8),.25,np.float32)
        m=q.mask_quality(truth,actual,(slice(1,7),slice(1,7)))
        self.assertEqual(m,{'recall':.5,'precision':1.,'area':.5})

    def test_edge_band_is_fixed_from_native_reference(self):
        x=np.zeros((12,12,4),np.float32);x[:,:,:3]=[0,0,1];x[:,6:,:3]=[0,1,0]
        band=q.edge_mask(x,(slice(2,10),slice(2,10)))
        expected=np.tile([False,False,True,True,True,True,False,False],(8,1))
        np.testing.assert_array_equal(band,expected)

    def test_known_old_history_fraction_and_phase_error(self):
        old=np.zeros((8,16,4),np.float32);old[:,:,:3]=[0,0,1];old[2:6,3:6,:3]=[0,1,0]
        final=np.zeros_like(old);final[:,:,:3]=[0,0,1];final[2:6,10:13,:3]=[0,1,0]
        mixed=.25*old+.75*final
        m=q.stop_quality(old,final,old,final,mixed,(slice(1,7),slice(1,15)))
        self.assertEqual(m['disoccluded_valid_pixels'],12)
        self.assertAlmostEqual(m['old_history_fraction'],.25,places=7)
        self.assertAlmostEqual(m['phase_matched_error'],1/6,places=7)
        self.assertEqual(m['localized_trail_review_only']['fraction_over_point1'],1.)

    def test_unclamped_overshoot_and_distant_halo_are_observed(self):
        fg=np.zeros((12,12,4),np.float32);fg[:,:,:3]=[0,1,0]
        bg=np.array([0,0,1,1],np.float32);out=np.broadcast_to(bg,fg.shape).copy();truth=np.zeros((12,12),np.float32)
        truth[6,6]=1;out[6,6,:3]=[0,1.1,0];out[1,1,:3]=[0,.05,.95]
        m=q.artifacts(out,fg,bg,truth,(slice(0,12),slice(0,12)))
        self.assertTrue(m['review_only'])
        self.assertAlmostEqual(m['peak_channel_overshoot'],.1,places=6)
        self.assertAlmostEqual(m['false_coverage_positive_peak'],.05,places=6)
        self.assertGreater(m['halo_fraction_background_residual_over_2_255'],0)

    def test_float_reader_keeps_negative_values_and_rejects_nonfinite(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'output.rgba32f';x=np.array([-.2,1.1,.5,1],dtype='<f4');p.write_bytes(x.tobytes())
            np.testing.assert_array_equal(q.image(p,1,1).ravel(),x)
            self.assertEqual(q.identity(p)['name'],'output.rgba32f')
            p.write_bytes(np.array([np.nan,0,0,1],dtype='<f4').tobytes())
            with self.assertRaisesRegex(ValueError,'nonfinite'):q.image(p,1,1)
            p.write_bytes(b'1234')
            with self.assertRaisesRegex(ValueError,'scalar count'):q.image(p,1,1)

    def test_vendor_alpha_is_observational_not_an_RGB_quality_requirement(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'output.rgba32f';p.write_bytes(np.array([.1,.2,.3,np.nan],dtype='<f4').tobytes())
            result=q.image(p,1,1)
            self.assertTrue(np.isfinite(result[...,:3]).all())
            self.assertTrue(np.isnan(result[0,0,3]))

    def test_reference_uses_last_complete_phase_cycle(self):
        short=[{'phase':str(i%18)} for i in range(54)]
        long=[{'phase':str(i%18)} for i in range(414)]
        self.assertEqual(q.reference_index(short,18,17),53)
        self.assertEqual(q.reference_index(long,18,17),413)
        with self.assertRaisesRegex(ValueError,'phase'):q.reference_index(long[:-1],18,17)

    def test_missing_frames_and_unexpected_resets_are_not_qualified(self):
        with tempfile.TemporaryDirectory() as td:
            p=Path(td)/'sequence.csv'
            p.write_text('frame,reset\n0,1\n2,0\n')
            with self.assertRaisesRegex(ValueError,'noncontiguous'):q.csv_rows(p)
            p.write_text('frame,reset\n0,1\n1,1\n')
            with self.assertRaisesRegex(ValueError,'reset'):q.csv_rows(p)
            p.write_text('frame,reset\n0,1\n1,0\n')
            self.assertEqual(len(q.csv_rows(p)),2)

if __name__=='__main__':unittest.main()
