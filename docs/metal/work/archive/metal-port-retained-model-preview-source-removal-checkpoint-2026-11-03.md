# Retained model-preview source-removal execution checkpoint

**Source:** `1830ac9d0b`

**Build:** Alchemy Metal Development `26.4.0d68217`

**Detailed evidence:**
[state archive](../../state/archive/CURRENT-retained-model-preview-source-removal-checkpoint-2026-11-03.md)

## Completed implementation slice

After the complete Metal model-upload preview passed its feature checkpoint,
the 1,019-line retained/non-Darwin renderer and five orphaned helper/declaration
lines were deleted. Darwin `render()` now routes only to `renderMetal()`;
targetless native publication and fail-closed retry remain intact.

## Acceptance evidence

- Exact warnings-as-errors Xcode Release and Ninja builds pass at 68217.
- All 349 bundled Mach-O files are ARM64 and deep strict signing passes.
- Metal validation passes 52,468 assertions; focused native upload passes 17;
  fresh-render 2,612; appearance 13,207; render 557,312; viewer 98,973.
- The separately recorded shared-state diagnostic remains non-green at
  8,352/8,357 due to four legacy/headless frame-fence and one depth-format
  assertion; the new case passes.
- Strict inventory falls by five findings and three unique records to
  3,242/2,931 without rule or suppression changes.
- Harness validation and all 27 tooling tests pass.
- No viewer was launched or compared; owner-attended visual review remains
  queued.

## Next owner

Select the next live Darwin GL producer from source evidence, implement its full
Metal behavior, and only then retire the producer.
