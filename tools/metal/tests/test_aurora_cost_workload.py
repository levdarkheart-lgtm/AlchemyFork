"""Small analytical checks of the shared target-size cost producer (no GPU)."""
import ctypes
import math
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / 'tools/metal/aurora-evaluation/workload/input_workload.c'


@unittest.skipUnless(platform.system() == 'Darwin' and shutil.which('clang'),
                     'Shared producer uses the pinned Apple toolchain')
class TargetCostWorkloadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.library = Path(cls.tmp.name) / 'workload.dylib'
        subprocess.run(['clang', '-std=c11', '-O3', '-ffp-contract=off', '-dynamiclib',
                        str(SOURCE), '-o', str(cls.library)], check=True, capture_output=True)
        cls.lib = ctypes.CDLL(str(cls.library))
        cls.lib.ae_workload_sample.argtypes = [ctypes.c_uint32, ctypes.c_uint32,
            ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint16),
            ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_uint16)]
        cls.lib.ae_workload_jitter.argtypes = [ctypes.c_uint32, ctypes.POINTER(ctypes.c_float)]
        cls.lib.ae_workload_projection.argtypes = [ctypes.POINTER(ctypes.c_float)]

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def sample(self, phase, x, y):
        color=(ctypes.c_uint16*4)(); depth=ctypes.c_float(); motion=(ctypes.c_uint16*2)()
        self.assertEqual(self.lib.ae_workload_sample(phase,x,y,color,ctypes.byref(depth),motion),1)
        return list(color), depth.value, list(motion)

    def test_production_phase_units_and_period(self):
        def halton(n, base):
            out=0.; f=1.
            while n:
                f/=base; out+=f*(n%base); n//=base
            return out-.5
        for p in range(18):
            j=(ctypes.c_float*2)(); self.lib.ae_workload_jitter(p,j)
            for value, base in zip(j,(2,3)):
                self.assertAlmostEqual(value,halton(p+1,base),delta=8e-8)
            repeated=(ctypes.c_float*2)();self.lib.ae_workload_jitter(p+18,repeated)
            self.assertEqual(list(j),list(repeated))

    def test_actual_perspective_reverse_depth(self):
        p=(ctypes.c_float*16)();self.lib.ae_workload_projection(p)
        self.assertEqual(p[11],-1.);self.assertEqual(p[15],0.)
        self.assertAlmostEqual(p[0],2168/3456,delta=1e-7)
        self.assertEqual(p[5],1.)
        for distance in (2.,4.,12.):
            self.assertAlmostEqual(p[14]/distance,.1/distance,delta=1e-8)
        for phase in range(18):
            for y in range(17,1446,137):
                for x in range(13,2304,173):
                    color,depth,motion=self.sample(phase,x,y)
                    self.assertTrue(any(abs(depth-v)<1e-8 for v in (0.,.05,.025,.1/12)))
                    self.assertEqual(motion,[0,0])
                    self.assertTrue(all((v&0x7c00)!=0x7c00 for v in color))

    def test_nontrivial_scene_coverage_and_invalid_phase(self):
        depths=set();colors=set()
        for y in range(7,1446,41):
            for x in range(7,2304,43):
                c,d,_=self.sample(0,x,y);depths.add(round(d,6));colors.add(tuple(c))
        self.assertEqual(depths,{0.,.05,.025,round(.1/12,6)})
        self.assertTrue(any(color[0] > 0x3c00 for color in colors), "HDR radiance above1 exists")
        self.assertTrue(any(color[3] > 0 for color in colors), "Current glow alpha exists")
        color=(ctypes.c_uint16*4)();depth=ctypes.c_float();motion=(ctypes.c_uint16*2)()
        self.assertEqual(self.lib.ae_workload_sample(18,0,0,color,ctypes.byref(depth),motion),0)

if __name__ == '__main__':
    unittest.main()
