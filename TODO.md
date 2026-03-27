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

## Implement gfxinit_vtxbuf / gfxinit_img

Opaque handles `gfxbuf` and `gfximg` are declared in `graphics.h` along with
their creation/destruction signatures, but have no backing implementation in
`vulkan.c`. Needs internal struct definitions and Vulkan resource creation.

---

## Implement gfxpass

The render pass abstraction that bundles a vertex buffer, pipeline, and
input/output images into a single draw unit. Depends on `gfxbuf`, `gfximg`,
and `gfxppl` being implemented first.
