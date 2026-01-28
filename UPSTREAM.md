# Vanilla vs Enhanced: Recommended Two-Tree Workflow

This repository started as a modernization of the original Linux DOOM 1.10 sources, but it now also contains behavior-changing enhancements (gameplay/UI/network features). If you want a clean separation between “the original port” and “further development”, use one of the approaches below.

## Option A (Recommended): Two Branches + `git worktree`

This keeps the repository history simple while allowing you to work on both trees simultaneously.

1. Create (or designate) a **vanilla** branch:
   - Import or reset it to the unmodified Linux DOOM 1.10 baseline.
   - Keep it “no enhancements” (only minimal build fixes if you must, and document them).

2. Keep enhanced development on an **enhanced** branch (recommended name: `enhanced`).

3. Check out both at once:
```sh
git worktree add ../doom-vanilla vanilla
git worktree add ../doom-enhanced enhanced
```

Suggested initialization (one-time):
```sh
# Use 6aedbff ("Not wanted stuff removed") as the last known "vanilla" cut point.
git branch vanilla 6aedbff

# Make `enhanced` your main development branch.
git branch enhanced HEAD
```

Suggested policy:
- **Vanilla**: correctness/build-only fixes, no gameplay/network/UI feature changes.
- **Enhanced**: everything else (SDL2 modernization, new features, balance, UX, security, etc.).

## Option B: Two Directories in One Repo (Archive-Only Vanilla)

If you don’t want multiple branches, keep a snapshot directory like:
- `vanilla/linuxdoom-1.10/` (archived reference, not built)
- `linuxdoom-1.10/` (enhanced, built by CMake)

This avoids branch juggling but duplicates sources and makes syncing harder.

## Notes

- Legacy DOS/serial/IPX utilities are archived under `legacy/` and are not built by CMake (e.g. `legacy/ipxsrc/`).
- The top-level CMake build remains SDL2-only and should continue to build a single `linuxdoom` target by default.
