# Wii U Platform Notes

## Current Milestone

- The shared editor CLI can build Wii U outputs with platform id `wiiu`.
- The first editor-driven artifact is a raw `helengine_wiiu.rpx` suitable for direct Cemu launch verification.
- The repository still keeps the lower-level native build for bring-up and proof-of-life boot checks.

## Generated Core Seam

The native build reserves `HELENGINE_CORE_CPP_ROOT` for later `cs2.cpp` integration, but the first milestone does not compile generated core output yet.

## Boot Check

Load `build/helengine_wiiu.rpx` in Cemu. The expected result for this milestone is a stable solid red frame with no immediate crash or return to the menu.

## Cube Test Verification

Build the `city` project through the shared editor CLI, then launch the resulting RPX through `scripts/launch_in_emulator.ps1`. The expected result for this slice is that the authored `cube_test` scene is the packaged startup target and the RPX still launches through the checked-in Cemu workflow.
