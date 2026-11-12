"""Synthetic cache fixtures for the local JPEG2000 corpus extractor."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import uuid


TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "texture_decode_benchmark", TOOLS / "texture_decode_benchmark.py")
BENCHMARK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(BENCHMARK)


def marker(code, payload):
    return struct.pack(">HH", code, len(payload) + 2) + payload


def siz(width=64, height=64, components=3):
    # SOC followed by the mandatory Part 1 SIZ fields and component records.
    return struct.pack(">4H8IH", 0xFF4F, 0xFF51, 38 + 3 * components, 0,
                       width, height, 0, 0, width, height, 0, 0, components) + (
                           b"\x07\x01\x01" * components)


def cod(levels):
    return marker(0xFF52, bytes((0, 0, 0, 1, 0, levels, 4, 4, 0, 1)))


def coc(component, levels):
    return marker(0xFF53, bytes((component, 0, levels, 4, 4, 0, 1)))


def codestream(width=64, height=64, components=3, levels=3, length=900):
    header = siz(width, height, components) + cod(levels)
    header += marker(0xFF90, struct.pack(">HIBB", 0, 0, 0, 1)) + b"\xff\x93"
    if length < len(header) + 2:
        raise ValueError("fixture length is shorter than its header")
    # This is a parser fixture, not a decodable or copyrighted texture.
    return header + b"\x42" * (length - len(header) - 2) + b"\xff\xd9"


def write_cache(path, records):
    path.mkdir()
    entries = bytearray(struct.pack("<fI32sI", 1.71, 64,
                                    b"OpenJPEG runtime: 2.5.4", len(records)))
    headers = bytearray()
    bodies = []
    for index, (data, declared_size) in enumerate(records, 1):
        ident = uuid.UUID(int=index)
        body = data[600:]
        entries.extend(struct.pack("<16siiI", ident.bytes, declared_size, len(body), 0))
        headers.extend(data[:600].ljust(600, b"\0"))
        body_path = path / str(ident)[0] / (str(ident) + ".texture")
        if body:
            body_path.parent.mkdir(exist_ok=True)
            body_path.write_bytes(body)
        bodies.append(body_path)
    (path / "texture.entries").write_bytes(entries)
    (path / "texture.cache").write_bytes(headers)
    return bodies


def snapshot(path):
    return {str(p.relative_to(path)): p.read_bytes() for p in path.rglob("*") if p.is_file()}


class TextureCorpusTests(unittest.TestCase):
    def test_complete_and_partial_reconstruction_preserves_original_cache(self):
        full = codestream(1024, 512, 4, levels=5, length=1400)
        small = codestream(4, 4, 3, levels=1, length=200)
        partial = codestream(256, 256, 3, levels=4, length=1600)[:750]
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            cache, output = base / "cache", base / "output"
            write_cache(cache, [(full, len(full)), (small, len(small)), (partial, 1600)])
            before = snapshot(cache)
            summary = BENCHMARK.extract(cache, output, limit=10, partial_limit=10)
            self.assertEqual(before, snapshot(cache), "extracting must not modify the viewer cache")
            self.assertEqual({"complete": 2, "partial": 1}, summary["source_counts"])
            by_hash = {item["sha256"]: item for item in summary["selected"]}
            for data, category in ((full, "complete"), (small, "complete"), (partial, "partial")):
                item = by_hash[hashlib.sha256(data).hexdigest()]
                self.assertEqual(category, item["category"])
                self.assertEqual(data, (output / "corpus" / (item["id"] + ".j2c")).read_bytes())
                self.assertEqual(len(data), item["bytes"])
            small_item = by_hash[hashlib.sha256(small).hexdigest()]
            rows = [line.split("\t") for line in (output / "complete-progressive.tsv").read_text().splitlines()]
            small_reductions = {int(row[2]) for row in rows if row[0].startswith(small_item["id"] + "_")}
            self.assertEqual({0, 1}, small_reductions,
                             "small textures must not request nonexistent resolution levels")
            self.assertEqual(len(rows), len({row[0] for row in rows}))

    def test_incomplete_cache_records_are_reported_without_entering_corpus(self):
        valid = codestream(length=900)
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            cache = base / "cache"
            bodies = write_cache(cache, [(valid, 900), (valid, 900), (b"invalid", 7), (b"", -1)])
            bodies[0].unlink()
            bodies[1].write_bytes(b"changed")
            summary = BENCHMARK.extract(cache, base / "output", limit=8, partial_limit=8)
            self.assertEqual([], summary["selected"])
            self.assertEqual({"missing_body": 1, "body_size_changed": 1,
                              "unsupported_header": 1, "inactive": 1}, summary["source_counts"])

    def test_foreign_or_truncated_cache_layout_fails_closed(self):
        for malformed in ("short_header", "foreign_word_size", "missing_entry", "short_texture_header"):
            with self.subTest(malformed=malformed), tempfile.TemporaryDirectory() as directory:
                base = Path(directory)
                cache = base / "cache"
                write_cache(cache, [(codestream(length=200), 200)])
                entry_path = cache / "texture.entries"
                entries = bytearray(entry_path.read_bytes())
                if malformed == "short_header":
                    entry_path.write_bytes(entries[:43])
                elif malformed == "foreign_word_size":
                    struct.pack_into("<I", entries, 4, 32)
                    entry_path.write_bytes(entries)
                elif malformed == "missing_entry":
                    entry_path.write_bytes(entries[:-1])
                else:
                    (cache / "texture.cache").write_bytes(b"\0" * 599)
                with self.assertRaises(ValueError):
                    BENCHMARK.extract(cache, base / "output", limit=8, partial_limit=8)

    def test_siz_rejects_signed_or_subsampled_components(self):
        good = siz(128, 32, 4)
        self.assertEqual((128, 32, 4), BENCHMARK.image_info(good))
        for offset, value in ((42, 0x87), (43, 2), (44, 2)):
            with self.subTest(offset=offset):
                data = bytearray(good)
                data[offset] = value
                with self.assertRaises(ValueError):
                    BENCHMARK.image_info(data)

    def test_reduction_limit_respects_component_overrides_and_main_header_boundary(self):
        header = siz() + cod(5) + coc(1, 2)
        # Tile-header coding is outside this main-header parser's contract.
        tile = marker(0xFF90, struct.pack(">HIBB", 0, 0, 0, 1)) + cod(0)
        self.assertEqual(2, BENCHMARK.max_reduce(header + tile))
        self.assertEqual(1, BENCHMARK.max_reduce(siz() + cod(1) + coc(2, 3)))
        with self.assertRaises(ValueError):
            BENCHMARK.max_reduce(siz() + cod(3)[:5])

    def test_balanced_selection_is_bounded_and_independent_of_input_order(self):
        candidates = [dict(width=size, height=size, components=components,
                           sha256=f"{rank:064x}", name=(size, components, rank))
                      for size in (64, 1024) for components in (3, 4) for rank in (3, 1, 2)]
        selected = BENCHMARK.select_balanced(candidates, 6)
        reverse = BENCHMARK.select_balanced(list(reversed(candidates)), 6)
        self.assertEqual(selected, reverse)
        self.assertEqual(6, len(selected))
        self.assertEqual({(64, 3), (64, 4), (1024, 3), (1024, 4)},
                         {(item["width"], item["components"]) for item in selected[:4]})
        self.assertTrue(all(item["name"][2] == 1 for item in selected[:4]),
                        "selection should be stable within each size/component stratum")
        self.assertEqual(12, len(candidates), "selection must not consume its input list")


if __name__ == "__main__":
    unittest.main()
