# Reproducible blockers only

No Mac-local implementation blockers have been reproduced yet. Do **not** turn unknowns into blockers. Codex should reproduce/fix environment issues and continue on independent engineering work.

## Repository connection (harness authoring only)
- Observed: linked GitHub installation could read a few `LOCAL_USER/*` repos but `levdarkheart-lgtm/AlchemyFork` returned GitHub 404. No PR/push was made.
- Impact: this harness is delivered as a locally applicable overlay/patch. **This is not a blocker for Codex working on the user's existing Mac clone.**
- Unblock for future ChatGPT GitHub writes: grant the connector's installation repository access to `levdarkheart-lgtm/AlchemyFork` (or connect the GitHub identity owning it).

For each new actual blocker, record: `ID`, source SHA, host, exact command/exit code, observed error, suspected cause (labeled), attempted solutions, next independent work, and any owner input strictly required. Remove or mark resolved when proven fixed.
