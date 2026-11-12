# Local SL texture decoder experiment

These standalone CPU adapters do not change the viewer's codec or dependencies.
Grok is AGPLv3; OpenHTJ2K is BSD-3-Clause. Keep downloaded libraries and private
texture inputs under ignored `artifacts/metal/`. Do not publish cache data.

Extract copies of actual cached JPEG2000 codestreams without modifying the cache:

```sh
python3 tools/metal/texture_decode_benchmark.py --cache "$HOME/Library/Caches/AlchemyNext/texturecache" --output artifacts/metal/texture-decode-20261112
```

The extractor understands the current macOS cache's 44-byte entries header,
28-byte entries and 600-byte codestream header records. It fails on another
layout. Complete inputs require the declared byte count and an EOC marker.
Partial inputs are separate. Selection balances dimensions/component counts;
this is coverage-oriented, not a frequency-weighted region workload. Inputs
are anonymous copies, with SHA256 metadata in `corpus.json`.

Build each adapter with Apple Clang, C++23, `-O3 -DNDEBUG -arch arm64`, this
directory's include path and its codec's headers/library. OpenJPEG must link
the viewer's installed patched library. The local experiment's
`adapter-builds.json` records complete commands and binary hashes; candidate
build receipts pin upstream revisions, options and licenses.

Run an adapter with:

```sh
artifacts/metal/texture-decode-20261112/openjpeg-benchmark --manifest artifacts/metal/texture-decode-20261112/complete-full.tsv --workers 4 --rounds 5 --warmup 1 --output artifacts/metal/texture-decode-20261112/openjpeg-four.json
```

Each TSV row is `CASE_ID<TAB>ABSOLUTE_FILE_PATH<TAB>REDUCTION`. The `progressive`
manifest name denotes independent decodes at reductions 3, 2, 1 and 0, limited
by main-header COD/COC declarations. Rows are shuffled reproducibly. This is a
mixed-resolution workload, not simulation of network arrivals, prioritization
or retained incremental decoder state. Tile-specific coding overrides can
still impose tighter reduction limits; treat any decode failure as a failure.

All adapters create a fresh decoder and return top-down, interleaved unsigned
8-bit pixels. They reject signed, subsampled or >4-component layouts. Codec
parallelism is one execution lane: OpenJPEG API threads=0 (inline), Grok=1
and OpenHTJ2K=1. OpenHTJ2K's selected API copies compressed input into a padded
buffer; the other two use memory input without that copy. No GPU work runs.

The common harness preloads compressed inputs, validates pixels in its first
warmup pass and times decode/setup, pixel conversion and destruction. Hashing
and optional `--dump-dir` raw-pixel files are outside measured rounds. Batch
wall time includes outer worker creation/join and scheduling. Run candidates
sequentially on an otherwise idle host; interleave repeated variant runs and
record thermal/power state. Reject failed cases from performance comparisons,
and compare dimensions, alpha and pixel errors before considering replacement.
FNV hashes identify exact equality; they are not an image-quality metric.

These results isolate CPU decoding. They do not measure viewer queue waits,
Metal uploads/mipmap generation, frame time or complete relaunch loading time.
