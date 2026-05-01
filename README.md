# Helengine Wii U Host

This repository contains the native Wii U host scaffold for Helengine.

## Current milestone

- Docker-only build using the official devkitPro `wut` toolchain flow
- Standard Wii U homebrew outputs ending in `.rpx` and `.wuhb`
- First boot check with a solid red screen in Cemu

## Build

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
