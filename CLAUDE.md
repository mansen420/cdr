# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

A Vulkan + Wayland graphics application written in C. Renders a triangle using dynamically-compiled Slang shaders. Also contains a CPU-side raster image processing library with PPM I/O.

## Build

There is no Makefile. Compile manually from the project root:

```sh
# Compile the main Vulkan+Wayland application
gcc src/vulkan/vulkan.c src/wayland/wayland.c src/wayland/xdg-shell-client-protocol.c \
    src/raster.c src/io.c \
    -I. -lvulkan -lwayland-client -o cdr
```

```sh
# Compile a test (e.g. ppmread)
gcc test/ppmread.c src/io.c src/raster.c -I. -o ppmread_test
```

```sh
# Compile Slang shaders to SPIR-V (requires the bundled Slang compiler)
./1.4.341.1/x86_64/bin/slangc assets/shader.slang -target spirv -o assets/shader.spv
```

Run the application (pass the compiled SPIR-V path as argv[1], defaults to `./assets/shader.spv`):
```sh
./cdr assets/shader.spv
```

The Vulkan SDK is bundled at `1.4.341.1/x86_64/`. Headers and tools (slangc, glslc, etc.) are in `1.4.341.1/x86_64/include/` and `1.4.341.1/x86_64/bin/`.

## Architecture

### Type System (`include/defs.h`)
All code uses the custom typedefs from `defs.h`: `u8/u16/u32/u64`, `i8/i16/i32/i64`, `f32/f64`, `usize/isize`. Vector types follow the pattern `t{N}{T}` (e.g. `t4u8` = 4-channel u8, `t3f32` = 3-channel f32) with named fields accessible as `.r/.g/.b/.a` or `.x/.y/.z/.w`.

Raster buffers use typed structs: `struct raster_4u8`, `struct raster_3u8`, `struct raster_3f32`, `struct raster_4f32` — each holding a `buf`, `stride`, `pxw`, `pxh`, and `enum pxfmt`.

### Display Layer (`include/display.h` / `src/wayland/wayland.c`)
The `dpyctx` is an opaque handle to the Wayland connection. API:
- `dpyinit(title, w, h, flags)` — create window
- `dpypoll(ctx)` — dispatch events; returns `-1` on close
- `dpygetnwin(ctx)` / `dpygetncon(ctx)` — get native Wayland `wl_surface` / `wl_display` pointers (passed to Vulkan surface creation)
- `dpyterm(ctx)` — destroy

Wayland event handlers are defined directly in `wayland.c`. The handler functions use `NNUL`/`OK` macros defined at the top of that file.

### Graphics Layer (`include/graphics.h` / `src/vulkan/vulkan.c`)
The `gfxctx` is an opaque handle to the Vulkan context. API:
- `gfxinit(dpy)` — takes a `dpyctx *` and creates instance, surface, device, swapchain, command pools
- `gfxterm(ctx)` — destroys all Vulkan objects in reverse order

Internally, the Vulkan state is `vkctx` (typedef'd as both `gfxctx` and `vkctx`). Key design choices:
- Uses **dynamic rendering** (VK_KHR_dynamic_rendering / Vulkan 1.3) — no render pass objects
- Uses **VK_KHR_synchronization2** for pipeline barriers (`vkCmdPipelineBarrier2`, `vkQueueSubmit2`)
- Targets **Vulkan 1.4** API
- Validation layer (`VK_LAYER_KHRONOS_validation`) is always enabled
- Graphics and present queues may be the same (`gfxprs == 1`) or separate
- Frame contexts (`frmctx`) hold one command buffer, two semaphores (image acquired + render finished), and a fence; one per swapchain image

The pipeline is created by `mkgfxppl(ctx, spv_code, byte_len)` — takes SPIR-V bytecode, creates shader modules and a graphics pipeline. Vertex/fragment entry points must be named `vertMain` / `fragMain`.

### Raster Library (`src/raster.c`, `src/raster.h`)
CPU image processing operations:
- `upsample_4u8` / `upsample_3u8` — nearest-neighbor upscaling by integer factor
- `dnsamplebx_4u8` / `dnsamplebx_3u8` — box-filter downscaling by integer factor
- `rgba2planar` (inline) — interleaved RGBA → planar `[RR...GG...BB...AA...]`

### I/O (`src/io.c`, `src/io.h`)
Binary PPM read/write:
- `fwritebppm` / `prnbppm` — write PPM
- `fprsbppmhdr` + `freadbppm` — parse header then read pixels (must call in order)

### Shaders (`assets/`)
Shaders are written in **Slang** (`.slang` files). Both vertex and fragment stages can coexist in a single file using `[shader("vertex")]` / `[shader("fragment")]` attributes. Compiled to SPIR-V (`.spv`) using the bundled `slangc`.

### Error Handling Pattern
Two macros used throughout:
- `OK(expr, DO)` — execute `DO` if `expr < 0` (used for Vulkan `VkResult`)
- `NNUL(expr, DO)` — execute `DO` if `expr` is null
