# Helengine Wii U Host

This repository contains the Wii U platform host and builder integration for Helengine.

## Build

```powershell
dotnet run --project ..\helengine\tools\build-waiter\helengine.buildwaiter.csproj -- `
  --output ..\helprojs\city\wiiu-build `
  --require helengine_wiiu.wuhb `
  -- powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\scripts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform wiiu `
  -Output ..\helprojs\city\wiiu-build
```

The Build Waiter returns successfully only after `helengine_wiiu.wuhb` is fresh and non-empty.

When publishing the builder outside its repository, set `HELENGINE_WIIU_REPOSITORY_ROOT` to the absolute path of the real Wii U source checkout. The override must contain `Makefile` and `src/main.cpp`; invalid overrides fail explicitly. Without an override, the established installed assembly layout remains supported.

## Run In Emulator

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\launch_in_emulator.ps1 `
  -ArtifactPath ..\helprojs\city\wiiu-build\helengine_wiiu.wuhb
```

The current packaged startup target is the authored `textured_cube_grid` scene. After building the `city` project, launch the generated WUHB through `launch_in_emulator.ps1` and verify that `textured_cube_grid` boots successfully in Cemu. The output root also keeps the generated RPX for lower-level diagnostics.

## More Docs

- [Docker Build Notes](docs/Docker.md)
- [Platform Notes](docs/PlatformNotes.md)

## Portable source imports

The .NET 9 builder uses the engine's existing portable texture registry and file-backed font rasterizer for source cook work items, retaining authored importer ids and the existing default font pixel request. It no longer loads Windows editor UI assemblies. Serialized textures, embedded atlases, Wii U native layouts and numeric serialization policies retain their existing contracts.
