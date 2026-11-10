# Apple Metal research — 2026-11-10

Later owner observation: fullscreen sessions always displayed the Game Mode
activation notification. That is direct evidence for the sessions observed;
the earlier missing-key/direct-launch research does not prove nonactivation.
Leave Game Mode unchanged and de-prioritize this lead. An attempted read-only
ControlCenter CUA binding stalled7654seconds/error10005 despite a60second tool
timeout; no setting changed. Do not repeat unsupported system-helper binding.

User requested an additional search for Metal/M1 Max issues. Bounded Apple
documentation, release-note and developer-forum searches did not find a
published driver bug matching the measured resource-list/state-encoding cost.
That negative result does not establish that no such defect exists.

## Additional concrete lead: Game Mode activation

Apple's [macOS 26 release notes](https://developer.apple.com/documentation/macos-release-notes/macos-26-release-notes)
list issue153127050: launching an application binary directly from Terminal
may prevent Game Mode activation; the documented workaround uses `open`.
Whether this remains applicable on the installed26.5.2 is unknown.

Apple's [LSSupportsGameMode documentation](https://developer.apple.com/documentation/bundleresources/information-property-list/lssupportsgamemode)
recommends explicitly enabling the key and warns that omitting it may prevent
activation. Apple explains the fullscreen opt-in and reduced background work
in [Level up your games](https://developer.apple.com/videos/play/wwdc2025/209/).

Read-only checks against current source and canonical68601 found:

- `indra/newview/Info-AlchemySDL.plist` and packaged Info.plist declare
  `public.app-category.games`; both Game Mode support keys are absent.
- The existing QA launcher's `subprocess.Popen(command)` starts the executable
  directly. This resembles the documented launch condition, but does not prove
  that Launch Services or Game Mode behaved the same way in previous runs.
- Exact-original1eb also lacks those keys. Installed SDL source maps
  Control-Command-F to native `toggleFullScreen:`, separately from window Zoom.
- All viewers are closed. Actual Game Mode state during earlier measurements
  is unknown; no launch, bundle metadata or system setting was changed here.

Next authorized fullscreen trial should first record actual Game Mode status
from the macOS Game menu ([Apple instructions](https://support.apple.com/en-ie/105118));
onTahoe, Command-Escape opens Game Overlay. Any launch/opt-in experiment must preserve binary,
quality, camera and readiness and be measured separately from renderer changes.
This is a scheduling confound to test, not an explanation for the observed FPS.

## Guidance consistent with the existing diagnosis

Apple [documents argument buffers](https://developer.apple.com/documentation/metal/improving-cpu-performance-by-using-argument-buffers)
as a way to reduce repeated individual resource-binding and tracking costs.
This supports testing the material cache, not assuming its total cost is lower.
The current complete renderer benchmark still has a once-use pressure regression.

Apple's [CPU scheduling guidance](https://developer.apple.com/videos/play/tech-talks/110147/)
recommends bounded worker pools and avoiding excessive synchronization and tiny
jobs. Existing continuous trace evidence places world work on P cores and shows
roughly flat sampled CPU cost per frame; it does not establish a scheduler bug.

Apple's [GPU profiling guidance](https://developer.apple.com/documentation/xcode/optimizing-gpu-performance)
distinguishes overlapping runtime work from serial profiling and notes the
effect of GPU performance state. Existing replay pass times are not live frame
times. Nominal thermal observations do not measure GPU clocks.

No20FPS promise, system-update recommendation, priority override or driver
workaround follows from these sources. Source changes, new viewer timings and
complete-product release gates remain separate requirements.
