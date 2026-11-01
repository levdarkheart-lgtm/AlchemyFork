# Matched live-water pair checkpoint — 2026-11-01

**Observed host:** physical Apple M1 Max.  
**Candidate source:** `fd0fa7cea56e3a9ba933f1405a6be68e9099374e`,
Development build 68026.  
**Reference source:** `8a68422d107600f0074923eb2191f915e742e8e8`,
Release build 67607.

## Admissibility contract

Actual Computer Use drove both isolated profiles through the same live fixture:
fly, Map teleport to `Grignano (128, 228, 74)`, Midday environment, and a fresh
2048x1476 PNG Snapshot with UI and HUD excluded and post processing enabled.
The coordinate teleport replaced an earlier manual-flight attempt, which was
discarded rather than scored.

The post-exit fail-closed graphics comparison was admissible: 416/416 effective
settings matched, with no mismatch, missing required setting, or setting present
on only one side. Candidate and reference camera/focus X/Y values were identical
within serialized precision; Z differed by about 0.010216 m. Both profiles
recorded flying state at exit. The candidate executable SHA-256 was
`d33603803c5d45434d06e0332e14980dc095505b060624f09143a41f434e1d4e`;
the reference executable SHA-256 was
`fac5225435df2c096d10b856f6e0f186b37e41446576398ee8f4842b884ef28d`.

The candidate image SHA-256 was
`281bea0256613faa05013b489aab720829e6d10d659bd9059e31726107f9ea0d`;
the reference image SHA-256 was
`3c32b8f7266806d3041026ea100823e3a91085b6c4caedad229ece56c936c2fb`.
Raw authenticated images, profiles, logs, and the 416-setting manifests remain
local and untracked under `/private/tmp`.

## Comparison result

Human visual inspection found the images materially the same. Exact ffmpeg
whole-frame scoring reported SSIM 0.870878 and PSNR 26.392383 dB; these raw
metrics include temporally changing water/cloud detail and were retained as
descriptive evidence, not used as a false pixel-identity gate. A half-resolution
Gaussian-blurred structural comparison reported SSIM 0.993616. The bounded
left water/sky crop (`1024x520+0+240`, scaled to 512x260 and blurred with sigma
8) reported SSIM 0.998397.

The commands were:

```sh
ffmpeg -hide_banner -nostats -i <candidate.png> -i <reference.png> -lavfi ssim -f null -
ffmpeg -hide_banner -nostats -i <candidate.png> -i <reference.png> -lavfi psnr -f null -
ffmpeg -hide_banner -nostats -i <candidate.png> -i <reference.png> -filter_complex '[0:v]scale=1024:738,gblur=sigma=8[c];[1:v]scale=1024:738,gblur=sigma=8[r];[c][r]ssim' -f null -
ffmpeg -hide_banner -nostats -i <candidate.png> -i <reference.png> -filter_complex '[0:v]crop=1024:520:0:240,scale=512:260,gblur=sigma=8[c];[1:v]crop=1024:520:0:240,scale=512:260,gblur=sigma=8[r];[c][r]ssim' -f null -
```

Both viewers exited cleanly. Candidate logged `LogoutReply`, `Goodbye!`, and
stopped status; reference logged `Goodbye!` and stopped status. The final
viewer/helper census was zero and launch-validation variables were cleared.

## Product boundary

This accepts live parity for one fixed Midday Grignano water/default-probe
fixture and corroborates the source correction. It does not establish moving
water breadth, every environment/probe configuration, mirrors, projector or
cascade shadows, transparency/material replay, non-default post, advanced
output, M-008 completion, native-only completion, or a performance advantage.
No more manual replay is warranted for this accepted fixture. The priority
returns to the next source-demonstrated M-008 divergence and an asymmetric
physical regression before another live pair.
