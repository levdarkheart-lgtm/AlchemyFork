"""Exercise PPD-014 source identity with real Git and minimal CMake projects."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]


@unittest.skipUnless(shutil.which("cmake") and shutil.which("git"), "CMake and Git required")
class BuildIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="alchemy-build-identity-")
        self.addCleanup(self.temporary.cleanup)
        self.source = Path(self.temporary.name) / "source"
        self.build = Path(self.temporary.name) / "build"
        self.source.mkdir()
        newview = self.source / "indra/newview"
        newview.mkdir(parents=True)
        (newview / "VIEWER_VERSION.txt").write_text("26.4.0\n")
        (self.source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.25)\n'
            'project(BuildIdentity NONE)\n'
            'set(INDRA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/indra")\n'
            f'include("{ROOT / "indra/cmake/BuildVersion.cmake"}")\n'
            f'configure_file("{ROOT / "indra/newview/llversioninfovars.h.in"}" '
            '"${CMAKE_CURRENT_BINARY_DIR}/llversioninfovars.h" @ONLY)\n'
            'file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/identity.txt" '
            '"${AL_GIT_REVISION_COMMIT}\\n${VIEWER_VERSION_REVISION}\\n'
            '${VIEWER_VERSION_REVISION_SOURCE}\\n${AL_GIT_REVISION_COUNT}\\n")\n'
        )
        self.git("init", "--quiet")
        self.commit("initial")

    def git(self, *arguments):
        result = subprocess.run(
            ["git", "-c", "user.name=Build identity test", "-c", "user.email=identity@example.invalid",
             "-c", "commit.gpgsign=false", "-c", "core.hooksPath=/dev/null", "-C", str(self.source),
             *arguments], check=True, capture_output=True, text=True,
        )
        return result.stdout.strip()

    def commit(self, message):
        (self.source / "marker.txt").write_text(message + "\n")
        self.git("add", ".")
        self.git("commit", "--quiet", "-m", message)
        return self.git("rev-parse", "HEAD")

    def configure(self, *, revision=None, run_id=None, arguments=()):
        environment = dict(os.environ)
        environment.pop("revision", None)
        environment.pop("GITHUB_RUN_ID", None)
        if revision is not None:
            environment["revision"] = revision
        if run_id is not None:
            environment["GITHUB_RUN_ID"] = run_id
        result = subprocess.run(
            ["cmake", "-S", str(self.source), "-B", str(self.build), *arguments],
            env=environment, capture_output=True, text=True,
        )
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        fields = (self.build / "identity.txt").read_text().splitlines()
        return dict(zip(("sha", "revision", "origin", "count"), fields))

    def assert_source(self, identity, expected):
        self.assertEqual(expected, identity["sha"])
        self.assertIn(f'#define LL_VIEWER_SOURCE_REVISION "{expected}"',
                      (self.build / "llversioninfovars.h").read_text())

    def test_revision_override_keeps_current_source_and_precedes_run_id(self):
        identity = self.configure(revision="68895", run_id="43314101072")
        self.assert_source(identity, self.git("rev-parse", "HEAD"))
        self.assertEqual(("68895", "environment"), (identity["revision"], identity["origin"]))

    def test_github_run_id_keeps_current_source(self):
        identity = self.configure(run_id="43314101072")
        self.assert_source(identity, self.git("rev-parse", "HEAD"))
        self.assertEqual(("43314101072", "GitHub run id"),
                         (identity["revision"], identity["origin"]))

    def test_local_version_uses_git_count(self):
        identity = self.configure()
        self.assert_source(identity, self.git("rev-parse", "HEAD"))
        self.assertEqual(("1", "git", "1"),
                         (identity["revision"], identity["origin"], identity["count"]))

    def test_cached_local_source_and_count_refresh_after_commit(self):
        old = self.configure()
        current = self.commit("next source")
        identity = self.configure()
        self.assertNotEqual(old["sha"], current)
        self.assert_source(identity, current)
        self.assertEqual(("2", "2"), (identity["revision"], identity["count"]))

    def test_cached_source_refreshes_while_numeric_override_stays_fixed(self):
        self.configure()
        current = self.commit("CI source changes")
        identity = self.configure(revision="68895")
        self.assert_source(identity, current)
        self.assertEqual("68895", identity["revision"])
        local = self.configure()
        self.assert_source(local, current)
        self.assertEqual("2", local["revision"])

    def test_cached_source_refreshes_with_run_id(self):
        self.configure()
        current = self.commit("run source changes")
        identity = self.configure(run_id="43314101072")
        self.assert_source(identity, current)
        self.assertEqual("43314101072", identity["revision"])

    def test_missing_git_does_not_reuse_cached_source(self):
        self.configure()
        identity = self.configure(revision="68895", arguments=("-DCMAKE_DISABLE_FIND_PACKAGE_Git=TRUE",))
        self.assert_source(identity, "")
        self.assertEqual("68895", identity["revision"])

    def test_failed_git_probe_does_not_reuse_cached_source(self):
        self.configure()
        shutil.rmtree(self.source / ".git")
        identity = self.configure(revision="68895")
        self.assert_source(identity, "")
        self.assertEqual("68895", identity["revision"])


if __name__ == "__main__":
    unittest.main()
