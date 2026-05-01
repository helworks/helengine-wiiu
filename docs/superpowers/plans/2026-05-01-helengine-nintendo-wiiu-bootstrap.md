# Nintendo Wii U Bootstrap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first `helengine-wiiu` Dockerized Wii U homebrew scaffold that targets Wii U only and boots in Cemu to a stable solid red screen with no input dependency.

**Architecture:** Use the established `helengine-*` scaffold shape, but follow the current `wut` Makefile-based Wii U toolchain flow instead of the libogc-style Wii/GameCube flow. Keep the platform boundary explicit by having `main.cpp` delegate immediately to `WiiUBootHost`, preserve the generated-core seam, and use Wii U native screen/proc APIs for the first visible frame.

**Tech Stack:** Docker, devkitPro `devkitPPC`, `wut`, GNU Make, C++17, Wii U coreinit/WHB APIs, Cemu

---

## File Structure

- Create: `Dockerfile`
  Purpose: define the Wii U build container on top of `devkitpro/devkitppc:latest` and install the official `wiiu-dev` package group.
- Create: `Makefile`
  Purpose: build Wii U homebrew using the current `wut_rules` flow, Wii U only compile macros, and the generated-core seam.
- Create: `README.md`
  Purpose: document the Docker build flow, output artifact names, and the expected Cemu red-screen verification result.
- Create: `src/main.cpp`
  Purpose: enter the Wii U bootstrap and hand off immediately to `WiiUBootHost`.
- Create: `src/platform/wiiu/WiiUBootHost.hpp`
  Purpose: declare the Wii U specific boot host interface and helper methods for buffer allocation and frame presentation.
- Create: `src/platform/wiiu/WiiUBootHost.cpp`
  Purpose: initialize the Wii U process/screen path, allocate the TV and GamePad screen buffers, clear them to red, and keep the frame alive.

### Task 1: Confirm The Current Wii U Toolchain Surface

**Files:**
- Research only

- [ ] **Step 1: Confirm the official Wii U package and helper names from primary sources**

Run:

```bash
rtk printf '%s\n' \
  'Official Wii U facts to preserve in the scaffold:' \
  '1. devkitPro installs Wii U support via the wiiu-dev package group' \
  '2. Makefile builds include $(DEVKITPRO)/wut/share/wut_rules' \
  '3. CMake builds use /opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake and wut_create_rpx(target)'
```

Expected: you have the current Wii U build facts written down before creating repo files.

- [ ] **Step 2: Inspect the active container layout rather than guessing paths**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm devkitpro/devkitppc:latest sh -lc 'find /opt/devkitpro -maxdepth 5 \( -name wut_rules -o -name powerpc-eabi-cmake -o -name "libwut*" \) | sort'
```

Expected: you know whether the base image already contains `wut` or whether the Dockerfile must install `wiiu-dev` explicitly.

- [ ] **Step 3: Record the concrete Wii U build facts before scaffolding**

Capture these facts for implementation:

```text
- whether devkitpro/devkitppc:latest already contains wut
- exact path to wut_rules
- exact path to powerpc-eabi-cmake if present
- output extension(s) produced by the official make-based flow (.rpx and/or .wuhb)
- minimum libraries needed for an OSScreen + WHBProc bootstrap
```

Expected: later tasks can write repo files without old Wii/GameCube assumptions leaking in.

### Task 2: Create The Wii U Build Skeleton

**Files:**
- Create: `Dockerfile`
- Create: `Makefile`

- [ ] **Step 1: Inspect the official sample-style wut Makefile pattern before adapting it**

Run:

```bash
rtk printf '%s\n' \
  'Reference the official wut make helloworld sample pattern:' \
  '- include $(DEVKITPRO)/wut/share/wut_rules' \
  '- TARGET defaults to project name' \
  '- BUILD stores object files' \
  '- LIBS starts with -lwut'
```

Expected: the scaffold preserves the official Wii U build entry points while adapting the repo layout to `src/` and `src/platform/wiiu/`.

- [ ] **Step 2: Write the Wii U Dockerfile**

Create `Dockerfile` with:

```dockerfile
FROM devkitpro/devkitppc:latest

RUN dkp-pacman -Syu --noconfirm --needed wiiu-dev

ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITPPC=/opt/devkitpro/devkitPPC
ENV PATH=/opt/devkitpro/devkitPPC/bin:/opt/devkitpro/tools/bin:/opt/devkitpro/portlibs/wiiu/bin:${PATH}

WORKDIR /workspace
```

- [ ] **Step 3: Write the Wii U Makefile header and toolchain wiring**

Create `Makefile` with:

```makefile
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=/opt/devkitpro")
endif

