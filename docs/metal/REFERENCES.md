# External engineering references

Use these sources for API semantics, not as a replacement for local source inspection or hardware testing.

## OpenAI / Codex conventions
- Repository-as-source-of-truth and concise `AGENTS.md`: https://openai.com/index/harness-engineering/
- Repository-local skills and GitHub Actions: https://developers.openai.com/blog/skills-agents-sdk
- Avoid instructions/context bloat: https://developers.openai.com/blog/rethinking-skills-and-prompts-for-gpt-6-astra
- Codex prompting and autonomous delivery: https://developers.openai.com/cookbook/examples/gpt-5/codex_prompting_guide
- Codex app/server and execution limitations: https://openai.com/index/unlocking-the-codex-harness/

## Apple
- Metal documentation: https://developer.apple.com/metal/
- Apple GPU storage modes and unified memory: https://developer.apple.com/documentation/metal/choosing-a-resource-storage-mode-for-apple-gpus
- Programmatic capture: https://developer.apple.com/documentation/xcode/capturing-a-metal-workload-programmatically
- Xcode Metal frame capture: https://developer.apple.com/documentation/xcode/capturing-a-metal-workload-in-xcode

## Alchemy / compliance
- Public upstream: https://github.com/AlchemyViewer/Alchemy
- Upstream build instructions: https://github.com/AlchemyViewer/Alchemy/blob/develop/doc/BUILD.md
- Upstream architecture: https://github.com/AlchemyViewer/Alchemy/blob/develop/doc/ARCHITECTURE.md
- Upstream tests: https://github.com/AlchemyViewer/Alchemy/blob/develop/doc/TESTING.md
- Linden Lab third-party viewer policy: https://secondlife.com/corporate/third-party-viewers

Always check current API availability and installed device family rather than hardcode a 2026 SDK assumption.
