# Active goal — integrated Apple-first engine

Owner explicitly requested reformulation and setting of the goal on2026-11-17. This is the controlling project objective for subsequent work; existing evidence and unfinished requirements remain retained.

## Objective

Complete the remaining Apple-first Alchemy architecture as an integrated native Metal ARM64/macOS engine on the experiment branch, using the retained Home and club evidence to reduce the growing GPU workload and frame stalls. Implement producer-owned immutable scene/resource publication shared across main, shadow and probe views; dependency-safe bounded CPU preparation; useful native visibility and command preparation for actual small Second Life workloads; shared GPU lighting/probe preparation that avoids repeated work; and coordinated frame-wide streaming, upload and resource admission with completion-based retirement on Apple unified memory. Apply shader spilling/register-pressure fixes only where source and compiler/runtime evidence support them. Preserve scene, animation, materials, lighting, projectors, probes, queries, UI, media/voice, asset loading, rendering quality, existing crash repairs and all private evidence. Work in coherent groups: audit source and original behavior, implement, run appropriate correctness/build checks, and stage one exact, verified combined viewer. After each group, pause implementation and run the owner checkpoint when ready: capture login/loading and settlement at Home first, then club teleport/loading and settlement after one or two minutes, preserving the owner's settings. Use those results to select the next group. Judge success by demonstrated whole-viewer FPS, frame pacing, loading, correctness and stability; keep architecture and performance acceptance partial until the required boundaries and owner runtime outcomes are met. Do not launch a new recording before a group is ready or push without explicit owner instruction.

## Current evidence and next group

[Home69233 findings](../../research/probe-candidate-home-checkpoint-69233-20261117.md) reproduce29.690→15.886FPS and main GPU completion wait11.616→43.026ms/frame. New BASE candidate preparation is active with low reuse; its isolated total cost is unknown. Current compiler spill fields remain incomplete. These facts guide priorities but do not replace the full architecture scope.

The next group is being selected from the dependency audit of probe preparation/cache, remaining alpha/lighting consumers and shared scene/admission/command boundaries. Preserve all original probe traversal, weighting, order, accepted caps, quality and failure behavior. No new viewer/profile is running. The prior checkpoint pause is superseded by the owner's explicit active-goal/resume instruction; pause again after a coherent verified group for owner evaluation.

## Goal tool state

The tool `create_goal` was invoked with the full objective above and returned: `cannot create a new goal because this thread has an unfinished goal; complete the existing goal first`. The existing goal is active and unachieved. It has not been marked complete, paused or replaced to bypass that restriction. This document and the linked active ledger record the revised engineering scope; the app's existing goal text was not changed.
