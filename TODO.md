## s7 REPL blocks the main thread (render loop stalls)

**Symptom:** Window freezes as soon as the engine starts. No frames are
rendered while the REPL is waiting for input.

**Root cause:** `s7_repl()` is a blocking call. `main.c` currently calls
`reploop()` directly on the main thread, which also owns the Wayland event
loop and render loop. The two cannot share the same thread.

**Location:** `src/main.c` — `reploop(repl)` is called before any render loop.

**Fix direction:** Run the REPL on a background thread (as was done before
`main.c` was separated out). `pthread_create` -> `reploop` on worker thread,
main thread owns `dpypoll` + `gfxrender`.

---

## No gfx FFI registered in s7

**Symptom:** The s7 REPL has no engine bindings — `(gfx-pause)`, `(gfx-render)`,
etc. are undefined.

**Root cause:** `replinit` initialises `s7_init()` but registers no C
functions. The FFI layer has not been written yet.

**Location:** `src/repl/repl.c` — `init()`.

**Fix direction:** Register C functions via `s7_define_function()` before
handing off to `s7_repl()`. Engine context (`gfxctx *`) can be passed through
a file-scope pointer since it is a singleton.

---

## gfx status (updated 2026-08-07)

The core path used by g2d is implemented: Vulkan/Wayland context creation,
device and host memory arenas, buffer creation and upload, vertex/instance
bindings, SPIR-V vertex/fragment pipelines, push constants, offscreen color
passes, instanced draws, submission, swapchain resize, and presentation.

This is currently sufficient for g2d's opaque solid-color rectangles. It is
not yet a general-purpose graphics layer: some broader API fields are unused,
and some nominally implemented paths need correctness fixes.

### P0: correctness and lifecycle

- Fix indexed buffers. `gfxdesc_drawcmd.idx` is recorded with
  `vkCmdBindIndexBuffer`, but `gfxinit_buf` creates every buffer without
  `VK_BUFFER_USAGE_INDEX_BUFFER_BIT`. Model buffer usage explicitly or create
  buffers with all required usage flags.
- Fix frame synchronization in `gfxpresent`. The command buffer and fence use
  `frmidx`, while the render-finished semaphore is selected using the acquired
  swapchain `imgidx`. Make frame-slot ownership consistent and validation-clean.
- Handle `VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR` from swapchain
  acquire/present, and recreate the swapchain safely.
- Make resize propagate beyond the swapchain. Offscreen framebuffers and their
  passes currently retain their old size, so presentation merely stretches the
  old image after a window resize.
- Validate every fixed-size descriptor count before writing/iterating arrays:
  shaders, bindings, attributes, push ranges, pass outputs, draw buffers, and
  swapchain images/frames.
- Track image layouts instead of repeatedly assuming `UNDEFINED` or
  `COLOR_ATTACHMENT_OPTIMAL`. Define synchronization between render, sampling,
  transfer, and presentation uses.
- Make `gfxget_display` return a usable abstraction or remove it. It currently
  returns a sentinel pointer that cannot safely be passed where an ordinary
  `gfximg *` is dereferenced.
- Decide what `gfxpause`/`gfxresume` mean and enforce it in submit/present, or
  remove them. They currently only toggle an otherwise-unused field.
- Add null/descriptor validation to public entry points and propagate failures
  from `gfxupdate` instead of discarding swapchain recreation errors.
- Make `gfxsync(NULL)` safe, or guard cleanup call sites before invoking it.
  `src/main.c` and the g2d demos call `gfxsync(gfx)` unconditionally from their
  error paths, so a failure before `gfxinit` succeeds currently dereferences a
  null context during cleanup.

### P1: pipeline and render-pass features needed by g2d

- Add configurable alpha blending. Blending is currently hard-coded off, so
  the alpha member of `g2ddesc_rect.color` does not produce translucent
  composition.
- Add per-draw viewport/scissor state, especially rectangular clipping for GUI
  panels, scrolling regions, and nested widgets.
- Model render-target formats in pipeline creation. Pipelines currently assume
  exactly one color attachment with the swapchain format, regardless of the
  actual pass output image(s).
- Generalize attachment load/store controls. Basic `CLEAR` and `DONT_CARE`
  loading now work, but preserving prior content requires image-layout tracking
  and store behavior remains fixed to `STORE`.
- Either properly support multiple color outputs throughout pipeline creation
  and submission or temporarily constrain `gfxdesc_pass` to one output.
- Add depth attachment support. D32 images can be created, but pass submission
  treats every output as a color attachment.
- Expose the pipeline state that is currently fixed: primitive topology,
  culling/front face, polygon mode, depth state, multisampling, and blend state.

### P1: textures and images

- Implement image data upload (preferably staged), with format/extent checks and
  explicit layout transitions.
- Implement samplers, descriptor set layouts/pools/sets, and sampled-image
  binding. `gfxdesc_drawcmd.inputs` and `nrinputs` currently have no effect.
- Define texture lifetime/update semantics so g2d can support sprites, icons,
  font atlases, and textured shapes.
- Add a small set of sampler options needed by 2D rendering: nearest/linear
  filtering and clamp/repeat addressing.

### P1: memory

- Replace or extend the monotonic arena allocator. Destroying an image or buffer
  destroys its Vulkan handle but does not reclaim its suballocation, so dynamic
  workloads eventually exhaust an arena.
- Add implicit-memory growth (for example, slabs) and useful out-of-memory
  diagnostics rather than relying on one fixed default 2 MiB allocation.
- Implement or remove the currently ignored `gfxdesc.hostmem` setting.
- Correct the implicit-memory flag condition to the unambiguous
  `!(desc.flags & GFX_NO_IMPLICIT_MEM)` form.
- Make buffer ownership explicit. `gfxupload` accepts a memory arena even though
  the buffer is already bound to one; passing a different arena can address the
  wrong allocation. This also affects repeated `g2dupload` calls with changing
  external memory.

### P2: portability and API cleanup

- Honor `gfxdesc.swpfmt`, including a documented fallback when the requested
  surface format is unavailable.
- Do not require the Vulkan validation layer in normal builds; enable it through
  a debug/configuration flag when installed.
- Negotiate the Vulkan API version and required features instead of
  unconditionally requesting Vulkan 1.4.
- Remove the Wayland-specific Vulkan surface dependency from the generic gfx
  layer, or formally document that gfx is a Wayland-only backend for now.
- Replace silent fixed limits such as `GFX_MAX_DRAWS` with checked errors or
  dynamically sized command storage.
- Remove unused diagnostics/declarations and clean the current compiler warnings.

### Tests

- Repair `src/vulkan/tests/test_vulkan.c`; its `updata_direct` tests use an old
  function signature and no longer compile.
- Add automated descriptor-validation and allocator tests that require no GPU.
- Run integration tests with Vulkan validation enabled for buffer upload,
  indexed and instanced draws, resize/out-of-date recovery, and repeated frames.
- Add offscreen readback/image comparison tests so g2d output is verified rather
  than relying only on interactive visual tests.
- Add focused tests for alpha composition, clipping, clear/load behavior,
  texture sampling, and resource churn/reclamation as those features land.

### Suggested implementation order

1. Fix validation/lifecycle issues: indexed usage, synchronization, resize and
   out-of-date recovery, layout tracking, and descriptor bounds.
2. Make render-target format, blending, clear/load/store, and scissor explicit.
3. Add image upload, samplers, descriptors, and sampled image inputs.
4. Add reclaimable/growing memory management.
5. Generalize depth, multiple render targets, and remaining pipeline state.
