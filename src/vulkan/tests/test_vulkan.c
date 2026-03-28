// AI-generated (Claude)
// Compile: gcc -DTESTING src/wayland/wayland.c \
//              src/wayland/xdg-shell-client-protocol.c \
//              src/raster.c src/io.c \
//              src/vulkan/tests/test_vulkan.c \
//              -I. -lvulkan -lwayland-client -o vulkan_test
// Run:     ./vulkan_test

#include "src/vulkan/vulkan.c"

static int tests_run    = 0;
static int tests_failed = 0;

#define CHECK(cond, msg)                                                 \
  do {                                                                   \
    tests_run++;                                                         \
    if(!(cond))                                                          \
    {                                                                    \
      fprintf(stderr, "FAIL [%s:%d]: %s\n", __func__, __LINE__, (msg)); \
      tests_failed++;                                                    \
    }                                                                    \
  } while(0)

// ---------------------------------------------------------------------------
// gfxqry_init
// ---------------------------------------------------------------------------

static void test_gfxqry_init_null(void)
{
  // NULL must return 0 and must not crash
  int result = gfxqry_init(NULL);
  CHECK(result == 0, "gfxqry_init(NULL) should return 0");
}

static void test_gfxqry_init_zeroed_struct(void)
{
  // A zeroed vkctx has alive==0, so gfxqry_init must return 0
  vkctx ctx;
  memset(&ctx, 0, sizeof ctx);
  int result = gfxqry_init((gfxctx *)&ctx);
  CHECK(result == 0, "gfxqry_init on zeroed vkctx should return 0 (alive==0)");
}

static void test_gfxqry_init_alive_set(void)
{
  // When alive is non-zero gfxqry_init must return non-zero
  vkctx ctx;
  memset(&ctx, 0, sizeof ctx);
  ctx.alive = 1;
  int result = gfxqry_init((gfxctx *)&ctx);
  CHECK(result != 0, "gfxqry_init should return non-zero when alive==1");
}

// ---------------------------------------------------------------------------
// alignup
// ---------------------------------------------------------------------------

static void test_alignup_already_aligned(void)
{
  // An address that is already a multiple of align must be returned unchanged
  CHECK(alignup(0,   4) == 0,   "alignup(0, 4) should be 0");
  CHECK(alignup(16,  4) == 16,  "alignup(16, 4) should be 16");
  CHECK(alignup(256, 256) == 256, "alignup(256, 256) should be 256");
  CHECK(alignup(64,  8) == 64,  "alignup(64, 8) should be 64");
}

static void test_alignup_unaligned_rounds_up(void)
{
  // Addresses not on the alignment boundary must round up to the next multiple
  CHECK(alignup(1,  4) == 4,   "alignup(1, 4) should be 4");
  CHECK(alignup(3,  4) == 4,   "alignup(3, 4) should be 4");
  CHECK(alignup(5,  8) == 8,   "alignup(5, 8) should be 8");
  CHECK(alignup(9,  16) == 16, "alignup(9, 16) should be 16");
  CHECK(alignup(17, 256) == 256, "alignup(17, 256) should be 256");
  CHECK(alignup(255, 256) == 256, "alignup(255, 256) should be 256");
  CHECK(alignup(257, 256) == 512, "alignup(257, 256) should be 512");
}

static void test_alignup_align_one(void)
{
  // align=1 means every address is already aligned — result must equal addr
  CHECK(alignup(0,   1) == 0,   "alignup(0, 1) should be 0");
  CHECK(alignup(1,   1) == 1,   "alignup(1, 1) should be 1");
  CHECK(alignup(7,   1) == 7,   "alignup(7, 1) should be 7");
  CHECK(alignup(100, 1) == 100, "alignup(100, 1) should be 100");
}

static void test_alignup_power_of_two_alignments(void)
{
  // Spot-check a range of power-of-two alignments
  CHECK(alignup(3,   4)   == 4,   "alignup(3, 4) should be 4");
  CHECK(alignup(7,   8)   == 8,   "alignup(7, 8) should be 8");
  CHECK(alignup(15,  16)  == 16,  "alignup(15, 16) should be 16");
  CHECK(alignup(1,   256) == 256, "alignup(1, 256) should be 256");
  // A value exactly one past a boundary
  CHECK(alignup(4,   4)   == 4,   "alignup(4, 4) should be 4 (already aligned)");
  CHECK(alignup(8,   8)   == 8,   "alignup(8, 8) should be 8 (already aligned)");
  CHECK(alignup(128, 16)  == 128, "alignup(128, 16) should be 128 (already aligned)");
}

