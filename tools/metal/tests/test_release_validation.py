"""Exercise packaged release gates with real bundle fixtures and mocked Apple tools."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("verify_release", ROOT / "tools/metal/verify_release.py")
release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(release)
SOURCE = "a" * 40
TAG = "26.4.0.68898-alpha5"


class ReleaseValidationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name).resolve()
        self.app = self.base / "Alchemy Metal.app"
        self.main = self.bundle(self.app, "Alchemy Metal")
        self.main.write_bytes(b"fixture " + SOURCE.encode() + b" OpenHTJ2K 0.27.0 decode; ")
        self.cef = self.bundle(self.app / "Contents/Resources/media_plugin_cef.app", "media_plugin_cef")
        self.vlc = self.bundle(self.app / "Contents/Resources/media_plugin_libvlc.app", "media_plugin_libvlc")
        self.source = self.base / "source"
        self.source.mkdir()
        self.license = self.source / "LICENSE"
        self.license.write_text("Pinned BSD-3-Clause license fixture\n")
        notice = self.app / "Contents/Resources/licenses/OpenHTJ2K.txt"
        notice.parent.mkdir(parents=True)
        notice.write_bytes(self.license.read_bytes())
        self.library = self.base / "libopenhtj2k.a"
        self.library.write_bytes(b"!<arch>\nfixture native archive")
        self.receipt = dict(schema_version=1, source_revision=release.OPENHT_REVISION,
                            architecture="arm64", configuration="Release", deployment_target="26.0",
                            portable_tuning="apple-m1", source_clean=True, success=True,
                            source_dir=str(self.source), library_path=str(self.library),
                            library_sha256=hashlib.sha256(self.library.read_bytes()).hexdigest())
        self.receipt_path = self.base / "build-receipt.json"
        self.write_receipt()
        self.imports = {}
        self.undefined = {}
        self.symbols = ("00000001 (__TEXT,__text) external _ZN26LLStreamingAudio_CoreAudioC1Ev\n"
                        "00000002 (__TEXT,__text) external _ZN19LLWebRTCVoiceClientC1Ev\n"
                        "00000003 (__TEXT,__text) external _ZN10open_htj2k18openhtj2k_decoderC1Ev\n"
                        "00000004 (__TEXT,__text) external _opj_encode\n")

    def write_receipt(self):
        self.receipt_path.write_text(json.dumps(self.receipt))

    def bundle(self, directory, executable):
        (directory / "Contents/MacOS").mkdir(parents=True)
        info = dict(CFBundleExecutable=executable, CFBundleShortVersionString="26.4.0.68898",
                    CFBundleVersion="26.4.0d68898",
                    CFBundleIdentifier="org.alchemyviewer.fixture")
        (directory / "Contents/Info.plist").write_bytes(plistlib.dumps(info))
        binary = directory / "Contents/MacOS" / executable
        binary.write_bytes(b"fixture owned executable")
        return binary

    def run_tool(self, argv, **_kwargs):
        name = Path(argv[0]).name
        if name == "otool":
            stdout = self.imports.get(str(argv[-1]), "/System/Library/Frameworks/Metal.framework/Versions/A/Metal\n")
        elif name == "nm":
            stdout = self.undefined.get(str(argv[-1]), "") if argv[1] == "-u" else self.symbols
        elif argv[1:3] == ["lipo", "-archs"]:
            stdout = "arm64\n"
        else:
            stdout = "fixture tool version\n"
        return subprocess.CompletedProcess(argv, 0, stdout, "")

    def verify(self, *, source=SOURCE, tag=TAG, report=None):
        with patch.object(release.subprocess, "run", side_effect=self.run_tool):
            return release.verify_release(self.app, source, tag, self.receipt_path, report=report)

    def test_matching_release_and_real_license_fixture_pass(self):
        result = self.verify()
        self.assertTrue(result["success"])
        self.assertEqual(3, len(result["owned_bundles"]))
        self.assertTrue(result["openhtj2k"]["archive_hash_verified"])
        self.assertFalse(result["openhtj2k"]["runtime_decoder_verified"])
        self.assertIn("Not recoverable", result["openhtj2k"]["linked_archive_identity"])
        self.assertTrue(result["openhtj2k"]["packaged_license"]["source_match"])

    def test_malformed_tag_does_not_reach_external_tools(self):
        with patch.object(release.subprocess, "run") as runner:
            with self.assertRaisesRegex(release.ValidationError, "Release tag"):
                release.verify_release(self.app, SOURCE, "26.4.0.68898", self.receipt_path)
            runner.assert_not_called()

    def test_non_sha_source_is_rejected(self):
        with self.assertRaisesRegex(release.ValidationError, "Source SHA"):
            self.verify(source="codex/apple-silicon-metal")

    def test_embedded_source_mismatch_is_rejected(self):
        with self.assertRaisesRegex(release.ValidationError, "does not embed"):
            self.verify(source="b" * 40)

    def test_tag_version_and_revision_must_match_packaged_info(self):
        for tag in ("26.4.0.68899-alpha5", "26.5.0.68898-alpha5"):
            with self.subTest(tag=tag), self.assertRaisesRegex(release.ValidationError, "does not match tag version"):
                self.verify(tag=tag)

    def test_bundle_revision_cannot_disagree_with_short_version(self):
        path = self.app / "Contents/Info.plist"
        info = plistlib.loads(path.read_bytes()); info["CFBundleVersion"] = "26.4.0d68897"
        path.write_bytes(plistlib.dumps(info))
        with self.assertRaisesRegex(release.ValidationError, "CFBundleVersion"):
            self.verify()

    def test_external_template_revision_is_recorded_and_validated(self):
        with patch.dict(os.environ, MASTER_MESSAGE_TEMPLATE_SHA="c" * 40):
            result = self.verify()
            self.assertEqual("c" * 40, result["external_revisions"]["master_message_template"])
        with patch.dict(os.environ, MASTER_MESSAGE_TEMPLATE_SHA="main"):
            with self.assertRaisesRegex(release.ValidationError, "MASTER_MESSAGE_TEMPLATE_SHA"):
                self.verify()

    def test_each_owned_host_rejects_forbidden_import(self):
        for host in (self.cef, self.vlc):
            with self.subTest(host=host):
                self.imports[str(host)] = "/System/Library/Frameworks/OpenGL.framework/Versions/A/OpenGL\n"
                with self.assertRaisesRegex(release.ValidationError, "Forbidden direct graphics"):
                    self.verify()
                self.imports.clear()

    def test_undefined_owned_graphics_calls_are_rejected(self):
        self.undefined[str(self.vlc)] = "                 U _glBindTexture\n"
        with self.assertRaisesRegex(release.ValidationError, "Forbidden owned graphics"):
            self.verify()

    def test_native_metal_link_is_required(self):
        self.imports[str(self.main)] = "/usr/lib/libSystem.B.dylib\n"
        with self.assertRaisesRegex(release.ValidationError, "must link native Metal"):
            self.verify()

    def test_compiled_music_voice_decoder_and_encoder_evidence_required(self):
        original = self.symbols
        for marker in ("LLStreamingAudio_CoreAudio", "LLWebRTCVoiceClient", "openhtj2k_decoder", "_opj_encode"):
            with self.subTest(marker=marker):
                self.symbols = "\n".join(line for line in original.splitlines() if marker not in line)
                with self.assertRaisesRegex(release.ValidationError, "Missing compiled"):
                    self.verify()
        self.symbols = original

    def test_undefined_symbol_is_not_a_linked_feature(self):
        self.symbols = self.symbols.replace("00000002 (__TEXT,__text)", "                 (undefined)")
        with self.assertRaisesRegex(release.ValidationError, "Missing compiled webrtc_voice"):
            self.verify()

    def test_unused_decoder_or_wrong_engine_marker_cannot_pass(self):
        self.main.write_bytes(b"fixture " + SOURCE.encode())
        with self.assertRaisesRegex(release.ValidationError, "selected OpenHTJ2K"):
            self.verify()

    def test_duplicate_owned_host_is_rejected(self):
        self.bundle(self.app / "Contents/Other/media_plugin_cef.app", "second_cef")
        with self.assertRaisesRegex(release.ValidationError, "exactly one owned media host"):
            self.verify()

    def test_receipt_requires_pinned_clean_portable_native_success(self):
        original = self.receipt.copy()
        for change in (dict(schema_version=True), dict(source_revision="b" * 40),
                       dict(architecture="x86_64"), dict(source_clean="true"),
                       dict(success=False), dict(portable_tuning="native"),
                       dict(deployment_target="25.0")):
            with self.subTest(change=change):
                self.receipt = dict(original, **change); self.write_receipt()
                with self.assertRaises(release.ValidationError):
                    self.verify()
        self.receipt = original; self.write_receipt()

    def test_tampered_available_archive_is_rejected(self):
        self.library.write_bytes(b"!<arch>\nchanged archive")
        with self.assertRaisesRegex(release.ValidationError, "archive digest"):
            self.verify()

    def test_unavailable_original_archive_is_explicitly_unverified(self):
        self.library.unlink()
        result = self.verify()
        self.assertTrue(result["success"])
        self.assertFalse(result["openhtj2k"]["archive_hash_verified"])
        self.assertIn("unavailable", result["openhtj2k"]["archive_verification_limit"])

    def test_missing_or_wrong_bundled_license_is_rejected(self):
        notice = self.app / "Contents/Resources/licenses/OpenHTJ2K.txt"
        notice.unlink()
        with self.assertRaisesRegex(release.ValidationError, "Missing packaged OpenHTJ2K license"):
            self.verify()
        notice.write_text("another license\n")
        with self.assertRaisesRegex(release.ValidationError, "license differs"):
            self.verify()

    def test_bundle_executable_cannot_escape_the_application(self):
        info_path = self.app / "Contents/Info.plist"
        info = plistlib.loads(info_path.read_bytes());info["CFBundleExecutable"] = "../outside"
        info_path.write_bytes(plistlib.dumps(info))
        with self.assertRaisesRegex(release.ValidationError, "Invalid CFBundleExecutable"):
            self.verify()

    def test_inspection_tool_failure_is_not_a_pass(self):
        with patch.object(release.subprocess, "run", return_value=subprocess.CompletedProcess([], 1, "", "failed")):
            with self.assertRaisesRegex(release.ValidationError, "Command failed"):
                release.verify_release(self.app, SOURCE, TAG, self.receipt_path)

    def test_cli_preserves_failure_receipt(self):
        output = self.base / "receipt/validation.json"
        code = release.main([str(self.app), "--source-sha", SOURCE, "--tag", "bad-tag",
                             "--openht-receipt", str(self.receipt_path), "--output", str(output)])
        self.assertEqual(1, code)
        record = json.loads(output.read_text())
        self.assertFalse(record["success"])
        self.assertIn("Release tag", record["error"])


if __name__ == "__main__":
    unittest.main()
