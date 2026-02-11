# Frank Luna D3D12 Book – Reference for Dojo

This file summarizes key terminology and patterns from *Introduction to 3D Game Programming with DirectX 12* (Luna, 2016) for use when building the dojo. The full book is in this folder as PDF. **Use as reference scaffolding only; do not import Luna's framework wholesale.** The dojo stays minimal and destructible.

**Luna book source code location (in this repo):** `docs/luna/d3d12book-codeonly/`
- **Common/** – shared code: d3dApp.h/cpp, d3dUtil.h/cpp, UploadBuffer.h, GameTimer, Camera, GeometryGenerator, MathHelper, d3dx12.h, DDSTextureLoader
- **Chapter N /** – per-chapter demos; many include local FrameResource.h/cpp (e.g. Chapter 7 Shapes, LandAndWaves; Chapter 20 Shadows)
- Base D3DApp uses a single command allocator and FlushCommandQueue() each frame (Ch4 Init); from Ch7 onward demos use FrameResource with per-frame allocator + fence (no global flush per frame).

---

## Execution & Sync (Layer 1)

- **Command queue / command lists / command allocators**
  - CPU submits commands via command lists; commands are stored in the **allocator**.
  - **Invariant:** Allocator must NOT be reset until the GPU has finished executing all commands that reference it. Otherwise: undefined behavior, debug layer errors, or crash.
- **Fence**
  - `ID3D12Fence` + monotonically increasing value. Used to mark a point on the GPU timeline and to wait on the CPU until the GPU reaches that point.
  - **FlushCommandQueue pattern:** Increment fence → `Signal` on queue → `Wait` on CPU (event). Used in Ch4 Init demo; book notes this is inefficient per-frame and defers proper frame ring to later.
- **Frame ring (Luna: “FrameResource” in Ch7)**
  - Per-frame resources: one allocator (or set of allocators) per swap-chain buffer index; fence value per frame; reuse allocator only after the fence shows the GPU has finished that frame.
  - Dojo Belt 1: no global WaitForGpu per frame; fence per frame index; allocator reset only after fence completion.

---

## Resource & Memory (Layer 2)

- **Heaps**
  - **Upload (D3D12_HEAP_TYPE_UPLOAD):** CPU-writable, GPU-readable; for constant buffers, staging for copies to default.
  - **Default (D3D12_HEAP_TYPE_DEFAULT):** GPU-only; for vertex/index buffers, textures, depth buffers. CPU upload via CopyQueue or copy from upload buffer.
- **UploadBuffer (Luna Ch6)**
  - Wrapper: committed resource in upload heap, Map once, `CopyData(elementIndex, data)`. Used for constant buffers and any CPU→GPU upload. **Do not overwrite while GPU is still reading** (synchronize with fence / frame ring).
- **Constant buffer alignment**
  - Size and offset must be multiple of 256 bytes. Luna: `CalcConstantBufferByteSize`: `(byteSize + 255) & ~255`.
- **Resource transitions (barriers)**
  - e.g. back buffer: `PRESENT` → `RENDER_TARGET` (before clear/draw), then `RENDER_TARGET` → `PRESENT` (before Present). Copy: `COPY_DEST` for upload, then `VERTEX_BUFFER` (or appropriate read state) before draw.
- **Upload → default**
  - Create buffer in default heap; create upload buffer; record Copy from upload to default; transition default resource to correct read state before use.

---

## Fixed-Function Pipeline (Layer 3)

- **Viewport / scissor**
  - Set each frame (e.g. after command list reset). `RSSetViewports`, `RSSetScissorRects`.
- **RTV / DSV**
  - Clear RTV and DSV at start of pass. `OMSetRenderTargets` binds RTV(s) and DSV.
- **PSO (ID3D12PipelineState)**
  - Aggregates root signature, VS/PS (and optional HS/DS/GS), input layout, rasterizer, depth/stencil, blend, RTV/DSV formats, sample desc. Create at init; minimize PSO switches per frame.
- **Rasterizer state**
  - FillMode (solid vs wireframe), CullMode (back/front/none). Luna: wireframe = `D3D12_FILL_WIREFRAME`, no cull = `D3D12_CULL_NONE`.

---

## Programmable / Shaders (Layer 4)

- **Root signature**
  - Defines what is bound before draw: descriptor tables, root descriptors (CBV), root constants. Must match shader `register` bindings (b0, t0, s0, etc.).
- **Descriptor tables**
  - Contiguous range in a descriptor heap. `SetGraphicsRootDescriptorTable(index, gpuHandle)`.
- **CBV in HLSL**
  - `cbuffer name : register(b0)`. Constant buffers in upload heap; 256-byte alignment.
- **Luna shader compilation**
  - `D3DCompileFromFile` (or offline FXC); vs_5_0/ps_5_0; debug flags in Debug builds.

---

## Chapter Mapping (Dojo Belts)

| Dojo belt / topic      | Luna chapters / concepts                    |
|------------------------|---------------------------------------------|
| Belt 0 (clear screen)  | Ch4: Init, swap chain, RTV, allocator, fence, barriers, clear |
| Belt 1 (frame ring)   | Ch4.2.2 sync; Ch7 FrameResource (per-frame allocator + fence) |
| Belt 2 (upload arena) | Ch6.6 Upload buffer; upload heap; copy to default; transitions |
| Belt 3 (triangle)     | Ch6: VB, IA layout, root sig, PSO, Draw*    |
| Shadow mapping        | Ch20: depth pass, orthographic projection, PCF, bias         |

---

## Debug / Instrumentation

- **Debug layer:** Enable in Debug builds (`EnableDebugLayer()`). Sends messages to debug output; catches many sync and state errors.
- **Luna error handling:** `ThrowIfFailed` macro; `DxException` with HRESULT, function name, file, line. Dojo can use similar minimal pattern.

---

## Luna Code Layout (d3d12book-codeonly)

**Path:** `dojo/docs/luna/d3d12book-codeonly/`

### Common
- **d3dApp:** One allocator in base; `FlushCommandQueue()` = mCurrentFence++, Signal(mFence), wait event until GetCompletedValue() >= mCurrentFence. SwapChainBufferCount = 2.
- **d3dUtil:** ThrowIfFailed, DxException, CalcConstantBufferByteSize, CreateDefaultBuffer (default + upload heap, COMMON→COPY_DEST→GENERIC_READ), CompileShader. MeshGeometry, SubmeshGeometry, Light, Material, Texture.
- **UploadBuffer:** Template; UPLOAD heap, Map, CopyData; 256-byte align for CB; do not write while GPU uses (fence/FrameResource).
- **FrameResource** (per-chapter): CmdListAlloc, PassCB, ObjectCB (UploadBuffer), Fence. "Cannot reset allocator until GPU done."

### Frame ring (ShapesApp)
- Update: advance mCurrFrameResourceIndex; wait if mCurrFrameResource->Fence not completed (SetEventOnCompletion, WaitForSingleObject); update CBs.
- Draw: cmdListAlloc = mCurrFrameResource->CmdListAlloc; Reset allocator; Reset command list; record; ExecuteCommandLists; mCurrFrameResource->Fence = ++mCurrentFence; Signal(mFence).

### Ch4 Init
Single allocator; each frame Reset allocator/list, barriers, clear, Present, then FlushCommandQueue (inefficient).

### Ch20 Shadows
ShadowMap: depth texture R24G8_TYPELESS, SRV+DSV, Viewport/Scissor; depth pass then sample in main pass.

---

*Generated for dojo; anchor dojo steps to these terms and patterns without copying Luna’s full framework.*
