# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

A Vulkan 1.4 + Wayland graphics application written in C. Renders geometry using dynamically-compiled Slang shaders. Includes a 2D rendering layer (g2d), a CPU-side raster image processing library with PPM I/O, a pluggable allocator, a GUI layer (stb_truetype), and an embedded Scheme REPL (s7).

## Build

No Makefile. The build uses per-module shell scripts that produce static libraries.

```sh
# Build the main application (compiles all modules, links into ./cdr)
./build.sh            # output defaults to ./cdr
./build.sh mybin      # custom output name

# Run
./cdr assets/shader.spv
```

Each module under `src/` has a `build.sh` that calls `src/tools/buildlib.sh` to produce a `.a`. The top-level `build.sh` calls `src/tools/buildbin.sh`, which sources each module's build script, collects link flags, and links the final binary.

```sh
# Compile unit tests (unity build pattern)
gcc -DTESTING src/wayland/wayland.c src/wayland/xdg-shell-client-protocol.c \
    src/raster.c src/io.c src/vulkan/tests/test_vulkan.c \
    -I. -lvulkan -lwayland-client -o vulkan_test

gcc src/wayland/xdg-shell-client-protocol.c \
    src/wayland/tests/test_wayland.c -I. -lwayland-client -o wayland_test
```

```sh
# Compile Slang shaders to SPIR-V
./1.4.341.1/x86_64/bin/slangc assets/shader.slang -target spirv -o assets/shader.spv
```

The Vulkan SDK is bundled at `1.4.341.1/x86_64/`.

## Architecture

### Module Structure

```
include/          public API headers (defs.h, display.h, graphics.h, graphics2D.h, mem.h, repl.h)
src/core/         allocator (mem.c), raster processing, PPM I/O, utils (memfill)
  include/        internal headers: debug.h (OK/NNUL/IF macros), arn.h (arena), io.h, raster.h
src/vulkan/       Vulkan context, pipeline, typed GPU memory, frame management
src/wayland/      Wayland display layer, XDG shell glue
src/g2d/          2D rendering (rects) over the gfx layer, with embedded SPIR-V headers
src/gui/          GUI layer using stb_truetype
src/repl/         Embedded Scheme REPL (s7)
src/tools/        Build helpers: buildlib.sh, buildbin.sh, emitspv (SPIR-V to C header)
src/main.c        Application entry point — wires display, graphics, pipeline, and render loop
```

### Key Abstractions

**Display** (`dpyctx`/`wlctx` — same pointer): Wayland window. `dpyinit` -> `dpypoll` loop -> `dpyterm`. Callbacks for resize/close via `dpyset_onrsz`/`dpyset_onclose`.

**Graphics** (`gfxctx`/`vkctx` — same pointer): Vulkan context with typed resources:
- `gfxmem_dev` / `gfxmem_host` — typed GPU memory pools (implicit default pool, or user-allocated)
- `gfxbuf` — vertex/index buffers bound to memory
- `gfximg` — images (render targets, textures)
- `gfxppl` — shader pipeline (SPIR-V + vertex bindings + push constants)
- `gfxpass` — render pass with output images
- Command recording: `gfxcmd_begin` -> `gfxcmd_draw` -> `gfxcmd_end` -> `gfxsubmit` -> `gfxpresent`

**2D Layer** (`g2dctx`): Immediate-mode rect rendering over gfx. Owns its pipeline with embedded SPIR-V. `g2dinit` -> `g2dupload` -> `g2ddraw` per frame.

**Allocator** (`mem.h`): `cdralloc`/`cdrfree` with pluggable backend via `cdrset_alloc`. Default is `calloc`/`free`.

### Type System (`include/defs.h`)

Custom typedefs: `u8/u16/u32/u64`, `i8/i16/i32/i64`, `f32/f64`, `usize/isize`. Vector types: `t{N}{T}` (e.g. `t4u8`, `t3f32`) with `.r/.g/.b/.a` or `.x/.y/.z/.w` fields. Raster buffers: `struct raster_{N}{T}` with `buf`, `stride`, `pxw`, `pxh`, `pxfmt`.

### Error Handling (`src/core/include/debug.h`)

Three macros, always used instead of raw `if`:
- `OK(expr, DO)` — runs `DO` if `expr < 0`
- `NNUL(expr, DO)` — runs `DO` if `expr` is null
- `IF(expr, DO)` — runs `DO` if `expr` is truthy

Pair with `goto err`/`goto exit` for cleanup. Declare all variables before the first goto target.

### Vulkan Design
- Dynamic rendering (no VkRenderPass objects), synchronization2 for barriers
- Validation layer always enabled
- rBAR detection: tries DEVICE_LOCAL|HOST_VISIBLE, falls back to staging
- `#ifndef TESTING` guards `main()` in vulkan.c for unit test builds
- Shader entry points: `vertMain` / `fragMain`

## Conventions

- **Formatting**: `x= value` (no space before `=`), Allman braces, 80-col limit, ASCII only in source
- **Naming**: `<module><verb>_<specifier>` grammar (e.g. `gfxinit_buf`, `dpyset_onrsz`). Queries use `qry` as a verb (`gfxqry_init`, `dpyqry_init`). Event handlers use `_h` suffix internally.
- **Public/internal split**: module-prefixed functions (e.g. `gfxinit_buf`) are thin bridges to `static` internal functions (e.g. `init_buf`)
- **All internal functions are `static`**; tests use unity build (`#include "module.c"`) to access them
- **"All zeroed == dead" invariant**: `alive` flag, `memset` before `free` in term functions
- **Test files**: marked `// AI-generated (Claude)` at the top
