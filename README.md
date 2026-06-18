# Helengine Wii U Host

This repository contains the native Wii U host scaffold, the Wii U platform builder integration, and the Wii U-specific runtime source audits for Helengine.

## Current milestone

- The shared editor CLI can build Wii U outputs with platform id `wiiu`.
- The first editor-driven artifact is a raw `helengine_wiiu.rpx` suitable for direct Cemu launch verification.
- The repository still keeps the lower-level Docker build for native bring-up and proof-of-life boot checks.

## Editor CLI build

If your workspace keeps `helengine-wiiu`, `helengine`, and `helprojs` as sibling directories, use the shared wrapper like this:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform wiiu `
  -Output ..\helprojs\city\wiiu-build
```

That wrapper runs the main editor CLI with `--build wiiu` and writes the generated Wii U output to the output directory you provide.

## Launching in Cemu

Use the checked-in launcher script:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_wiiu_rpx_in_cemu.ps1 `
  -RpxPath ..\helprojs\city\wiiu-build\helengine_wiiu.rpx
```

The launcher requires an explicit `-RpxPath`. Before launch it force-closes any running `Cemu.exe` processes, recreates a dedicated Cemu user directory under `tmp\`, prints the RPX path and last write time, and prints the spawned process id.

It then starts Cemu with `-g <rpx>`.

The script fails fast when:

- `-RpxPath` is missing
- the RPX file is missing
- the Cemu executable is missing

## Low-level Docker build

```bash
docker build -t helengine-wiiu .
docker run --rm -v "$PWD":/workspace -w /workspace helengine-wiiu make
```

If Docker Desktop's credential helper blocks anonymous pulls on this machine, use:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-wiiu .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-wiiu make
```

The build emits `build/helengine_wiiu.elf`, `build/helengine_wiiu.rpx`, and `build/helengine_wiiu.wuhb`.

## Generated core seam

The native build reserves `HELENGINE_CORE_CPP_ROOT` for later `cs2.cpp` integration, but the first milestone does not compile generated core output yet.

## Boot check

Load `build/helengine_wiiu.rpx` in Cemu. The expected result for this milestone is a stable solid red frame with no immediate crash or return to the menu.

## Cube Test Verification

Build the `city` project through the shared editor CLI:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform wiiu `
  -Output ..\helprojs\city\wiiu-build
```

Launch the resulting RPX in Cemu:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_wiiu_rpx_in_cemu.ps1 `
  -RpxPath ..\helprojs\city\wiiu-build\helengine_wiiu.rpx
```

Expected result for this slice:

- the authored `cube_test` scene is the packaged startup target
- the generated Wii U packaged bootstrap resolves `cooked/scenes/rendering/cube_test.hasset`
- the RPX still launches through the checked-in Cemu workflow
