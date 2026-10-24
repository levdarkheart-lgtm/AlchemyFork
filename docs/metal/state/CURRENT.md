# Current engineering state (recovery snapshot)

**As recorded:** 2026-10-24, harness authoring. **Do not interpret this file as local runtime evidence.**

- Fork target: `levdarkheart-lgtm/AlchemyFork` (user-provided; its private HEAD was **not accessible** to the harness authoring session's GitHub installation).
- Branch/SHA: **UNVERIFIED** until Codex runs in local fork.
- Physical host: M1 Max / 64 GiB per owner; GPU core count, OS/Xcode, display mode and device feature queries **UNVERIFIED**.
- Local open-source ARM64 build: **NOT RUN** in authoring environment. Public upstream's ARM64 macOS CI passed; this is not local proof.
- Native Metal implementation: **NOT STARTED** in this harness-only change. Existing Alchemy viewer is GL-based.
- Feature matrix: reference and candidate results **UNVERIFIED**.
- Baseline / candidate performance: **NO SAMPLES**; no claimed speedup.
- Known external constraints: no licensed private Alchemy SDKs available/required by documented `xcode-os-arm64` baseline; verify actual manifest. GitHub connector for target fork was inaccessible to harness authoring session; local Codex clone bypasses that limitation.

## Exact first executable action for local Codex
1. Read the root `AGENTS.md` and mission; inspect local git state, source HEAD and docs.
2. Run `tools/metal/doctor.sh`; run `tools/metal/build.sh configure` and `tools/metal/build.sh build`. Diagnose and fix failures rather than stop at reporting them. Run and verify the resulting upstream reference viewer on M1 Max.
3. Capture baseline provenance, OpenGL call census and reference functionality/performance; write actual measurements, then implement native Metal presentation and renderer functionality in code. Track work in `work/active/metal-port.md` and continue autonomously.

## Last verified engineering change
Harness-only overlay created. No viewer code changed or compiled in the authoring environment. Replace this statement with actual source revisions and successful/failed commands as work proceeds.
