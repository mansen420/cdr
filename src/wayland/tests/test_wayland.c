// AI-generated (Claude)
// Compile: gcc src/wayland/wayland.c src/wayland/xdg-shell-client-protocol.c \
//              src/wayland/tests/test_wayland.c \
//              -I. -lwayland-client -o wayland_test
// Run:     ./wayland_test

#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/defs.h"
#include "src/wayland/wayland.h"

static int tests_run    = 0;
static int tests_failed = 0;

#define CHECK(cond, msg)                                                   \
  do {                                                                     \
    tests_run++;                                                           \
    if(!(cond))                                                            \
    {                                                                      \
      fprintf(stderr, "FAIL [%s:%d]: %s\n", __func__, __LINE__, (msg));   \
      tests_failed++;                                                      \
    }                                                                      \
  } while(0)

// ---------------------------------------------------------------------------
// dpyqry_init
// ---------------------------------------------------------------------------

static void test_dpyqry_init_null(void)
{
  // NULL pointer must return 0 (not crash)
  int result = dpyqry_init(NULL);
  CHECK(result == 0, "dpyqry_init(NULL) should return 0");
}

static void test_dpyqry_init_zeroed_struct(void)
{
  // A zeroed wlctx has alive==0, so dpyqry_init must return 0
  wlctx ctx;
  memset(&ctx, 0, sizeof ctx);
  int result = dpyqry_init((dpyctx *)&ctx);
  CHECK(result == 0, "dpyqry_init on zeroed struct should return 0 (alive==0)");
}

static void test_dpyqry_init_alive_set(void)
{
  // When alive is non-zero dpyqry_init must return non-zero
  wlctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.alive = 1;
  int result = dpyqry_init((dpyctx *)&ctx);
  CHECK(result != 0, "dpyqry_init should return non-zero when alive==1");
}

// ---------------------------------------------------------------------------
// dpyinit with no Wayland compositor
// ---------------------------------------------------------------------------

static void test_dpyinit_no_compositor(void)
{
  // Point WAYLAND_DISPLAY at a socket that cannot exist so that
  // wl_display_connect fails.  dpyinit must return <0 without crashing.
  setenv("WAYLAND_DISPLAY", "wayland-test-nonexistent-9999", 1);

  wlctx *ctx = NULL;
  int rc = dpyinit((dpyctx **)&ctx, "test", 800, 600, 0);
  CHECK(rc < 0, "dpyinit should return <0 when Wayland is unavailable");

  // Restore: unset so later tests are not affected
  unsetenv("WAYLAND_DISPLAY");
}

// ---------------------------------------------------------------------------
// toplvlbounds_h
// ---------------------------------------------------------------------------

static void test_toplvlbounds_stores_nonzero(void)
{
  wlctx ctx;
  memset(&ctx, 0, sizeof ctx);

  toplvlbounds_h(&ctx, NULL, 1920, 1080);

  CHECK(ctx.w_max == 1920, "toplvlbounds_h should store w_max=1920");
  CHECK(ctx.h_max == 1080, "toplvlbounds_h should store h_max=1080");
}

static void test_toplvlbounds_ignores_zero_dimensions(void)
{
  wlctx ctx;
  memset(&ctx, 0, sizeof ctx);
  // Pre-set sentinel values; the handler must not overwrite them with zeros
  ctx.w_max = 1280;
  ctx.h_max = 720;

  toplvlbounds_h(&ctx, NULL, 0, 0);

  CHECK(ctx.w_max == 1280, "toplvlbounds_h must not overwrite w_max when width==0");
  CHECK(ctx.h_max == 720,  "toplvlbounds_h must not overwrite h_max when height==0");
}