HELENGINE_CORE_CPP_ROOT ?=
TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

TARGET := helengine_wiiu
BUILD := build
SOURCES := src src/platform/wiiu
DATA :=
INCLUDES := src
CONTENT :=
ICON :=
TV_SPLASH :=
DRC_SPLASH :=
```

- [ ] **Step 4: Add the Wii U only compile/link configuration**

Append to `Makefile`:

```makefile
CFLAGS := -g -Wall -O2 -ffunction-sections $(MACHDEP)
CFLAGS += $(INCLUDE) -D__WIIU__ -D__WUT__
CXXFLAGS := $(CFLAGS) -std=gnu++17
ASFLAGS := -g $(ARCH)
LDFLAGS := -g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map)
LIBS := -lwut

ifeq ($(strip $(HELENGINE_CORE_CPP_ROOT)),)
CFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_CORE=0
CXXFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_CORE=0
else
CFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_CORE=1
CXXFLAGS += -DHELENGINE_WIIU_HAS_GENERATED_CORE=1 -I$(HELENGINE_CORE_CPP_ROOT)
endif

LIBDIRS := $(PORTLIBS) $(WUT_ROOT)
```

- [ ] **Step 5: Add the sample-derived recursive build rules**

Append to `Makefile`:

```makefile
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES :=

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_BIN :=
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SRC)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
	$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
	-I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@rm -fr $(BUILD) $(TARGET).wuhb $(TARGET).rpx $(TARGET).elf

else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).wuhb

$(OUTPUT).wuhb: $(OUTPUT).rpx
$(OUTPUT).rpx: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)
endif
```

- [ ] **Step 6: Dry-run the build surface before adding runtime code**

Run:

```bash
rtk make -n
```

Expected: make prints the recursive `wut_rules`-based build shape and references `helengine_wiiu.elf`, `helengine_wiiu.rpx`, and `helengine_wiiu.wuhb` instead of any Wii/GameCube-specific tools.

- [ ] **Step 7: Commit the build scaffold**

Run:

```bash
rtk git add Dockerfile Makefile
rtk git commit -m "Add Wii U Docker build scaffold"
```

Expected: commit succeeds with only the Wii U build files staged.

### Task 3: Add The Wii U Boot Host

**Files:**
- Create: `src/main.cpp`
- Create: `src/platform/wiiu/WiiUBootHost.hpp`
- Create: `src/platform/wiiu/WiiUBootHost.cpp`

- [ ] **Step 1: Write the minimal entry point**

Create `src/main.cpp` with:

```cpp
#include "platform/wiiu/WiiUBootHost.hpp"

int main() {
    return helengine::wiiu::WiiUBootHost::Run();
}
```

- [ ] **Step 2: Declare the Wii U boot host interface**

Create `src/platform/wiiu/WiiUBootHost.hpp` with:

```cpp
#pragma once

#include <cstdint>

#include <coreinit/screen.h>

namespace helengine::wiiu {
    class WiiUBootHost {
    public:
        static int Run();

    private:
        static void* AllocateScreenBuffer(OSScreenID screen);
        static void ClearScreenBuffers();
        static void PresentScreenBuffers();
        static uint32_t BuildRedColor();
    };
}
```

- [ ] **Step 3: Implement the Wii U process and screen bootstrap**

Create `src/platform/wiiu/WiiUBootHost.cpp` with:

```cpp
#include "platform/wiiu/WiiUBootHost.hpp"

#include <cstring>

#include <coreinit/memdefaultheap.h>
#include <whb/proc.h>

namespace helengine::wiiu {
    namespace {
        void* TvBuffer = nullptr;
        void* DrcBuffer = nullptr;
    }

    int WiiUBootHost::Run() {
        WHBProcInit();
        OSScreenInit();

        TvBuffer = AllocateScreenBuffer(SCREEN_TV);
        DrcBuffer = AllocateScreenBuffer(SCREEN_DRC);
        if (TvBuffer == nullptr || DrcBuffer == nullptr) {
            WHBProcShutdown();
            return 1;
        }

        OSScreenSetBufferEx(SCREEN_TV, TvBuffer);
        OSScreenSetBufferEx(SCREEN_DRC, DrcBuffer);
        OSScreenEnableEx(SCREEN_TV, true);
        OSScreenEnableEx(SCREEN_DRC, true);

        while (WHBProcIsRunning()) {
            ClearScreenBuffers();
            PresentScreenBuffers();
        }

        OSScreenShutdown();
        WHBProcShutdown();
        return 0;
    }

