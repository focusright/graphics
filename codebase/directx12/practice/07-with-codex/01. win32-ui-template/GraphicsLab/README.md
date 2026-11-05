# GraphicsLab — Win32 + Direct3D 12 research shell

A small, standalone, native Windows graphics-experiment application. The first experiment is a rotating, interpolated-color triangle. The window, sliders, checkbox, combo box, menu, and status bar are native Win32 controls; a child `HWND` hosts a D3D12 swap chain.

## Requirements to **build**

- **Visual Studio 2026**, with the Desktop development with C++ workload and MSVC **v145** tools
- A Windows 10/11 SDK with the legacy **FXC** shader compiler (`fxc.exe`)
- x64 build machine

Open `GraphicsLab.slnx`, choose **Release | x64**, then **Build Solution**. The executable will be under `bin/x64/Release/GraphicsLab.exe`. Select **Debug | x64** for `/MTd` and optional D3D12 debug-layer messages.

The project selects the latest installed Windows 10/11 SDK with `WindowsTargetPlatformVersion=10.0`. Shader byte arrays are generated **during the build** with the Windows SDK's `fxc.exe`, then compiled into the application. If your SDK no longer contains FXC, install a Windows SDK version that provides it (build machine only). Nothing shader-related is needed on the user's machine.

The shader target uses the resolved `TargetPlatformVersion` for the SDK directory. `WindowsSDK_ExecutablePath_x64` is a search-path list, so it cannot be used as a single directory. Native x64 compiler tools are preferred. For command-line builds, use the x64 MSBuild at `MSBuild\Current\Bin\amd64\MSBuild.exe` under the Visual Studio installation, with `GraphicsLab.slnx /m /p:Configuration=Release /p:Platform=x64` (or `Configuration=Debug`).

## Requirements to **run**

- Windows 10 or Windows 11 (x64)
- The Windows-provided D3D12/DXGI components; an appropriate GPU driver, or the system's WARP software rasterizer
- No Visual C++ Redistributable, .NET, Windows App SDK, DirectX Agility SDK, installer, or shader compiler

The Release build uses **`/MT`**. Only the **`.exe`** is intended for distribution via GitHub Releases. A `.pdb` might also be built for development, but is not needed by users. The application uses OS-provided DLLs; it does not bundle them.

**Compatibility scope:** Windows 10 and 11 desktop releases; Windows 10 22H2 and current Windows 11 are the primary validation targets. D3D12 hardware capabilities vary; the program attempts WARP when hardware D3D12 is unavailable. Compatibility must be tested on actual target installations before a public release.

## UI

- **Rotation speed:** 0.00–4.00 radians per second
- **Animate:** pause/resume without losing the current angle
- **Background color:** five clear colors
- **Reset:** restore settings and triangle angle
- **Help > About:** basic application info

Frame animation is time-based and the swap chain uses vertical synchronization. The renderer deliberately waits for GPU completion each frame to keep synchronization simple and easy to audit, rather than introducing pipelined frame allocators into this initial teaching example.

## Code map

| Source | Responsibility |
| --- | --- |
| `main.cpp` | Unicode entry point, DPI awareness, fatal error dialog |
| `Win32App.h/.cpp` | Window, native controls, UI events, message/render loop |
| `D3D12Renderer.h/.cpp` | Device, WARP fallback, DXGI swap chain, resizing, fences, drawing |
| `TriangleExperiment.h/.cpp` | Triangle pipeline, upload buffer, animation, draw commands |
| `Experiment.h` | Experiment parameters |
| `Errors.h` | HRESULT diagnostics |
| `shaders/*.hlsl` | Vertex and pixel shaders |

## Replacing the triangle with another experiment

1. Replace or extend `TriangleExperiment` with the new technique's GPU resources and recorded commands.
2. Add experiment parameters to `ExperimentParams`, then native controls/events in `Win32App`.
3. Add new HLSL files and compile them in the `CompileShaders` MSBuild target. Include the generated headers in your C++ source.
4. Reuse `D3D12Renderer` for device initialization, viewport resizing, back-buffer transitions, and presentation.

Keep the host simple rather than growing an editor framework. The experiment itself owns its shaders, pipeline, and experiment-specific resources.

## Release validation checklist

Debug and Release x64 were built with Visual Studio 2026 / MSVC v145 on 2026-11-05. The application rendered on the hardware GPU; pause, rotation speed, background selection, reset, and maximized viewport rendering were exercised. Release imports were inspected with `dumpbin /dependents` and `/imports`: only `d3d12.dll`, `dxgi.dll`, `COMCTL32.dll`, `USER32.dll`, `GDI32.dll`, and `KERNEL32.dll` were listed. The Release executable was 421,376 bytes. This does not establish compatibility with clean Windows 10/11 installations or validate the WARP path. GPU debug messages were not captured.

On a Windows machine with Visual Studio 2026:

1. Build **Debug | x64** and **Release | x64**.
2. Run and test slider, checkbox, combo, reset, window resizing, minimize/restore, and multi-monitor DPI behavior.
3. Run `dumpbin /dependents bin\x64\Release\GraphicsLab.exe` and `dumpbin /imports bin\x64\Release\GraphicsLab.exe`.
4. Confirm there are no imports from `vcruntime140.dll`, `msvcp140.dll`, or other redistributable runtime libraries.
5. Test the single Release EXE on clean Windows 10 22H2 and Windows 11 installations **without** Visual Studio or the VC++ Redistributable.
6. Publish only the validated `GraphicsLab.exe` to GitHub Releases.

## Style and licensing

C++ with K&R braces and a modern, straightforward code style. No third-party source code or runtime libraries are included. Code in this sample is offered under the MIT license (see `LICENSE`). Windows/D3D12 remain subject to Microsoft's platform terms.
