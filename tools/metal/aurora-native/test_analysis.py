#!/usr/bin/env python3
"""Small negative cases for the exported cost reducer, no Metal execution."""
import json
from pathlib import Path
import tempfile
import unittest
import analyze_native as a


class CostTests(unittest.TestCase):
    def test_distribution_uses_midpoint_median_and_nearest_rank_p95(self):
        d=a.distribution([4.,1.,2.,3.]);self.assertEqual(d['median_ms'],2.5);self.assertEqual(d['p95_ms'],4.)

    def test_invalid_distribution_rejected(self):
        for v in ([],[float('nan')],[-1.]):
            with self.assertRaises(ValueError):a.distribution(v)

    def record(self):
        return {'validation':False,'input':[2304,1446],'output':[3456,2168],
                'warmup_worlds':360,'scope':'test','input_fingerprints':[],
                'after_warmup':{'metal_allocated_bytes':100,'buffers_created':2},
                'after_measurement':{'metal_allocated_bytes':100,'buffers_created':2},
                'samples':[{'sample':i,'world':360+i,'gpu_start_seconds':1.,'gpu_end_seconds':1.01,
                            'total_gpu_ms':10.,'cpu_encode_ms':.1,'cpu_submit_ms':.01,
                            'excluded_completion_wait_ms':20.} for i in range(300)]}

    def reduce(self,record):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/'cost.json';path.write_text(json.dumps(record));return a.cost(path)

    def test_host_wait_is_not_gpu_or_encode_cost(self):
        d=self.reduce(self.record());self.assertEqual(d['gpu']['median_ms'],10.);self.assertEqual(d['cpu_encode']['median_ms'],.1)
        self.assertEqual(d['excluded_completion_wait']['median_ms'],20.)

    def test_rejects_validation_wrong_size_and_missing_gpu_time(self):
        for change in ('validation','size','time','world'):
            r=self.record()
            if change=='validation':r['validation']=True
            elif change=='size':r['input']=[256,128]
            elif change=='time':r['samples'][0]['gpu_end_seconds']=0
            else:r['samples'][0]['world']=999
            with self.assertRaises(ValueError):self.reduce(r)


if __name__=='__main__':unittest.main()
