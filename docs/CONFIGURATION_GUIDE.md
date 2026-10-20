# Configuration & Hardware Tuning Guide (`config.ini`)

This guide explains every setting in [`config.ini`](../config.ini) and provides recommended presets for running the PS5 Low-Spec Translation Layer across three hardware tiers below standard emulator requirements.

---

## 1. Configuration Parameters Reference

### `[CPU]` Section
| Key | Default (6-Core) | Description |
| :--- | :--- | :--- |
| `EnableDirectNCE` | `true` | Executes decrypted Zen 2 x86-64 guest code directly on the host CPU instead of using a software JIT recompiler. |
| `PatchTlsAtLoadTime` | `true` | Binary-patches guest `fs:[disp32]` Thread-Local Storage reads at load time to avoid costly Windows VEH page/privilege faults. |
| `FiberWorkerThreads` | `5` | Number of host OS threads executing guest pthreads via M:N cooperative fibers. Set to `PhysicalCores - 1`. |
| `ReservedFlipAudioCore` | `1` | Reserves 1 physical core exclusively for GPU command submission and WASAPI audio mixing to prevent frame pacing stutter. |

### `[Memory]` Section
| Key | Default (4GB VRAM / 16GB RAM) | Description |
| :--- | :--- | :--- |
| `SparseDemandPaging64KB` | `true` | Uses Win32 `VirtualAlloc` (`MEM_RESERVE`) to reserve the 16 GB guest address space with `0 MB` initial physical commitment, committing 64 KB pages on demand. |
| `VramCeilingMB` | `3072` | Maximum GPU `DEVICE_LOCAL` memory budget in MB. Set to `1536` on 2 GB GPUs, `3072` on 4 GB GPUs (e.g., GTX 1650), or `4608` on 6 GB GPUs. |
| `RamCommitBudgetMB` | `8928` | Maximum physical system RAM working set before aggressive cold-page eviction triggers. |
| `EnableZstdColdPageSwap` | `true` | Compresses guest memory pages untouched for `ColdPageAgeFrames` frames in system RAM (~2.85:1 compression ratio) and decommits their raw 64 KB pages. |
| `ColdPageAgeFrames` | `90` | Number of frames (1.5 seconds at 60 FPS) before an inactive page is classified as cold. Lower to `45` on 8 GB RAM PCs. |
| `DropTextureMipLevels` | `1` | Automatically strips top mip levels on non-UI 2D textures $\ge 1024 \times 1024$. `1` = Drop Mip 0 (**-75% texture VRAM**); `2` = Drop Mip 0 & Mip 1 (**-93.75% texture VRAM**). |

### `[GPU]` Section
| Key | Default (GTX 1650 4GB) | Description |
| :--- | :--- | :--- |
| `Backend` | `Vulkan_1_2` | Graphics API backend (`Vulkan_1_2` or `DirectX12_FL11_1`). `Vulkan_1_2` is recommended for sparse residency and lower driver overhead. |
| `InternalResolutionScale` | `0.67` | Internal 3D render target multiplier (`0.50` = 540p, `0.67` = 720p, `0.83` = 900p, `1.00` = 1080p). UI layers remain at native 1080p. |
| `EnableFsr1CasUpscaler` | `true` | Applies single-pass AMD FidelityFX Super Resolution 1.0 (EASU + RCAS) to upscale sub-native 3D targets to 1080p in $<0.18\text{ ms}$. |
| `ConvertNggToComputeCull` | `false` | Rewrites RDNA 2 NGG Primitive Shaders into standard Compute Culling + Indirect Draw passes. Set to `true` on Pascal (GTX 10-series), Polaris (RX 500), or Intel UHD iGPUs. |
| `StubHardwareRayTracing` | `true` | Replaces `image_bvh64_intersect_ray` instructions with an immediate miss/baked-probe return. **Must be `true` on all non-RTX GPUs (GTX 1650, GTX 1060, RX 580).** |
| `AsyncPipelineCompilation` | `true` | Compiles SPIR-V/DXIL pipelines on background worker threads to eliminate shader compilation stutter on 4–6 core CPUs. |
| `SkipHeavyPostProcessing` | `true` | Bypasses Bokeh Depth of Field, Per-Object Motion Blur, and half-res Volumetric Fog dispatches. |
| `FrameRateLimit` | `60` | Frame rate cap (`30`, `60`, or `120`). Use `30` on 4-core CPUs or 2 GB VRAM GPUs for consistent frame pacing. |

### `[IO_And_Audio]` Section
| Key | Default | Description |
| :--- | :--- | :--- |
| `MaxKrakenDecompressionMsPerFrame` | `3.5` | Maximum CPU time slice (in ms) allowed for software Oodle Kraken decompression per frame before deferring non-critical LOD chunks. |
| `EnableLooseAssetDiskCache` | `true` | Caches decompressed Kraken blocks on disk to bypass CPU decompression on subsequent loads. |
| `BypassTempest3dHrtf` | `true` | Bypasses 512-tap FFT HRTF 3D audio convolution in favor of constant-power SIMD stereo panning. |
| `WasapiAudioBufferMs` | `32` | Audio ring buffer size in milliseconds. Increase to `48` on 4-core CPUs if audio crackles during heavy scene transitions. |

---

## 2. Recommended Hardware Tier Presets

### Preset A: Ultra-Low-Spec (4-Core CPU / 8 GB RAM / 2 GB VRAM e.g. GTX 1050 / Integrated GPU)
```ini
[CPU]
FiberWorkerThreads = 3
ReservedFlipAudioCore = 1

[Memory]
VramCeilingMB = 1536
RamCommitBudgetMB = 4096
ColdPageAgeFrames = 45
DropTextureMipLevels = 2

[GPU]
InternalResolutionScale = 0.50
RenderResolution = 960x540
ConvertNggToComputeCull = true
StubHardwareRayTracing = true
FrameRateLimit = 30
```

### Preset B: Balanced Low-Spec (6-Core CPU / 16 GB RAM / 4 GB VRAM e.g. i7-9750H + GTX 1650 / RX 570)
```ini
[CPU]
FiberWorkerThreads = 5
ReservedFlipAudioCore = 1

[Memory]
VramCeilingMB = 3072
RamCommitBudgetMB = 8928
ColdPageAgeFrames = 90
DropTextureMipLevels = 1

[GPU]
InternalResolutionScale = 0.67
RenderResolution = 1280x720
ConvertNggToComputeCull = false
StubHardwareRayTracing = true
FrameRateLimit = 60
```
