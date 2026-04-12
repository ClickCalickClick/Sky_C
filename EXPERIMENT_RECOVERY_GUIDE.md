# Experiment Recovery Guide

Date: 2026-04-12
Baseline tag: baseline-2026-04-12-pre-color-only

## Purpose

This guide defines a safe workflow for experimenting with gradient visuals while keeping a known-good baseline that can be restored quickly.

## Baseline Assets

- Baseline git tag: `baseline-2026-04-12-pre-color-only`
- Baseline screenshots folder: `analysis/screens/`
- Core files to compare during experiments:
  - `src/c/Sky.c`
  - `src/pkjs/index.js`
  - `package.json`
  - `GRADIENT_ANALYSIS.md`

## Safe Experiment Workflow

1. Confirm current status:

```bash
git status --short
```

2. Create an experiment branch from current work:

```bash
git checkout -b experiment/<short-name>
```

3. Build and validate snapshots after each meaningful change:

```bash
pebble build
pebble install --emulator chalk --vnc
pebble screenshot --emulator chalk --vnc --no-open analysis/screens/chalk_experiment.png
```

Repeat for `basalt`, `emery`, and `gabbro`.

4. Compare against baseline behavior and screenshots before merging.

## Fast Recovery Options

### Option A: Inspect baseline without altering your branch

```bash
git show baseline-2026-04-12-pre-color-only:src/c/Sky.c | head
```

### Option B: Temporary checkout baseline in detached HEAD

```bash
git checkout baseline-2026-04-12-pre-color-only
```

Return to your branch:

```bash
git checkout -
```

### Option C: Create a fresh recovery branch from baseline

This preserves your current experiment branch while giving you a clean restart point.

```bash
git checkout -b recovery/from-baseline baseline-2026-04-12-pre-color-only
```

## Validation Checklist

- Routine 10-minute refresh does not show full loading card.
- Cold start and reconnect still recover with loading UX.
- High-altitude daylight (40 to 55 deg) has visible multi-region sky background.
- Time/footer text remains legible on all color targets.
- No regressions at twilight and deep night checkpoints.

## Notes

- Build artifacts under `build/` may change frequently and are not design regressions by themselves.
- Keep visual experiments small and measurable to simplify rollback and comparison.