    void* WiiUBootHost::AllocateScreenBuffer(OSScreenID screen) {
        const uint32_t bufferSize = OSScreenGetBufferSizeEx(screen);
        void* buffer = MEMAllocFromDefaultHeapEx(bufferSize, 0x100);
        if (buffer != nullptr) {
            std::memset(buffer, 0, bufferSize);
        }

        return buffer;
    }

    void WiiUBootHost::ClearScreenBuffers() {
        const uint32_t red = BuildRedColor();
        OSScreenClearBufferEx(SCREEN_TV, red);
        OSScreenClearBufferEx(SCREEN_DRC, red);
    }

    void WiiUBootHost::PresentScreenBuffers() {
        OSScreenFlipBuffersEx(SCREEN_TV);
        OSScreenFlipBuffersEx(SCREEN_DRC);
    }

    uint32_t WiiUBootHost::BuildRedColor() {
        return 0xFF000000;
    }
}
```

- [ ] **Step 4: Check include paths and symbol names before container verification**

Run:

```bash
rtk sed -n '1,220p' src/main.cpp
rtk sed -n '1,260p' src/platform/wiiu/WiiUBootHost.hpp
rtk sed -n '1,320p' src/platform/wiiu/WiiUBootHost.cpp
```

Expected: `main.cpp` includes `platform/wiiu/WiiUBootHost.hpp`, the namespace is consistently `helengine::wiiu`, and the buffer helpers match the declarations.

- [ ] **Step 5: Build in Docker to verify the boot host compiles and links**

Run:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-wiiu .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-wiiu make
```

Expected: the container emits `helengine_wiiu.elf`, `helengine_wiiu.rpx`, and `helengine_wiiu.wuhb`.

- [ ] **Step 6: Commit the Wii U bootstrap code**

Run:

```bash
rtk git add src/main.cpp src/platform/wiiu/WiiUBootHost.hpp src/platform/wiiu/WiiUBootHost.cpp
rtk git commit -m "Add Wii U red-screen bootstrap"
```

Expected: commit succeeds with only the new source files staged.

### Task 4: Document The Verified Flow

**Files:**
- Create: `README.md`

- [ ] **Step 1: Write the README with the exact Docker build flow**

Create `README.md` with:

````md
# helengine-wiiu

Nintendo Wii U bootstrap scaffold for Helengine.

## Build

```bash
docker build -t helengine-wiiu .
docker run --rm -v "$PWD":/workspace -w /workspace helengine-wiiu make
```

If Docker credential-helper issues prevent anonymous pulls, use:

```bash
DOCKER_CONFIG=/tmp/docker-no-creds docker build -t helengine-wiiu .
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-wiiu make
```

## Output

The build emits:

- `helengine_wiiu.elf`
- `helengine_wiiu.rpx`
- `helengine_wiiu.wuhb`

## Generated core seam

`HELENGINE_CORE_CPP_ROOT` is reserved for future generated core integration. This milestone only builds the native Wii U host scaffold.

## Boot check

Load `helengine_wiiu.rpx` in Cemu. The expected result is an immediate stable solid red screen with no input dependency.
````

- [ ] **Step 2: Sanity-check the README against the actual build outputs**

Run:

```bash
rtk sed -n '1,220p' README.md
```

Expected: the README names the same commands and artifact extensions produced by the verified build.

- [ ] **Step 3: Commit the documentation**

Run:

```bash
rtk git add README.md
rtk git commit -m "Document Wii U bootstrap flow"
```

Expected: commit succeeds with only the README staged.

### Task 5: Final Verification And Cleanup

**Files:**
- Verify repository state only

- [ ] **Step 1: Re-run the full container build from a clean workspace**

Run:

```bash
rtk make clean
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-wiiu make clean
DOCKER_CONFIG=/tmp/docker-no-creds docker run --rm -v "$PWD":/workspace -w /workspace helengine-wiiu make
```

Expected: the clean build completes and re-emits the Wii U artifacts without manual intervention.

- [ ] **Step 2: Verify the runtime milestone in Cemu**

Run this manual check:

```text
Load helengine_wiiu.rpx in Cemu.
Expected: immediate stable solid red screen on boot, no controller required, no immediate crash back to the menu.
```

- [ ] **Step 3: Check repository cleanliness before handoff**

Run:

```bash
rtk git status --short
```

Expected: only intended source/docs/build-script changes remain. Generated artifacts should still be ignored or intentionally unstaged.

- [ ] **Step 4: Prepare the final integration checkpoint**

Run:

```bash
rtk git log --oneline --decorate -n 5
```

Expected: the recent history shows the scaffold commits in a clean order for review or squashing later if desired.
