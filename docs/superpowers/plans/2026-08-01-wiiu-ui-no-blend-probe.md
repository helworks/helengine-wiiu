# Wii U UI No-Blend Hardware Probe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Wii U diagnostic WUHB that disables blending only for the existing slot-zero textured UI probe.

**Architecture:** Strengthen the existing source-contract test so it isolates the slot-zero helper and requires a zero target blend-enable mask. Change that helper's mask from `0x1` to `0x0`; captured-frame UI rendering and every other probe input remain unchanged.

**Tech Stack:** C++17, WUT GX2, C# xUnit source-contract tests, HelEngine Wii U build pipeline.

---

### Task 1: Specify the probe-only no-blend contract

**Files:**
- Modify: `builder.tests/WiiURuntimeSourceTests.cs`
- Test: `builder.tests/helengine.wiiu.builder.tests.csproj`

- [ ] **Step 1: Isolate the slot-zero helper in the existing test**

In `RuntimeSeam_SelectsTexturedUiSlotZeroHardwareProbe`, extract only `RenderDiagnosticUiSlotZeroToColorBuffer` and require blending to be disabled there while retaining the normal captured UI blend assertion elsewhere:

```csharp
int slotZeroStartIndex = presenterSource.IndexOf("void WiiUGx2Presenter::RenderDiagnosticUiSlotZeroToColorBuffer(", StringComparison.Ordinal);
int diagnosticSquareStartIndex = presenterSource.IndexOf("void WiiUGx2Presenter::RenderDiagnosticSquareToColorBuffer(", slotZeroStartIndex, StringComparison.Ordinal);
Assert.True(slotZeroStartIndex >= 0, "The presenter must define the UI slot-zero target helper.");
Assert.True(diagnosticSquareStartIndex > slotZeroStartIndex, "The diagnostic square helper must follow the UI slot-zero helper.");
string slotZeroSource = presenterSource.Substring(slotZeroStartIndex, diagnosticSquareStartIndex - slotZeroStartIndex);

Assert.Contains("GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x0, FALSE, TRUE);", slotZeroSource, StringComparison.Ordinal);
Assert.DoesNotContain("GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x1, FALSE, TRUE);", slotZeroSource, StringComparison.Ordinal);
Assert.Contains("GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x1, FALSE, TRUE);", presenterSource, StringComparison.Ordinal);
```

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --no-restore --filter FullyQualifiedName~RuntimeSeam_SelectsTexturedUiSlotZeroHardwareProbe -v minimal
```

Expected: FAIL because the slot-zero helper still contains blend mask `0x1` rather than `0x0`.

### Task 2: Disable blending only for the slot-zero probe

**Files:**
- Modify: `src/platform/wiiu/WiiUGx2Presenter.cpp`
- Test: `builder.tests/WiiURuntimeSourceTests.cs`

- [ ] **Step 1: Make the single production change**

Inside `WiiUGx2Presenter::RenderDiagnosticUiSlotZeroToColorBuffer`, change only:

```cpp
GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x1, FALSE, TRUE);
```

to:

```cpp
GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x0, FALSE, TRUE);
```

Do not alter `RenderQuadCommandsToColorBuffer`, which must retain blend mask `0x1` for captured UI.

- [ ] **Step 2: Run the focused test and verify GREEN**

Run:

```powershell
dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --no-restore --filter FullyQualifiedName~RuntimeSeam_SelectsTexturedUiSlotZeroHardwareProbe -v minimal
```

Expected: PASS with one test and zero failures.

- [ ] **Step 3: Run the renderer regression subset**

Run:

```powershell
dotnet test builder.tests/helengine.wiiu.builder.tests.csproj --no-restore --filter "FullyQualifiedName~RuntimeSeam_SelectsTexturedUiSlotZeroHardwareProbe|FullyQualifiedName~RuntimeSeam_SelectsCapturedPresentationAfterForegroundLifecycleRepair|FullyQualifiedName~RuntimeSeam_SelectsBatchedUiQuadsWithBaseVertex|FullyQualifiedName~RuntimeSeam_PlacesHardwarePresentationResourcesInRequiredHeaps" -v minimal
```

Expected: PASS with four tests and zero failures.

### Task 3: Build and identify the no-blend WUHB

**Files:**
- Produce: `C:/dev/helprojs/demodisc/wiiu-build/helengine_wiiu.wuhb`

- [ ] **Step 1: Build the Demo Disc**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\dev\helworks\helengine\scripts\build-platform.ps1 -Project C:\dev\helprojs\demodisc\project.heproj -Platform wiiu -Output C:\dev\helprojs\demodisc\wiiu-build
```

Expected: exit code 0 and a freshly timestamped `helengine_wiiu.wuhb`.

- [ ] **Step 2: Record artifact identity**

Run:

```powershell
Get-Item C:\dev\helprojs\demodisc\wiiu-build\helengine_wiiu.wuhb | Select-Object FullName,Length,LastWriteTime
Get-FileHash C:\dev\helprojs\demodisc\wiiu-build\helengine_wiiu.wuhb -Algorithm SHA256
```

Expected: a nonzero file, current build timestamp, and SHA-256 hash.

- [ ] **Step 3: Observe both real-hardware targets**

Deploy the WUHB without taking a screenshot. Record TV and GamePad results as cyan, black, unchanged lilac, or different. Cyan or black proves fragment output and redirects investigation to texture alpha/content; unchanged lilac redirects investigation to pre-blend shader geometry; different targets redirects investigation to target context state.

The overlapping diagnostic source files already contain intentional uncommitted hardware-debug work, so execution must not create a mixed source commit. The design and plan documents remain separately committed.
