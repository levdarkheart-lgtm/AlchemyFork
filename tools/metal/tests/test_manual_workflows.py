"""Guard the owner's manual-only Actions and native release budget policy."""
import pathlib
import unittest

import yaml

ROOT = pathlib.Path(__file__).resolve().parents[3]


class ManualWorkflowTests(unittest.TestCase):
    def test_every_live_workflow_requires_explicit_dispatch(self):
        paths = sorted((ROOT / '.github/workflows').glob('*.y*ml'))
        self.assertTrue(paths)
        for path in paths:
            with self.subTest(path=path.name):
                workflow = yaml.load(path.read_text(), Loader=yaml.BaseLoader)
                self.assertEqual(set(workflow['on']), {'workflow_dispatch'})

    def test_release_is_single_arm64_job_without_platform_matrix(self):
        workflow = yaml.load((ROOT / '.github/workflows/tag-release.yaml').read_text(), Loader=yaml.BaseLoader)
        self.assertEqual(set(workflow['jobs']), {'release'})
        job = workflow['jobs']['release']
        self.assertEqual(job['runs-on'], 'macos-26')
        self.assertNotIn('strategy', job)
        self.assertEqual(workflow['concurrency']['cancel-in-progress'], 'false')
        self.assertEqual(workflow['on']['workflow_dispatch']['inputs']['source_ref']['default'],
                         'codex/apple-silicon-metal')
        self.assertFalse(any('actions/upload-artifact' in str(s.get('uses', '')) for s in job['steps']))

    def test_manual_harness_installs_numeric_dependency_before_testing(self):
        workflow = yaml.load((ROOT / '.github/workflows/metal-harness.yml').read_text(), Loader=yaml.BaseLoader)
        steps = workflow['jobs']['validate-harness']['steps']
        install = next(i for i, step in enumerate(steps) if 'pip install' in step.get('run', ''))
        tests = next(i for i, step in enumerate(steps) if 'unittest discover' in step.get('run', ''))
        self.assertIn('-r tools/metal/requirements.txt', steps[install]['run'])
        requirements = (ROOT / 'tools/metal/requirements.txt').read_text()
        self.assertIn('numpy==', requirements)
        self.assertIn('PyYAML==', requirements)
        self.assertLess(install, tests)


if __name__ == '__main__':
    unittest.main()
