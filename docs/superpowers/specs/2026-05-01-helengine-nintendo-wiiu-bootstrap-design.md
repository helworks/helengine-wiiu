# Nintendo Wii U Bootstrap Design

## Goal

Create the first working `helengine-wiiu` bootstrap that builds in Docker with devkitPro `devkitPPC`, targets Wii U only, and boots in Cemu to an immediate solid red screen with no input dependency.

## Constraints

- The repository should stay structurally close to the other recent Nintendo platform scaffolds.
- The build must be Wii U only, not a dual Wii/Wii U target.
- The first milestone should keep the same generated-core seam used in the other platform repos.
- The first runtime target is intentionally minimal: initialize, display red, and keep running.
- The low-level rendering boundary should remain explicitly Wii U native so future shader and 3D work is not biased toward older Nintendo graphics paths.

## Chosen Approach

Use a shared-shape scaffold with Wii U defaults and a Wii U native rendering boundary.

This keeps the repo layout and build conventions aligned with the established `helengine-*` platform repos while making it clear that the actual graphics/bootstrap layer is Wii U specific from day one. Future code sharing, if any, should happen above that low-level platform boundary rather than by weakening the target now.

## Repository Shape

The initial scaffold should include:

- `Dockerfile`
- `Makefile`
- `README.md`
- `src/main.cpp`
- `src/platform/wiiu/WiiUBootHost.hpp`
- `src/platform/wiiu/WiiUBootHost.cpp`

The file naming should remain platform explicit so later shared engine code can sit above the platform host layer without blurring the boot boundary.

## Build Design

The Docker image should follow the devkitPro Nintendo console pattern:

- Base image: `devkitpro/devkitppc:latest`
- Explicit `DEVKITPRO`, `DEVKITPPC`, and Wii U relevant environment variables as needed by the current toolchain layout
- Explicit tool paths where necessary if the current image no longer guarantees the right `PATH`

The Makefile should be derived from the recent Nintendo scaffolds, but targeted for Wii U:

- Include the current Wii U devkitPro rules shipped in the image
- Set the build macros and flags required for Wii U only
- Link using the Wii U libraries and paths expected by the active devkitPro image
- Emit the normal Wii U homebrew output that Cemu can run

The build should preserve:

- `HELENGINE_CORE_CPP_ROOT ?=`
- `HELENGINE_WIIU_HAS_GENERATED_CORE=0/1`

This keeps the same generated-core override seam used elsewhere without adding generated code now.

## Runtime Design

`main.cpp` should hand off immediately to a Wii U specific boot host.

`WiiUBootHost` should:

- initialize the minimum Wii U video / application path needed for the first visible frame
- configure the first framebuffer or presentation path using the current Wii U homebrew APIs
- clear the visible output to a solid red color
- remain alive in a simple loop so Cemu keeps showing the frame

No controller setup, input polling, audio, filesystem, gameplay, or full shader-driven rendering is required for this milestone.

## Platform Boundary

This repository is Wii U first and Wii U only for now.

That means:

- no Wii compatibility toggle
- no shared Wii / Wii U compile target inside this repo
- no attempt to reuse older Nintendo rendering assumptions at the low-level graphics boundary

Wii U graphics work is expected to become highly platform specific later, especially around shader and 3D pipeline work. The scaffold should leave that path open rather than abstract it away prematurely.

## Verification

Success means:

1. `docker build -t helengine-wiiu .` succeeds
2. `docker run --rm -v "$PWD":/workspace -w /workspace helengine-wiiu make` succeeds
3. the build emits the normal Wii U runnable homebrew artifact for Cemu
4. Cemu boots that artifact and shows a stable solid red screen immediately

## Non-Goals

- Wii compatibility
- controller input
- shared engine runtime code beyond the generated-core seam
- shader-driven rendering for this first milestone
- broader abstraction over the Wii U graphics stack before real 3D work begins