// ---------------------------------------------------------------------------
// arena_suballoc
// ---------------------------------------------------------------------------

// Helper: build a fake arena on the stack with no real VkDeviceMemory.
// The data handle is VK_NULL_HANDLE — fine for pure bump-pointer logic.
static vkarena make_arena(usize size)
{
  vkarena a;
  memset(&a, 0, sizeof a);
  a.size  = size;
  a.flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  // a.data remains VK_NULL_HANDLE; arena_suballoc never dereferences it
  return a;
}

static void test_arena_suballoc_null_arena(void)
{
  // NULL arena must return -1
  i64 result = arena_suballoc(NULL, 64, 4);
  CHECK(result == -1, "arena_suballoc(NULL, ...) should return -1");
}

static void test_arena_suballoc_first_alloc_offset_zero(void)
{
  // The first allocation from a fresh arena with align==1 should be at offset 0
  vkarena arena = make_arena(1024);
  i64 offset = arena_suballoc(&arena, 64, 1);
  CHECK(offset == 0, "first arena_suballoc with align=1 should return offset 0");
}

static void test_arena_suballoc_advances_offset(void)
{
  // After a 64-byte allocation the arena offset should have moved forward by 64
  vkarena arena = make_arena(1024);
  arena_suballoc(&arena, 64, 1);
  CHECK(arena.offset == 64, "arena.offset should be 64 after one 64-byte alloc");
}

static void test_arena_suballoc_second_alloc_after_first(void)
{
  // Second alloc (align=1) starts immediately after the first
  vkarena arena = make_arena(1024);
  arena_suballoc(&arena, 64, 1);
  i64 second = arena_suballoc(&arena, 32, 1);
  CHECK(second == 64, "second arena_suballoc with align=1 should return offset 64");
  CHECK(arena.offset == 96, "arena.offset should be 96 after two allocs (64+32)");
}

static void test_arena_suballoc_alignment_padding_tracked_in_slop(void)
{
  // Start with offset=1, request align=4: alignup(1,4)==4, overhead=3, slop+=3
  vkarena arena = make_arena(1024);
  arena_suballoc(&arena, 1, 1);          // offset becomes 1
  i64 second = arena_suballoc(&arena, 8, 4);  // aligned to 4 → offset 4, slop should be 3
  CHECK(second == 4,         "arena_suballoc with align=4 from offset 1 should return 4");
  CHECK(arena.slop == 3,     "3 bytes of padding should be recorded in arena.slop");
  CHECK(arena.offset == 12,  "offset should be 12 (4 aligned + 8 bytes)");
}

static void test_arena_suballoc_overflow_returns_minus_one(void)
{
  // Requesting more bytes than the arena holds must return -1
  vkarena arena = make_arena(64);
  i64 result = arena_suballoc(&arena, 128, 1);
  CHECK(result == -1, "arena_suballoc should return -1 when bytes exceed arena size");
}

static void test_arena_suballoc_exact_fit(void)
{
  // An allocation that exactly fills the remaining space should succeed
  vkarena arena = make_arena(64);
  i64 result = arena_suballoc(&arena, 64, 1);
  CHECK(result == 0, "exact-fit arena_suballoc should succeed and return 0");
  CHECK(arena.offset == 64, "arena.offset should equal arena.size after exact fill");
}

static void test_arena_suballoc_one_past_capacity_returns_minus_one(void)
{
  // An allocation of size+1 must be rejected
  vkarena arena = make_arena(64);
  i64 result = arena_suballoc(&arena, 65, 1);
  CHECK(result == -1, "arena_suballoc of size+1 should return -1");
}

static void test_arena_suballoc_overflow_after_partial_use(void)
{
  // Fill most of the arena then try an allocation that would overflow
  vkarena arena = make_arena(64);
  arena_suballoc(&arena, 48, 1);        // 16 bytes remain
  i64 result = arena_suballoc(&arena, 17, 1);
  CHECK(result == -1, "arena_suballoc should return -1 when remaining space is insufficient");
}

static void test_arena_suballoc_alignment_overflow_returns_minus_one(void)
{
  // Fill arena to 1 byte from end, then request align=256 — padding alone overflows
  vkarena arena = make_arena(64);
  arena_suballoc(&arena, 63, 1);   // offset == 63, 1 byte free
  i64 result = arena_suballoc(&arena, 1, 256);
  CHECK(result == -1,
        "arena_suballoc should return -1 when alignment padding alone overflows the arena");
}

