# Experimental native Aurora reconstruction notices

The experimental native host `almetalaurora.mm` follows the resources, constants,
bindings and render graph of Aurora's `crates/aurora-render/src/fsr.rs`, pinned at
commit `ee458fbf1d95e98c5b3a55c0027a523bb28d7e7f`. That host is covered by
Aurora's GNU General Public License version 3 or later. Additional native Metal
integration is Copyright (C) 2026, Alchemy Viewer Project, under the same terms.

The generated `prepare_inputs.metal`, `pyramids.metal`, `reactivity.metal` and
`accumulate.metal`, and their embedded `sources.h`, are reproducible offline MSL
translations of Aurora's unchanged WGSL port of **AMD FidelityFX Super Resolution
3.1 (upscaler only)** from FidelityFX SDK 1.1.4. They are not an AMD-maintained MSL
implementation and do not implement frame generation.

Original upstream WGSL, `fsr.rs`, full GNU GPL licence, Aurora notices/trademarks
and FidelityFX MIT notice are retained in:
`tools/metal/aurora-evaluation/vendor/aurora/`. The exact source and generated
hashes and compiler options are recorded in:
`tools/metal/aurora-native/translator/PROVENANCE.json`.

Naga, pinned to the evaluated Aurora fork revision
`6c13013b3f9c8a7e45154d45d120ada0cbe55a23`, is used **offline only**; the viewer
has no Naga, wgpu or Rust runtime dependency. Source translation does not remove
or replace the upstream copyright and licence notices.

## FidelityFX notice

Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
