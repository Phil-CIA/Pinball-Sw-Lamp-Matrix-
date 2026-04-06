# Solo Git Workflow (simple + Copilot-friendly)

Goal: keep `main` as "known good" while still doing messy bring-up work.

## Branches
- `main`:
  - Always buildable / readable
  - Contains only the code + schematic you would ship
- `bringup-*` branches:
  - Temporary test routines, prints, one-off experiments
  - Examples:
    - `bringup-switch-inputs`
    - `bringup-row-drivers`
    - `bringup-2026-04-06`

## Daily workflow (solo)
1. Create a branch before experiments
2. Commit small checkpoints often
3. When a block works:
   - merge back to `main` using **Squash merge** (clean history), or
   - cherry-pick only the "final" commits into `main`

## Why do this even solo?
- prevents "one giant file full of test junk"
- lets you keep a lab notebook history without polluting `main`
- makes it easier for Copilot (and future-you) to understand the project