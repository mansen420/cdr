# Known Bugs

## SIGSEGV on REPL `exit` command

**Symptom:** Typing `exit` in the REPL causes a SIGSEGV and crashes the process.

**Root cause:** Use-after-free on `replctx`.

When the REPL thread handles `exit`, `reploop()` sets `ctx->quit = 1` and
returns. `repl_thread_fn` then calls `replterm(arg)`, freeing the `replctx`.
Meanwhile, the main render loop is still reading `repl->quit` in its loop
condition — the pointer is now dangling.

```c
// src/repl/repl.c — main render loop
while(dpypoll(dpy) == 0 && !repl->quit && !quit)  // repl may be freed here
```

**Location:** `src/repl/repl.c` — `repl_thread_fn` and the main render loop.

**Fix direction:** `replctx` ownership is ambiguous between the two threads.
Options:
- Move `replterm` back to the main thread (after `pthread_join`), drop the
  detach. Accept the blocked-join-on-fgets cost.
- Keep detach but replace `repl->quit` in the loop condition with a separate
  `volatile int quit_repl` local to `main()`, written by the REPL thread via
  a pointer stored at init time. `replctx` can be freed by the thread without
  invalidating the flag the main thread reads.
- Use an atomic or a mutex-protected flag.