static void test_arena_suballoc_slop_zero_on_no_padding(void)
{
  // When all allocations are already aligned, slop must stay 0
  vkarena arena = make_arena(256);
  arena_suballoc(&arena, 16, 16);
  arena_suballoc(&arena, 32, 16);
  CHECK(arena.slop == 0, "slop should be 0 when all allocations are naturally aligned");
}

// ---------------------------------------------------------------------------
// arenaclear
// ---------------------------------------------------------------------------

static void test_arenaclear_resets_offset_and_slop(void)
{
  // After allocating with padding, arenaclear must zero both offset and slop
  vkarena arena = make_arena(1024);
  arena_suballoc(&arena, 1, 1);         // offset == 1
  arena_suballoc(&arena, 8, 4);         // offset == 12, slop == 3

  arenaclear(&arena);

  CHECK(arena.offset == 0, "arenaclear should reset offset to 0");
  CHECK(arena.slop   == 0, "arenaclear should reset slop to 0");
}

static void test_arenaclear_reuse_after_clear(void)
{
  // After clearing, the next allocation should return offset 0 again
  vkarena arena = make_arena(1024);
  arena_suballoc(&arena, 100, 1);

  arenaclear(&arena);

  i64 result = arena_suballoc(&arena, 64, 1);
  CHECK(result == 0,         "first alloc after arenaclear should return offset 0");
  CHECK(arena.offset == 64,  "arena.offset should be 64 after alloc post-clear");
}

static void test_arenaclear_preserves_size_and_flags(void)
{
  // arenaclear must not touch size or flags — only offset and slop
  vkarena arena = make_arena(512);
  arena.flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  arena_suballoc(&arena, 128, 1);

  usize saved_size  = arena.size;
  u32   saved_flags = arena.flags;

  arenaclear(&arena);

  CHECK(arena.size  == saved_size,  "arenaclear must not modify arena.size");
  CHECK(arena.flags == saved_flags, "arenaclear must not modify arena.flags");
}

// ---------------------------------------------------------------------------
// updata_direct — guard conditions (no GPU required)
// ---------------------------------------------------------------------------

static void test_updata_direct_null_ctx(void)
{
  vkarena arena= {0};
  arena.flags  = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  arena.size= 1024;
  u8       data= 42;
  VkBuffer buf = updata_direct(NULL, &arena, &data, sizeof data);
  CHECK(buf == VKNULL, "updata_direct(NULL ctx) must return VKNULL");
}

static void test_updata_direct_null_data(void)
{
  vkctx    ctx = {0};
  vkarena  arena= {0};
  arena.flags   = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  arena.size    = 1024;
  VkBuffer buf  = updata_direct(&ctx, &arena, NULL, 16);
  CHECK(buf == VKNULL, "updata_direct(NULL data) must return VKNULL");
}

static void test_updata_direct_wrong_flags(void)
{
  // Arena is DEVICE_LOCAL only — not host-visible, cannot map
  vkctx    ctx = {0};
  vkarena  arena= {0};
  arena.flags   = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  arena.size    = 1024;
  u8       data = 0;
  VkBuffer buf  = updata_direct(&ctx, &arena, &data, sizeof data);
  CHECK(buf == VKNULL,
        "updata_direct with non-host-visible arena must return VKNULL");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(void)
{
  // gfxqry_init
  test_gfxqry_init_null();
  test_gfxqry_init_zeroed_struct();
  test_gfxqry_init_alive_set();

  // alignup
  test_alignup_already_aligned();
  test_alignup_unaligned_rounds_up();
  test_alignup_align_one();
  test_alignup_power_of_two_alignments();

  // arena_suballoc
  test_arena_suballoc_null_arena();
  test_arena_suballoc_first_alloc_offset_zero();
  test_arena_suballoc_advances_offset();
  test_arena_suballoc_second_alloc_after_first();
  test_arena_suballoc_alignment_padding_tracked_in_slop();
  test_arena_suballoc_overflow_returns_minus_one();
  test_arena_suballoc_exact_fit();
  test_arena_suballoc_one_past_capacity_returns_minus_one();
  test_arena_suballoc_overflow_after_partial_use();
  test_arena_suballoc_alignment_overflow_returns_minus_one();
  test_arena_suballoc_slop_zero_on_no_padding();

  // arenaclear
  test_arenaclear_resets_offset_and_slop();
  test_arenaclear_reuse_after_clear();
  test_arenaclear_preserves_size_and_flags();

  // updata_direct — guard conditions only (no GPU required)
  test_updata_direct_null_ctx();
  test_updata_direct_null_data();
  test_updata_direct_wrong_flags();

  printf("%d/%d tests passed.\n", tests_run - tests_failed, tests_run);
  return tests_failed > 0 ? 1 : 0;
}
