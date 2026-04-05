# CDR

A graphics engine written in C that renders to a [Wayland](https://wayland.freedesktop.org/) window using the [Vulkan](https://www.vulkan.org/) graphics API. It also includes a CPU-side image processing library and an embedded [Scheme](https://en.wikipedia.org/wiki/Scheme_(programming_language\)) scripting REPL.

Right now, CDR renders a colored triangle to the screen using shaders written in [Slang](https://shader-slang.com/) and compiled to SPIR-V. The architecture is designed to grow into a fuller 2D/GUI engine over time.

## What you'll need

**Platform:** Linux with Wayland (not X11).

**Packages:**

| Dependency | What it is | How to get it (Arch) |
|---|---|---|
| `gcc` | C compiler | `pacman -S gcc` |
| `libwayland-client` | Talks to the Wayland display server | `pacman -S wayland` |
| `libvulkan` | Talks to your GPU | `pacman -S vulkan-icd-loader` |
| Vulkan driver | Your GPU's Vulkan implementation | e.g. `vulkan-radeon`, `vulkan-intel`, `nvidia-utils` |

On Debian/Ubuntu, the equivalents are `build-essential`, `libwayland-dev`, `libvulkan-dev`, and your GPU's Vulkan driver (`mesa-vulkan-drivers` for AMD/Intel).

You will also need the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) installed for headers and the Slang shader compiler (`slangc`). On Arch: `pacman -S vulkan-headers`. You can also download the SDK from LunarG and extract it locally.

## Building

There is no Makefile or CMake. The project uses small shell scripts to build.

From the project root:

```sh
./build.sh
```

This compiles everything and produces an executable called `cdr` in the project root.

Under the hood, `build.sh` calls `src/tools/buildbin.sh`, which compiles each module (`src/core`, `src/wayland`, `src/vulkan`, `src/g2d`, `src/gui`, `src/repl`) into a static library, then links them all together with `src/main.c`. Object files are cached in `.cache/` directories so rebuilds are fast.

To give the output binary a different name:

```sh
./build.sh myapp
```

## Running

```sh
./cdr
```

This opens an 800x600 Wayland window and renders a colored triangle. Close the window to exit.

You can optionally pass a path to a compiled SPIR-V shader:

```sh
./cdr path/to/shader.spv
```

It defaults to `./assets/shader.spv` if no argument is given.

### Compiling shaders

Shaders are written in [Slang](https://shader-slang.com/), a modern shading language. The `slangc` compiler (part of the Vulkan SDK) converts them to SPIR-V bytecode that Vulkan can consume:

```sh
slangc assets/shader.slang -target spirv -o assets/shader.spv
```

The pre-compiled `assets/shader.spv` is included in the repo, so you only need to do this if you modify `assets/shader.slang`.

## Project structure

```
cdr/
├── include/            # Public API headers
│   ├── defs.h          # Type system (u8, f32, vector types, raster structs)
│   ├── display.h       # Window management API
│   ├── graphics.h      # GPU/Vulkan abstraction API
│   ├── graphics2D.h    # 2D rectangle drawing API
│   ├── mem.h           # Pluggable memory allocator
│   └── repl.h          # Scheme REPL interface
│
├── src/
│   ├── main.c          # Application entry point & render loop
│   ├── core/           # Utilities: image I/O, raster ops, memory
│   ├── vulkan/         # Vulkan backend (~1600 lines)
│   ├── wayland/        # Wayland backend (~160 lines)
│   ├── g2d/            # 2D rectangle rendering
│   ├── gui/            # GUI/text rendering (work in progress)
│   ├── repl/           # Embedded s7 Scheme interpreter
│   └── tools/          # Build helper scripts
│
├── assets/
│   ├── shader.slang    # Triangle shader source (Slang)
│   └── shader.spv      # Compiled SPIR-V bytecode
│
├── test/               # Standalone test programs
└── build.sh            # Build entry point
```

## How the code is organized

CDR is split into layers. Each layer has a **public header** in `include/` that defines its API, and an **implementation** in `src/`. Here's how they stack up:

```
  Your application (src/main.c)
        |               |
  Graphics Layer    Display Layer
  (graphics.h)      (display.h)
        |               |
  Vulkan backend    Wayland backend
  (src/vulkan/)     (src/wayland/)
```

**Display layer** (`display.h` / `src/wayland/`) -- Creates a window and delivers events (resize, close). The API is small:
- `dpyinit()` -- open a window
- `dpypoll()` -- check for events
- `dpyterm()` -- close the window

**Graphics layer** (`graphics.h` / `src/vulkan/`) -- Manages the GPU. Handles buffers, pipelines, images, and draw commands. All the Vulkan complexity is hidden behind a clean C API:
- `gfxinit()` -- set up the GPU
- `gfxinit_buf()` -- create a vertex buffer
- `gfxinit_ppl()` -- create a shader pipeline
- `gfxcmd_begin/draw/end()` -- record draw commands
- `gfxsubmit()` / `gfxpresent()` -- send to GPU and display

**Core utilities** (`src/core/`) -- CPU-side helpers:
- `io.c` -- Read/write PPM image files
- `raster.c` -- Upsample and downsample images
- `mem.c` -- Pluggable memory allocator (defaults to malloc/free)

## A tour of main.c

The entry point is a good place to start reading. Here's what it does, step by step:

```c
// 1. Create a Wayland window (800x600, titled "CDR")
OK(dpyinit(&dpy, "CDR", 800, 600, 0), goto exit;)

// 2. Initialize the Vulkan graphics context
OK(gfxinit(&gfx, dpy, (gfxdesc){0}), goto exit;)

// 3. Define a triangle (3 vertices, each with a 2D position and RGB color)
vertex triangle[3] = {
    {.pos= {0.0, -0.5}, .color= {1, 0, 0}},   // top    (red)
    {.pos= {0.5,  0.5}, .color= {0, 1, 0}},   // right  (green)
    {.pos= {-0.5, 0.5}, .color= {0, 0, 1}}};  // left   (blue)

// 4. Upload triangle data to the GPU
OK(gfxinit_buf(gfx, &vtx, NULL, bufdesc, vtxdata), goto exit;)

// 5. Load the compiled shader and create a pipeline
OK(gfxinit_ppl(gfx, &ppl, ...), goto exit;)

// 6. Main loop: poll events, draw, present
while(!quit) {
    dpypoll(dpy);
    gfxcmd_begin(pass);
    gfxcmd_draw(pass, ...);
    gfxcmd_end(pass);
    gfxsubmit(gfx, pass);
    gfxpresent(gfx, fb);
}
```

Every function call is wrapped in `OK(...)`, which is the project's error-handling pattern (explained below).

## Key concepts for newcomers to C

### The type system (`include/defs.h`)

CDR defines its own shorthand types instead of using the verbose standard ones:

| CDR type | Standard C type | Meaning |
|---|---|---|
| `u8`, `u16`, `u32`, `u64` | `uint8_t`, `uint16_t`, ... | Unsigned integers |
| `i8`, `i16`, `i32`, `i64` | `int8_t`, `int16_t`, ... | Signed integers |
| `f32`, `f64` | `float`, `double` | Floating point |
| `usize`, `isize` | `size_t`, `ptrdiff_t` | Pointer-sized integers |

It also defines **vector types** like `t2f32` (2 floats), `t3u8` (3 bytes), `t4f32` (4 floats). These use C unions so you can access the same data as either colors or coordinates:

```c
t3f32 v;
v.r = 1.0;   // same memory as...
v.x = 1.0;   // ...this
```

### Error handling (`src/core/include/debug.h`)

Instead of checking return values with `if` statements everywhere, CDR uses three macros:

```c
OK(some_function(), goto exit;)    // runs "goto exit;" if the return value is negative
NNUL(some_pointer, goto exit;)     // runs "goto exit;" if the pointer is NULL
IF(bad_condition, goto exit;)      // runs "goto exit;" if the condition is true
```

All three print a diagnostic message with the file and line number when triggered. The `goto exit;` pattern is idiomatic C for cleanup -- it jumps to a label at the end of the function where all resources are freed.

### Opaque pointers

You'll see types like `gfxctx *` and `dpyctx *` used without ever seeing their struct definitions. This is the [opaque pointer](https://en.wikipedia.org/wiki/Opaque_pointer) pattern -- the struct internals are hidden in the `.c` file, and you interact with them only through API functions. This keeps the public API clean and lets the implementation change without affecting callers.

### Designated initializers

CDR makes heavy use of C99 designated initializers:

```c
vertex v = {.pos = {.x = 0.5, .y = 0.5}, .color = {.r = 1.0}};
```

This sets specific fields by name (instead of relying on position), which makes the code much more readable.

## Naming conventions

CDR does not use typical `snake_case` naming. Instead, it follows a structured grammar that encodes *what module owns the thing* and *what the thing is* directly into the name. Once you internalize the pattern, you can read any CDR identifier and immediately know what it does and where it belongs.

### The grammar: `<module><verb>_<specifier>`

Every function and type name is built from up to three parts:

```
<module>  <verb>     _<specifier>
  gfx      init        _buf
  dpy      poll
  gfx      cmd         _draw
```

- **Module prefix** -- a short tag (usually 3 letters) that says which subsystem owns this symbol. `gfx` is graphics, `dpy` is display, `cdr` is the core runtime.
- **Verb** -- what the function *does*: `init`, `term`, `cmd`, `get`, `set`, `upload`, etc.
- **Specifier** (optional) -- narrows the verb to a specific object. `gfxinit` initializes the graphics context itself; `gfxinit_buf` initializes a *buffer* within that context.

When there is no specifier, the verb applies to the module's primary object (its context). When there is a specifier, the verb applies to that specific sub-object.

Here's how this plays out across the graphics API:

| Function | Module | Verb | Specifier | Meaning |
|---|---|---|---|---|
| `gfxinit` | gfx | init | *(context)* | Initialize the graphics context |
| `gfxinit_buf` | gfx | init | buf | Initialize a buffer |
| `gfxinit_img` | gfx | init | img | Initialize an image |
| `gfxinit_ppl` | gfx | init | ppl | Initialize a pipeline |
| `gfxinit_pass` | gfx | init | pass | Initialize a render pass |
| `gfxinit_devmem` | gfx | init | devmem | Initialize device memory |
| `gfxinit_hostmem` | gfx | init | hostmem | Initialize host memory |
| `gfxterm` | gfx | term | *(context)* | Terminate/destroy the graphics context |
| `gfxterm_buf` | gfx | term | buf | Terminate a buffer |
| `gfxterm_img` | gfx | term | img | Terminate an image |
| `gfxterm_ppl` | gfx | term | ppl | Terminate a pipeline |
| `gfxterm_pass` | gfx | term | pass | Terminate a render pass |
| `gfxterm_mem` | gfx | term | mem | Terminate a memory allocation |
| `gfxcmd_begin` | gfx | cmd | begin | Begin recording commands |
| `gfxcmd_draw` | gfx | cmd | draw | Record a draw command |
| `gfxcmd_end` | gfx | cmd | end | End recording commands |
| `gfxsubmit` | gfx | submit | *(context)* | Submit recorded commands to the GPU |
| `gfxpresent` | gfx | present | *(context)* | Present a rendered image to the display |
| `gfxupload` | gfx | upload | *(context)* | Upload data to a buffer |
| `gfxmmap` | gfx | mmap | *(context)* | Memory-map host memory |
| `gfxsync` | gfx | sync | *(context)* | Wait for the GPU to finish all work |
| `gfxpause` | gfx | pause | *(context)* | Pause rendering |
| `gfxresume` | gfx | resume | *(context)* | Resume rendering |
| `gfxupdate` | gfx | update | *(context)* | Update context (e.g. on window resize) |
| `gfxqry_init` | gfx | qry | init | Query whether the context is initialized |
| `gfxget_display` | gfx | get | display | Get the current display/swapchain image |

The display layer follows the same pattern:

| Function | Module | Verb | Specifier | Meaning |
|---|---|---|---|---|
| `dpyinit` | dpy | init | *(context)* | Initialize the display (open a window) |
| `dpyterm` | dpy | term | *(context)* | Terminate the display |
| `dpypoll` | dpy | poll | *(context)* | Poll for window events |
| `dpyset_onrsz` | dpy | set | onrsz | Set the resize callback |
| `dpyset_onclose` | dpy | set | onclose | Set the close callback |
| `dpyset_usrdata` | dpy | set | usrdata | Set user data passed to callbacks |
| `dpyget_nwin` | dpy | get | nwin | Get native window handle |
| `dpyget_ncon` | dpy | get | ncon | Get native connection handle |
| `dpyget_err` | dpy | get | err | Get the last error code |
| `dpyget_errstr` | dpy | get | errstr | Get the error string for a code |
| `dpyqry_init` | dpy | qry | init | Query whether the display is initialized |

### The same grammar applies to types

Types follow the pattern `<module><noun>_<specifier>`:

```
gfxctx            -- the graphics context (the primary object)
gfxbuf            -- a graphics buffer
gfximg            -- a graphics image
gfxppl            -- a graphics pipeline
gfxpass           -- a graphics render pass
gfxmem            -- a base memory type
gfxmem_dev        -- specifically device-local GPU memory
gfxmem_host       -- specifically host-visible (CPU-accessible) memory
```

Notice `gfxmem` vs `gfxmem_dev` vs `gfxmem_host`: the base name is the general concept, and the specifier narrows it. `gfxterm_mem` can destroy *any* memory (it takes a `gfxmem *`), while `gfxinit_devmem` and `gfxinit_hostmem` create specific kinds.

### Descriptors: `<module>desc_<what>`

Any struct that *describes how to create or configure something* is a descriptor, named `gfxdesc_<thing>`:

| Type | Describes |
|---|---|
| `gfxdesc` | The graphics context itself (memory sizes, swapchain format, flags) |
| `gfxdesc_buf` | How to create a buffer (element count, stride) |
| `gfxdesc_img` | How to create an image (width, height, format) |
| `gfxdesc_ppl` | How to create a pipeline (shaders, bindings, push constant ranges) |
| `gfxdesc_pass` | How to create a render pass (output images) |
| `gfxdesc_drawcmd` | How to issue a draw call (pipeline, buffers, vertex count, etc.) |
| `gfxdesc_upload` | A data upload (pointer, byte count, offset) |
| `gfxdesc_shader` | A single shader stage (SPIR-V code, entry point, stage bits) |
| `gfxdesc_attr` | A vertex attribute (location, offset, format) |
| `gfxdesc_bufbind` | A buffer binding description (binding index, stride, input rate, attributes) |
| `gfxdesc_pushrange` | A push constant range (size, offset, stage flags) |

Descriptors are always passed **by value** (not by pointer). This is intentional -- they are small, plain-data structs that describe a one-time creation. You fill one in with designated initializers and pass it directly:

```c
OK(gfxinit_img(gfx, &fb, NULL,
    (gfxdesc_img){.w = 800, .h = 600, .fmt = GFXFMT_BGRA8_SRGB}),
  goto exit;)
```

This avoids the boilerplate of declaring a separate variable when you only need the descriptor once. The `(gfxdesc_img){...}` syntax is a C99 **compound literal** -- it creates a temporary struct inline.

### Enums and constants: `GFX<NOUN>_<VALUE>` or `GFXFMT_<VALUE>`

Format enums encode the channel layout and type directly in the name:

```
GFXFMT_2F32       -- 2-component, 32-bit float (e.g. vec2)
GFXFMT_3F32       -- 3-component, 32-bit float (e.g. vec3)
GFXFMT_4U8        -- 4-component, 8-bit unsigned (e.g. RGBA bytes)
GFXFMT_BGRA8_SRGB -- BGRA byte order, 8-bit, sRGB color space
GFXFMT_D32_SFLOAT -- 32-bit signed float depth buffer
```

Stage flags use bitfield naming:

```
GFXSTAGE_VERT = 1 << 0   -- vertex shader stage
GFXSTAGE_FRAG = 1 << 1   -- fragment shader stage
```

These can be OR'd together when something applies to multiple stages.

### The `nr` prefix for counts

Fields that represent "how many" are prefixed with `nr` (short for "number of"):

```c
.nrshaders  = 2      // number of shaders in this pipeline
.nrbindings = 1      // number of buffer bindings
.nrattrs    = 2      // number of vertex attributes
.nrvtx      = 3      // number of vertices to draw
.nrbufs     = 1      // number of buffers
.nroutputs  = 1      // number of output images
.nrinputs   = 0      // number of input images
.nrinst     = 0      // number of instances
.nridx      = 0      // number of indices
```

This is consistent everywhere. If you see `nr` at the start of a field name, it's a count.

### The `init`/`term` lifecycle

Every object follows the same lifecycle: `init` to create, `term` to destroy. These always come in pairs:

```c
gfxinit(&gfx, dpy, desc);     // create
// ... use gfx ...
gfxterm(gfx);                  // destroy

gfxinit_buf(gfx, &buf, ...);  // create
// ... use buf ...
gfxterm_buf(gfx, buf);        // destroy
```

`init` functions take a pointer-to-pointer (`gfxbuf **`) as an output parameter and return `int` (0 on success, negative on error). `term` functions take the context and the object to destroy, and return `void` -- destruction cannot fail.

All `init` functions require the parent context as the first argument, because the object is allocated within that context's memory. All `term` functions also require the context, because the context owns the memory.

### The `qry` verb

`gfxqry_init` and `dpyqry_init` are "query" functions -- they return whether the context has been successfully initialized. The verb `qry` means "ask a yes/no question about the state of something."

### Why this naming scheme?

The naming is designed so that you can:

1. **Autocomplete effectively** -- type `gfx` and your editor shows every graphics function. Type `gfxinit_` and you see every object you can create. Type `gfxterm_` and you see every object you can destroy.
2. **Read code fluently** -- `gfxinit_ppl` reads as "graphics: initialize pipeline." `gfxcmd_draw` reads as "graphics command: draw." `dpyset_onrsz` reads as "display: set on-resize callback."
3. **Predict names** -- once you know the buffer is called `buf`, you can guess `gfxinit_buf`, `gfxterm_buf`, `gfxdesc_buf` without looking them up.
4. **Avoid collisions** -- the module prefix means `gfxinit` and `dpyinit` coexist without conflict, and you always know which module you're calling into.

## Running tests

Tests are standalone C programs in the `test/` directory. Compile and run them individually:

```sh
# PPM read test
gcc test/ppmread.c src/core/io.c src/core/raster.c src/core/utils.c -I. -lm -o ppmread_test
./ppmread_test

# PPM write + upsampling test
gcc test/ppmwrite.c src/core/io.c src/core/raster.c src/core/utils.c -I. -lm -o ppmwrite_test
./ppmwrite_test
```

## Code style

The project uses `clang-format` with these key settings:
- 80-column line limit
- Allman brace style (opening brace on its own line)
- No space before parentheses: `if(x)` not `if (x)`
- Pointer alignment on the right: `int *p` not `int* p`

Run the formatter with:

```sh
clang-format -i src/**/*.c include/*.h
```

## Learning path

If you're new to this codebase, here's a suggested reading order:

1. **`include/defs.h`** -- Understand the type aliases and vector types. Everything else uses these.
2. **`src/core/include/debug.h`** -- Learn the `OK`/`NNUL`/`IF` error macros. You'll see them everywhere.
3. **`src/main.c`** -- Follow the application from start to finish. This ties all the layers together.
4. **`include/display.h`** then **`src/wayland/wayland.c`** -- The display layer is small and self-contained.
5. **`include/graphics.h`** -- Read the public GPU API before diving into the Vulkan implementation.
6. **`src/vulkan/vulkan.c`** -- The largest file. Read it after you're comfortable with the API it implements.
7. **`assets/shader.slang`** -- A short, readable shader that defines the triangle's vertex and fragment stages.

## License

See the project files for licensing information.
