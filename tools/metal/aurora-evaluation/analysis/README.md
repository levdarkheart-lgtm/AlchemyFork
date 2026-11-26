# Offline perspective quality reducer

This directory only reads retained image arrays and metadata. It neither launches the viewer nor executes a GPU algorithm. Python 3 and NumPy are required (`python3 -m pip install -r requirements.txt`). The reducer is intentionally pinned to this one256×128→384×192 perspective palm (96×96cell at192,0,scored6pxinterior), not an unqualified generalimage benchmark. Raw arrays/video remain private. The JSON output omits machine-absolute paths and embeds only receipt identities, not private command payloads.

Example for the new shared MetalFX baseline:

```sh
python3 tools/metal/aurora-evaluation/analysis/quality.py \
  --manifest artifacts/metal/aurora-evaluation-20261126/shared-inputs-warm360/shared-perspective/manifest.json \
  --label MetalFX-perspective-warm360 \
  --output artifacts/metal/aurora-evaluation-20261126/offline-quality/metalfx-warm360.json
```

For Aurora, use the same input manifest and supply its actual `--main-dir`, `--reference-dir`, `--main-pattern` and `--reference-pattern` (patterns use `{frame:03d}`). All warm-up/reference outputs must exist. Each algorithm uses its own independently warmed phase-matched final-camera references. No old orthographic metrics are mixed into the new population. Frame indices are zero-based; world IDs come from the retained CSV.

`quality_protocol.json` preserves the pre-candidate definitions. [corrective_reference_protocol.json](corrective_reference_protocol.json) preserves the one fixed360-warm maturity correction; the example above uses finalcorrective data, not the shortpreliminary reference. Original quality bounds are mandatory; halo/overshoot/local-trail indicators remain descriptive and support matched visual inspection, not invented hard gates. The reducer uses binary32 pixel math and double accumulation; the C++ seventeen-digit baseline exports remain authoritative at last-bit precision. Validate offline parity before interpreting candidate results.

Run the deterministic mathematical tests with `python3 -m unittest discover -s tools/metal/aurora-evaluation/analysis -p 'test_*.py' -v`. Cost is considered only after qualification; these files provide no viewer FPS result.
