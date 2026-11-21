"""Exercise the production CMake receipt gate with isolated external-tool fixtures."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
GATE = ROOT / "indra/cmake/OpenHTJ2K.cmake"
REVISION = "72ea0450d2b522a88e7dbb9c1a9c164fb029c1f2"


@unittest.skipUnless(shutil.which("cmake"), "CMake is required for the actual receipt gate")
class OpenHTReceiptGateTests(unittest.TestCase):
    def configure(self, change=None, *, legacy=False, dirty=False, decoder=None):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory).resolve()
            source, tools = base / "source", base / "bin"
            source.mkdir(); tools.mkdir()
            library = base / "libopenhtj2k.a"
            library.write_bytes(b"!<arch>\nreceipt-validation fixture")
            receipt = dict(schema_version=1, source_revision=REVISION, source_clean=True,
                           architecture="arm64", configuration="Release", deployment_target="26.0",
                           source_dir=str(source), library_path=str(library),
                           library_sha256=hashlib.sha256(library.read_bytes()).hexdigest())
            if change: change(receipt)
            receipt_path = base / "receipt.json"
            receipt_path.write_text(json.dumps(receipt))
            git = tools / "git"
            git.write_text("#!/usr/bin/env python3\nimport sys\n"
                           f"print({REVISION!r} if 'rev-parse' in sys.argv else "
                           f"{' M source/core/coding/changed.cpp' if dirty else ''!r})\n")
            git.chmod(0o755)
            xcrun = tools / "xcrun"
            xcrun.write_text("#!/usr/bin/env python3\nprint('arm64')\n")
            xcrun.chmod(0o755)
            project = base / "project"
            project.mkdir()
            content = "cmake_minimum_required(VERSION 4.0)\nproject(OpenHTReceipt LANGUAGES CXX)\n"
            if decoder is not None:
                content += f'set(AL_EXPERIMENTAL_OPENHTJ2K {decoder} CACHE BOOL "Retired decoder selector")\n'
            content += (f'set(AL_OPENHTJ2K_SOURCE_DIR "{source}")\n'
                       f'set(AL_OPENHTJ2K_LIBRARY "{library}")\n')
            if not legacy: content += f'set(AL_OPENHTJ2K_BUILD_RECEIPT "{receipt_path}")\n'
            content += f'include("{GATE}")\n'
            content += ('if(DEFINED AL_EXPERIMENTAL_OPENHTJ2K)\n'
                        '  message(FATAL_ERROR "Retired decoder selector was not removed")\nendif()\n'
                        'if(NOT TARGET ll::openhtj2k)\n'
                        '  message(FATAL_ERROR "Required OpenHTJ2K target is missing")\nendif()\n')
            (project / "CMakeLists.txt").write_text(content)
            environment = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"])
            result = subprocess.run(["cmake", "-S", str(project), "-B", str(base / "build")],
                                    env=environment, capture_output=True, text=True)
            return result.returncode, result.stdout + result.stderr

    def test_matching_rebuilt_archive_receipt_is_accepted(self):
        code, output = self.configure()
        self.assertEqual(0, code, output)

    def test_openhtj2k_is_required_without_a_selector(self):
        code, output = self.configure(decoder=None)
        self.assertEqual(0, code, output)
        self.assertIn("OpenHTJ2K 0.27.0", output)

    def test_disabling_required_pixel_decoder_is_rejected(self):
        code, output = self.configure(decoder="OFF")
        self.assertNotEqual(0, code)
        self.assertIn("requires OpenHTJ2K texture pixel decoding", output)

    def test_retired_on_cache_is_migrated_and_removed(self):
        code, output = self.configure(decoder="ON")
        self.assertEqual(0, code, output)
        self.assertIn("Migrating retired AL_EXPERIMENTAL_OPENHTJ2K=ON", output)

    @unittest.skipUnless(shutil.which("c++"), "C++ preprocessor is required")
    def test_direct_compile_has_no_selector_and_rejects_retired_off(self):
        source = (ROOT / "indra/llimagej2coj/llimagej2coj.cpp").read_text()
        prefix = source[:source.index('#include "linden_common.h"')]
        for definitions, succeeds in (([], True), (["-DAL_EXPERIMENTAL_OPENHTJ2K=0"], False),
                                       (["-DAL_EXPERIMENTAL_OPENHTJ2K=1"], True)):
            with self.subTest(definitions=definitions):
                result = subprocess.run(["c++", "-E", "-x", "c++", *definitions, "-"],
                                        input=prefix, capture_output=True, text=True)
                self.assertEqual(succeeds, result.returncode == 0, result.stderr)
                if not succeeds:
                    self.assertIn("requires OpenHTJ2K", result.stderr)

    def test_pixel_decoder_and_packaged_license_have_no_alternate_selection(self):
        source = (ROOT / "indra/llimagej2coj/llimagej2coj.cpp").read_text()
        self.assertIn('#include "decoder.hpp"', source)
        self.assertNotIn("opj_decode(", source)
        self.assertNotIn("opj_end_decompress(", source)
        self.assertIn("opj_read_header(", source)
        self.assertIn("opj_encode(", source)
        build = (ROOT / "indra/llimagej2coj/CMakeLists.txt").read_text()
        self.assertIn("include(OpenHTJ2K)", build)
        self.assertIn("target_link_libraries(llimagej2coj PRIVATE ll::openhtj2k)", build)
        self.assertNotIn("AL_EXPERIMENTAL_OPENHTJ2K", build)
        notices = json.loads((ROOT / "indra/cmake/attribution.json").read_text())
        # Notices have a stable dependencies array; find the actual package record.
        records = next(value for value in notices.values() if isinstance(value, list) and
                       any(isinstance(row, dict) and row.get("name") == "OpenHTJ2K" for row in value))
        self.assertNotIn("when", next(row for row in records if row.get("name") == "OpenHTJ2K"))
        install = (ROOT / "indra/cmake/ViewerInstall.cmake").read_text()
        self.assertNotIn("if(AL_EXPERIMENTAL_OPENHTJ2K)", install)

    def test_changed_archive_digest_is_rejected(self):
        code, output = self.configure(lambda receipt: receipt.update(library_sha256="0" * 64))
        self.assertNotEqual(0, code)
        self.assertIn("receipt does not match", output)

    def test_foreign_architecture_receipt_is_rejected(self):
        code, output = self.configure(lambda receipt: receipt.update(architecture="x86_64"))
        self.assertNotEqual(0, code)
        self.assertIn("receipt does not match", output)

    def test_wrong_source_revision_is_rejected(self):
        code, output = self.configure(lambda receipt: receipt.update(source_revision="a" * 40))
        self.assertNotEqual(0, code)
        self.assertIn("receipt does not match", output)

    def test_mismatched_receipt_path_is_rejected(self):
        code, output = self.configure(lambda receipt: receipt.update(library_path="/another/libopenhtj2k.a"))
        self.assertNotEqual(0, code)
        self.assertIn("receipt does not match", output)

    def test_dirty_source_is_rejected(self):
        code, output = self.configure(dirty=True)
        self.assertNotEqual(0, code)
        self.assertIn("entirely clean", output)

    def test_unconfirmed_source_clean_value_is_rejected(self):
        code, output = self.configure(lambda receipt: receipt.update(source_clean="true"))
        self.assertNotEqual(0, code)
        self.assertIn("receipt does not match", output)

    def test_legacy_archive_identity_guard_remains_strict(self):
        code, output = self.configure(legacy=True)
        self.assertNotEqual(0, code)
        self.assertIn("differs from the benchmarked ARM64 Release archive", " ".join(output.split()))


if __name__ == "__main__":
    unittest.main()
