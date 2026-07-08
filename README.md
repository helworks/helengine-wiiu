# Helengine Wii U Host

This repository contains the Wii U platform host and builder integration for Helengine.

## Build

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform wiiu `
  -Output ..\helprojs\city\wiiu-build
```

## Run In Emulator

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 `
  -ArtifactPath ..\helprojs\city\wiiu-build\helengine_wiiu.wuhb
```

The current packaged startup target is the authored `colored_cube_grid` scene. After building the `city` project, launch the generated WUHB through `launch_in_emulator.ps1` and verify that `colored_cube_grid` boots successfully in Cemu. The output root also keeps the generated RPX for lower-level diagnostics.

## More Docs

- [Docker Build Notes](docs/Docker.md)
- [Platform Notes](docs/PlatformNotes.md)
