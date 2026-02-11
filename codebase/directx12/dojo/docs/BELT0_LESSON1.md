# Belt 0, Lesson 1: Boot + Clear

**Layers:** (1) Execution & Sync, (3) Fixed-Function

**Goal:** Get a Win32 window, D3D12 device, command queue, swap chain (double buffer), RTV heap, one command allocator, one command list, one fence, and clear the back buffer to dark blue every frame. Debug layer enabled in Debug builds. This is the baseline for all later belts.

---

## What Was Created

- **src/main.cpp** – `wWinMain`, message pump, resize detection, frame loop: `BeginFrame` → `ClearRenderTarget` (dark blue) → `EndFrame`.
- **src/platform/Win32Window.h, .cpp** – Create/destroy/pump messages; window stores `clientWidth`, `clientHeight`, `running`.
- **src/gfx/DxCheck.h, .cpp** – `DX_CHECK(x)` macro, `DxException` with `ToString()` for HRESULT failures.
- **src/gfx/DxContext.h, .cpp** – Device, queue, swap chain (2 buffers), RTV heap, single allocator, single command list, fence. `DxContext_Init`, `Resize`, `FlushCommandQueue`, `BeginFrame` (allocator/list reset, transition to RENDER_TARGET, set viewport/scissor), `ClearRenderTarget`, `EndFrame` (transition to PRESENT, close, execute, present, advance back buffer index, **FlushCommandQueue**). No depth buffer yet (Belt 0 minimal).
- **dojo.sln, dojo.vcxproj** – x64 Debug/Release, links `d3d12.lib`, `dxgi.lib`.

---

## Build

1. Open **dojo.sln** in Visual Studio.
2. Set configuration to **Debug | x64** (or Release | x64).
3. Build Solution (F7).
4. Run (F5). You should see a window with a **dark blue** client area.

---

## Expected Result

- Window titled “D3D12 Dojo – Belt 0” opens.
- Client area is **dark blue** (0, 0, 0.2, 1).
- Resizing the window resizes the swap chain and RTVs; clear color remains correct.
- Closing the window exits the app cleanly.
- In **Debug**, the D3D12 debug layer is enabled; any D3D12 errors show in the Output window.

---

## Intentional Break (Execution & Sync)

**What to do:** In **DxContext.cpp**, in `DxContext_EndFrame`, **comment out** the call to `DxContext_FlushCommandQueue(ctx)` at the end.

**Why it breaks:** Each frame we reset the command **allocator** in `BeginFrame` and reuse it. The allocator’s memory is still referenced by the command queue until the GPU has finished executing the commands we submitted. If we don’t wait (flush) before the next frame, we reset the allocator while the GPU may still be reading from it.

**What you should see:** After a few frames, the debug layer can report errors about the allocator being reset while in use, and/or visual corruption or a crash. Symptom depends on timing.

---

## Fix

**Restore:** Uncomment `DxContext_FlushCommandQueue(ctx)` at the end of `DxContext_EndFrame`.

**Invariant:** *Do not reset a command allocator until the GPU has finished executing all commands that use it.* The fence + `FlushCommandQueue` enforces that. (In Belt 1 we will avoid a global flush every frame by using a per-frame allocator and waiting only when we wrap around.)

---

## Next

Belt 1: Frame ring – per-frame allocators and fence-per-frame, so we don’t call `FlushCommandQueue` every frame.
