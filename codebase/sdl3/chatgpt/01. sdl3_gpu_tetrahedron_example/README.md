# SDL3 GPU Orbiting Tetrahedron (Windows / D3D12)

This is a minimal **SDL3 GPU** example project that renders a tetrahedron with four differently colored faces.

## Controls

- **Middle mouse button drag**: orbit camera around the tetrahedron
- **Mouse wheel**: zoom in/out
- **Close window / Alt+F4**: quit

## What this example demonstrates

- Creating an SDL3 GPU device with the **Direct3D 12** backend
- Claiming an SDL window for GPU presentation
- Creating a graphics pipeline for the swapchain format
- Uploading dynamic vertex data every frame with a transfer buffer
- Rendering a simple 3D object with an orbit camera

## Important note

This packaged sample is intentionally **Windows-only** and uses the SDL3 GPU **D3D12 backend** with **runtime HLSL compilation to DXBC** through `D3DCompile`. That keeps the example small and avoids a separate offline shader compiler step.

## Build requirements

- **Windows 10 or newer**
- **Visual Studio 2022** with C++ workload
- **CMake 3.24+**
- Internet access during the first configure step so CMake can fetch **SDL 3.4.2**

## Build

From a Developer PowerShell for VS 2022:

```powershell
cd path\to\sdl3_gpu_tetrahedron_example
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Run

```powershell
.\build\Release\sdl3_gpu_tetrahedron_example.exe
```

The post-build step copies `SDL3.dll` next to the executable automatically.

## Project layout

```text
sdl3_gpu_tetrahedron_example/
  CMakeLists.txt
  README.md
  src/
    main.cpp
```