static void test_toplvlbounds_ignores_partial_zero(void)
{
  // Both dimensions must be non-zero for the update to happen
  wlctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.w_max = 100;
  ctx.h_max = 200;

  toplvlbounds_h(&ctx, NULL, 640, 0);   // height is zero — no update expected
  CHECK(ctx.w_max == 100, "toplvlbounds_h must not update when only height is zero");
  CHECK(ctx.h_max == 200, "toplvlbounds_h must not update when only height is zero");

  toplvlbounds_h(&ctx, NULL, 0, 480);   // width is zero — no update expected
  CHECK(ctx.w_max == 100, "toplvlbounds_h must not update when only width is zero");
  CHECK(ctx.h_max == 200, "toplvlbounds_h must not update when only width is zero");
}

static void test_toplvlbounds_null_data(void)
{
  // NNUL(data, return;) guard — must not crash
  toplvlbounds_h(NULL, NULL, 800, 600);
  tests_run++;   // reaching here counts as passing
}

// ---------------------------------------------------------------------------
// toplvlcnf_h
// ---------------------------------------------------------------------------

// Capture the last resize callback arguments
static i32  last_rsz_w   = 0;
static i32  last_rsz_h   = 0;
static int  rszcb_called = 0;

static void resize_spy(i32 w, i32 h, void *userdata)
{
  last_rsz_w   = w;
  last_rsz_h   = h;
  rszcb_called = 1;
}

static void reset_spy(void)
{
  last_rsz_w   = 0;
  last_rsz_h   = 0;
  rszcb_called = 0;
}

static void test_toplvlcnf_uses_compositor_dims(void)
{
  reset_spy();
  wlctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.w_pref = 800;
  ctx.h_pref = 600;
  ctx.rszcb  = resize_spy;

  // Non-zero compositor dimensions should be forwarded as-is
  toplvlcnf_h(&ctx, NULL, 1024, 768, NULL);

  CHECK(rszcb_called == 1, "toplvlcnf_h should invoke rszcb");
  CHECK(last_rsz_w == 1024, "toplvlcnf_h should pass compositor width to rszcb");
  CHECK(last_rsz_h == 768,  "toplvlcnf_h should pass compositor height to rszcb");
}

static void test_toplvlcnf_falls_back_to_pref(void)
{
  reset_spy();
  wlctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.w_pref = 800;
  ctx.h_pref = 600;
  ctx.rszcb  = resize_spy;

  // width==0 && height==0 means the compositor defers to the application
  toplvlcnf_h(&ctx, NULL, 0, 0, NULL);

  CHECK(rszcb_called == 1, "toplvlcnf_h should invoke rszcb even when dims are 0");
  CHECK(last_rsz_w == 800, "toplvlcnf_h should use w_pref when width==0");
  CHECK(last_rsz_h == 600, "toplvlcnf_h should use h_pref when height==0");
}

static void test_toplvlcnf_no_rszcb(void)
{
  // rszcb is NULL — must not crash
  wlctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.w_pref = 640;
  ctx.h_pref = 480;
  ctx.rszcb  = NULL;

  toplvlcnf_h(&ctx, NULL, 0, 0, NULL);
  tests_run++;   // reaching here counts as passing
}

static void test_toplvlcnf_null_data(void)
{
  // NNUL(data, return;) guard — must not crash
  toplvlcnf_h(NULL, NULL, 800, 600, NULL);
  tests_run++;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void)
{
  test_dpyqry_init_null();
  test_dpyqry_init_zeroed_struct();
  test_dpyqry_init_alive_set();

  test_dpyinit_no_compositor();

  test_toplvlbounds_stores_nonzero();
  test_toplvlbounds_ignores_zero_dimensions();
  test_toplvlbounds_ignores_partial_zero();
  test_toplvlbounds_null_data();

  test_toplvlcnf_uses_compositor_dims();
  test_toplvlcnf_falls_back_to_pref();
  test_toplvlcnf_no_rszcb();
  test_toplvlcnf_null_data();

  printf("%d/%d tests passed.\n", tests_run - tests_failed, tests_run);
  return tests_failed > 0 ? 1 : 0;
}
