# Nintendo Wii U Builder And Player Slice Design

## Goal

Implement the Wii U platform in the same small-step pattern already used for Wii and GameCube:

1. produce a minor Wii U editor-driven build
2. prove that build still launches with the current native Wii U bootstrap
3. introduce the real Wii U runtime seam
4. connect builder output to the Wii U player bootstrap
5. launch the authored `cube_test` scene

## Constraints

- Follow the same repository and workflow patterns already established in `helengine-wii` and `helengine-gc`.
- Use the shared editor CLI through the documented wrapper flow, not an ad hoc Wii U-only build entrypoint.
- Keep the first shipped artifact as a raw launchable `RPX`.
- Keep the current `WiiUBootHost` proof-of-life shape for the first slice.
- Add a checked-in PowerShell launcher script for Cemu in the same spirit as the Wii and GameCube emulator launcher scripts.
- Keep each slice independently verifiable before moving to the next one.

## Chosen Approach

Use incremental parity slices instead of trying to land the full Wii U builder and player path in one pass.

This matches the way Wii and GameCube were brought up: first make the platform buildable through the shared tooling, then prove the native runtime still boots, then add the real runtime seam, then connect packaged content, then switch startup to authored scene content. The main benefit is failure isolation. A broken editor export, a broken Wii U runtime seam, and a broken packaged-scene bootstrap should not be debugged at the same time.

## Platform Pattern To Reuse

The Wii U implementation should copy the established platform split already visible in the sibling repositories:

- editor CLI entrypoint documented in `README.md`
- platform builder assembly and tests for output shaping
- thin `main.cpp` launcher
- platform-owned bootstrap or application seam on the native side
- repo-local emulator launcher script
- focused source-contract or workflow tests for the platform-specific glue

Wii U should not invent a different daily workflow unless the console itself forces it.

## Slice Sequence

### Slice 1: Editor CLI RPX Build Plus Cemu Launcher

Keep the current `WiiUBootHost` behavior intact and make the shared editor CLI capable of producing a Wii U output that launches in Cemu.

This slice should add:

- the minimum Wii U builder integration needed for platform id `wiiu`
- output shaping that emits a launchable `RPX`
- README documentation for the editor CLI build path
- `scripts/launch_wiiu_rpx_in_cemu.ps1`
- contract tests that guard the README and launcher script behavior

Success criteria:

1. the shared wrapper can run `--build wiiu`
2. the output directory contains the expected Wii U launch artifact
3. the Cemu launcher script can start that artifact
4. the existing minimal Wii U bootstrap still runs

### Slice 2: Replace Proof-Of-Life Boot With The Wii U Runtime Seam

Introduce the Wii U runtime/application seam that corresponds to the Wii and GameCube player shape, but keep behavior minimal.

This slice should:

- preserve a thin `main.cpp`
- add the first Wii U-owned runtime boundary beyond `WiiUBootHost`
- keep startup simple enough that failures still point clearly to the runtime seam rather than packaged content

Success criteria:

1. the editor-built `RPX` still launches
2. the Wii U runtime seam replaces the pure proof-of-life boot path
3. the repo still boots without requiring packaged authored content yet

### Slice 3: Connect Builder Output To Wii U Packaged Bootstrap

Wire the builder-produced content into the Wii U player bootstrap using the same contract style already used on Wii and GameCube.

This slice should:

- define the Wii U packaged content root contract
- add the Wii U scene bootstrap boundary
- load runtime manifest or catalog data from the staged Wii U output
- fail fast when required packaged files are missing

Success criteria:

1. the Wii U player consumes staged builder output instead of only native bootstrap code
2. builder and runtime paths agree on content layout
3. missing staged content fails clearly and early

### Slice 4: Boot The Authored `cube_test` Scene

Switch the Wii U startup path to the authored `cube_test` scene after the packaged bootstrap path is already stable.

This slice should:

- stage the minimum cooked content bundle required for the first Wii U scene launch
- point the Wii U startup bootstrap at `cube_test`
- verify that the authored scene loads through the real generated runtime path

Success criteria:

1. the Wii U player launches the authored `cube_test` startup path
2. the build still comes from the shared editor CLI flow
3. Cemu launch stays repo-local and repeatable

## Editor CLI Contract

Wii U should follow the same top-level build workflow already documented for Wii and GameCube:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform wiiu `
  -Output ..\helprojs\city\wiiu-build
```

The README should present that wrapper as the primary day-to-day build path. Lower-level native build details can still exist below it, but they should not replace the editor CLI story.

## Cemu Launcher Design

Add one repo-local launcher script:

- `scripts/launch_wiiu_rpx_in_cemu.ps1`

The launcher should follow the same philosophy as the existing Dolphin launchers:

- require an explicit path parameter for the launch artifact
- fail fast when the artifact is missing
- fail fast when the emulator executable is missing
- print the artifact path and last write time before launch
- use a repo-local dedicated emulator profile under `tmp/`
- keep the launch workflow explicit and reproducible

The script is a developer utility only. It should not build, patch, or inspect the artifact. It should prepare a clean or dedicated Cemu launch environment and then launch the provided `RPX`.

## Testing Strategy

Each slice should add the smallest tests that prove the new contract and nothing more.

Expected test shape:

- builder-side tests for Wii U output shaping and path rules
- source-contract tests for Wii U runtime bootstrap seams
- launcher contract tests for the Cemu script
- README contract tests when the documented workflow becomes part of the developer contract

Runtime or emulator verification should stay focused. The goal is to prove one new layer at a time.

## Verification Flow

The expected developer progression is:

1. build through the shared editor CLI wrapper
2. inspect the Wii U output directory
3. launch the resulting `RPX` through the checked-in Cemu script
4. confirm the current slice-specific success criteria before moving on

This keeps Wii U aligned with the proven Wii and GameCube workflow instead of creating a special-case bring-up path.

## Non-Goals

- jumping directly to full packaged authored-scene boot in the first Wii U slice
- replacing the shared editor CLI with a Wii U-only build workflow
- adding a complex Wii U runtime architecture before the first editor-built `RPX` proof exists
- coupling the Cemu launcher to build execution
- debugging builder, runtime seam, packaged bootstrap, and authored scene loading all in the same initial change
