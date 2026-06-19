# Wii U Docker Build

Use the local Docker image when you need to build the Wii U host directly.

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
