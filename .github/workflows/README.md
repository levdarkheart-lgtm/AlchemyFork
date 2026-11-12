# Manual release builds for this fork

Automatic build/test workflows are disabled in repository settings, including
inherited schedules on `main`. The Metal branch has only manual dispatch entry
points. No push, tag push, pull request, or timer builds a viewer.

Use **tag-release.yaml** (Build Apple Silicon release), selecting
`codex/apple-silicon-metal` in **Run workflow**. `source_ref` accepts the Metal
branch, a tag, or a full commit SHA containing these native release tools. For
an immutable build, paste the exact commit SHA. Choose a new alpha number;
`release_tag` can be omitted to generate `VERSION.REVISION-alphaN`.

From GitHub CLI:

```sh
gh workflow run tag-release.yaml --repo levdarkheart-lgtm/AlchemyFork \
  --ref codex/apple-silicon-metal \
  -f source_ref=codex/apple-silicon-metal -f alpha_number=5
```

This is one standard `macos-26` ARM64 job with two compile jobs. It verifies the
source/tag identity before the expensive build, preserves the accepted pinned
OpenHTJ2K decoder, checks harness/static/package integrity, and publishes a new
prerelease with a ZIP, SHA-256 file and build provenance. It never overwrites an
existing release. App bytes are not duplicated into Actions artifact storage.

`metal-harness.yml` and `lint-workflows.yaml` are optional manual checks, kept
disabled at the repository level until explicitly wanted. The harness installs
`tools/metal/requirements.txt`; a bare runner does not supply its NumPy dependency.
Do not re-enable legacy workflows from `main`: their old scheduled definitions
are intentionally disabled. Dependabot's service-generated update workflow is
separate; GitHub rejects disabling it through the Actions workflow API.
