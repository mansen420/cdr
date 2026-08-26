#pragma once

#include "include/defs.h"
#include "include/img.h"

typedef enum
{
  DPYERR_INIT
} DPYERR;

typedef struct dpyctx dpyctx;
typedef struct dpysrf dpysrf;
typedef struct dpyswp dpyswp;

// Opens a connection to the platform display service. flags is currently
// reserved and should be zero. Returns 0 on success and a negative value on
// failure. On failure, *out is set to NULL.
int dpyinit(dpyctx **, u32 flags);

// Returns the context's last display error, or -1 for a NULL context.
int         dpyqry_err(dpyctx *);
const char *dpyqry_errstr(DPYERR);

// Acquires a borrowed platform-native window handle. The handle remains valid
// only while the surface is alive. Its concrete type is backend-specific.
void *dpyacq_nwin(dpysrf *);
// Acquires a borrowed platform-native display/instance handle. The handle
// remains valid only while the display context is alive.
void *dpyacq_ncon(dpyctx *);

// Performs one non-blocking platform I/O/event iteration.
//
// Call this regularly from the application's event loop. It dispatches pending
// resize/close callbacks and progresses backend work such as sending queued
// surface and presentation requests and receiving buffer-release events.
// Callbacks run synchronously inside this function and should normally only
// update application state; they should not recursively call dpypoll().
//
// A successful return only means that polling itself succeeded. It does not
// mean that previously queued presentation requests have appeared onscreen.
// Returns 0 on success and a negative value on failure.
int dpypoll(dpyctx *);

// Closes the display connection. All surfaces and swapchains associated with
// the context must be terminated first.
void dpyterm(dpyctx *);

typedef void (*resizefn_t)(i32, i32, void *);
typedef void (*closefn_t)(void *);

typedef struct
{
  // Preferred initial content size. A compositor/window manager may configure
  // a different size. Dimensions are expressed in logical surface units.
  u32 w, h;

  // Requested initial position. Backends whose window system controls window
  // placement may ignore these values.
  i32 x, y;

  // Initial title/application identifier. The string is consumed during this
  // call and need not remain valid afterward.
  const char *title;
} dpydesc_srf;

// Creates a top-level surface. This may exchange messages with the platform
// synchronously to obtain its initial configuration. Returns 0 on success and
// a negative value on failure; *out is set to NULL on failure.
int  dpyinit_srf(dpyctx *, dpysrf **, dpydesc_srf);
void dpyterm_srf(dpyctx *, dpysrf *);

// Sets the pointer passed to this surface's callbacks. The pointer is borrowed
// and remains owned by the caller.
void dpyset_usrdata(dpysrf *, void *);

// Installs callbacks for subsequent platform events. Callbacks are invoked
// synchronously by dpypoll(). Resize dimensions are the newly configured
// logical content size. A close callback reports a request to close; it does
// not destroy the surface automatically.
void dpyset_onrsz(dpysrf *, resizefn_t);
void dpyset_onclose(dpysrf *, closefn_t);

// A swapchain belongs to the surface with which it was created and must not be
// presented to another surface.
typedef struct
{
  // Buffer dimensions in pixels. A zero dimension selects the corresponding
  // configured surface dimension.
  u32 w, h;

  // Preferred image format. IMG_DEFAULT_FMT lets the backend choose; the
  // resulting imgview reports the format actually selected.
  imgfmt fmt;
} dpydesc_swp;

// Creates the surface's CPU-drawable swapchain. Returns 0 on success and a
// negative value on failure; *out is set to NULL on failure.
int  dpyinit_swp(dpyctx *, dpysrf *, dpyswp **, dpydesc_swp);

// The swapchain must not be destroyed while one of its images is still owned
// by the platform.
void dpyterm_swp(dpyswp *);

// Acquires the next CPU-writable swapchain image. Returns an imgview with
// data == NULL when that image is still owned by the platform. Availability is
// progressed by dpypoll(); this function does not block or dispatch events.
// Once acquired, the image must be either presented or discarded before
// another image is acquired from this swapchain.
imgview dpyacq_nextbuf(dpyswp *);

// Discards the most recently acquired image without presenting it and advances
// to the next swapchain image.
void dpyrel_buf(dpyswp *);

// Queues the most recently acquired image for presentation and relinquishes
// CPU access to it. This function is non-blocking: success means the request
// was queued, not that the image was displayed. The image becomes writable
// again only after platform events release it; call dpypoll() regularly to send
// queued requests and receive those events. Rapid successive presentations may
// supersede each other, so every successful call is not guaranteed to appear
// onscreen. Returns 0 on success and a negative value if no presentable image
// is currently acquired or the request cannot be queued.
int dpypresent(dpyctx *, dpysrf *, dpyswp *);

// Reserved; asynchronous presentation feedback is not implemented yet.
int dpypresent_async(); // prototype
