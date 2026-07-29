# CafeGLSL compiler

`glslcompiler.elf` is built from CafeGLSL commit `01f8575` with
`patches/0001-preserve-explicit-uniform-block-bindings.patch` applied. The patch is
required because the upstream GLSL interface-block lowering path otherwise uses
Mesa's internal uniform-block array index as the physical GX2 constant-buffer
bank. Runtime reflection retains the authored `layout(binding = N)` value, so
the unpatched compiler makes reflected bindings disagree with shader bytecode.

## Rebuild

Use a Debian 12 environment with `meson`, `ninja`, `bison`, `flex`,
`python3-mako`, `python3-setuptools`, and `zlib1g-dev` installed:

```sh
git clone https://github.com/Exzap/CafeGLSL.git
cd CafeGLSL
git checkout 01f8575
git apply /path/to/0001-preserve-explicit-uniform-block-bindings.patch
meson setup build-host \
    -Db_sanitize=none \
    -Dtools=glsl \
    -Dvulkan-drivers= \
    -Dgallium-drivers=r600 \
    -Db_lundef=false \
    -Dglx=disabled \
    -Degl=disabled \
    -Dplatforms= \
    -Dllvm=disabled \
    --buildtype=release
ninja -C build-host cafecompiler/glslcompiler.elf
```

Copy `build-host/cafecompiler/glslcompiler.elf` into this directory and run
`CafeGlslCompilerIntegrationTests` before using it for Wii U builds.
