#!/usr/bin/env python3
"""Identify exact pinned algorithm, generated MSL and experimental native host."""
import argparse
import hashlib
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[4]
DEST = Path(__file__).with_name("PROVENANCE.json")
def sha(path):
    return hashlib.sha256((ROOT/path).read_bytes()).hexdigest()
def result():
    paths = [
      "tools/metal/aurora-native/translator/Cargo.toml",
      "tools/metal/aurora-native/translator/rust-toolchain.toml",
      "tools/metal/aurora-native/translator/Cargo.lock",
      "tools/metal/aurora-native/translator/src/main.rs",
      "indra/almetal/almetalaurora.h", "indra/almetal/almetalaurora.mm",
    ]
    paths += [f"tools/metal/aurora-evaluation/vendor/aurora/shaders/fsr/{n}.wgsl" for n in ("common","prepare_inputs","pyramids","reactivity","accumulate")]
    paths += [f"indra/almetal/shaders/aurora/{n}.metal" for n in ("prepare_inputs","pyramids","reactivity","accumulate")]
    paths += ["indra/almetal/shaders/aurora/sources.h"]
    return {"algorithm":"Aurora WGSL port of FSR 3.1 (FidelityFX SDK 1.1.4)",
      "upstream":"ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f",
      "naga":"6c13013b3f9c8a7e45154d45d120ada0cbe55a23",
      "msl_version":"4.0", "evaluated_wgpu_msl_version":"4.0 on macOS26", "runtime_compile_options":{"preserveInvariance":True,"floating_point":"Metal defaults (same as evaluated wgpu Metal backend)"},
      "rcas":"OFF (no pipeline created or executed)",
      "sha256":{p:sha(p) for p in paths}}
def main():
    p=argparse.ArgumentParser();p.add_argument("--write",action="store_true");args=p.parse_args()
    actual=result()
    if args.write:DEST.write_text(json.dumps(actual,indent=2)+"\n")
    else:
        expected=json.loads(DEST.read_text())
        if actual!=expected:raise SystemExit("Native Aurora provenance mismatch; inspect changes before updating manifest")
    print("Native Aurora provenance verified" if not args.write else "Native Aurora provenance recorded")
if __name__=="__main__":main()
