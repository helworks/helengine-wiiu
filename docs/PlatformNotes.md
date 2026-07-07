# Wii U Platform Notes

## Current Milestone

- The shared editor CLI can build Wii U outputs with platform id `wiiu`.
- The editor-driven output now stages both `helengine_wiiu.rpx` and `helengine_wiiu.wuhb`, plus a `content` tree that mirrors the packaged runtime payloads.
- The repository still keeps the lower-level native build for bring-up and proof-of-life boot checks.

## Generated Core Seam

The native build reserves `HELENGINE_CORE_CPP_ROOT` for later `cs2.cpp` integration, but the first milestone does not compile generated core output yet.

## Boot Check

Load `build/helengine_wiiu.rpx` in Cemu. The expected result for this milestone is a stable solid red frame with no immediate crash or return to the menu.

## Cube Test Verification

Build the `city` project through the shared editor CLI, then launch the resulting WUHB through `scripts/launch_in_emulator.ps1`. The expected result for this slice is that the authored `cube_test` scene is the packaged startup target and the checked-in Cemu workflow boots the packaged content-backed artifact successfully.
