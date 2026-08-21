#pragma once

#define _GNU_SOURCE

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_wayland.h>

#include <time.h>

#include "include/defs.h"
#include "include/graphics.h"
#include "src/core/include/debug.h"

#define ARENAS_MAX 32
#define MB (1024 * 1024)

static gfximg *const DISPLAY_IMG= (gfximg *)1;

#define VKNULL VK_NULL_HANDLE

typedef struct vkfrmctx
{
  VkCommandBuffer cmdbuf;
  VkSemaphore     renderfin, imggotten;
  VkFence         fence;
} frmctx;

typedef struct gfxmem
{
  VkDeviceMemory data;
  usize          size;
  usize          offset;
  u32            typeidx;
  u32            flags;

  // diagnostic
  u32 slop;
} vkarena;

struct gfxmem_dev
{
  gfxmem base;
};

struct gfxmem_host
{
  gfxmem base;
  void  *mapped;
};

typedef struct gfxctx
{
  // handles
  VkInstance       inst;
  VkSurfaceKHR     surf;
  VkPhysicalDevice gpu;
  VkDevice         dev;
  VkQueue          gfxq;
  VkQueue          prsq;

  // swapchain stuff
  VkSwapchainKHR swp;
  u32            nrswpimgs;
  VkImage        swpimgs[16];
  VkImageView    swpimgvs[16];
  VkExtent2D     swpextent;

  // cmdpool stuff
  VkCommandPool gfxcmdpool;
  VkCommandPool prscmdpool;

  u32 gfxqfamidx;
  u32 prsqfamidx;

  // frames
  frmctx frames[8];
  u32    nrframes;

  // memory
  gfxmem_dev dfltpool;

  // offline submit
  VkCommandBuffer offlinecmdbuf;
  VkFence         offlinefence;

  // render state
  u32             frmidx;
  u32             absfrmidx;
  volatile int    pause;
  struct timespec diag_t_last;
  u64             diag_acc_ns;
  u32             diag_acc_count;

  // info
  u32                              gfxprs;
  VkPhysicalDeviceFeatures2        gpufeats;
  VkPhysicalDeviceVulkan11Features gpufeats11;
  VkPhysicalDeviceVulkan12Features gpufeats12;
  VkPhysicalDeviceVulkan13Features gpufeats13;
  VkPhysicalDeviceVulkan14Features gpufeats14;

  VkPhysicalDeviceProperties gpuprops;
  const char               **layers;
  u32                        nrlayers;
  const char               **extensions;
  u32                        nrextensions;
  const char               **devextensions;
  u32                        nrdevextensions;
  VkQueueFamilyProperties    qfams[16];
  u32                        nrqfams;
  u32                        apiver;
  VkSurfaceCapabilitiesKHR   gpuscaps;
  VkSurfaceFormatKHR         gpusfmts[64];
  u32                        nrgpusfmts;
  VkPresentModeKHR           prsmodes[8];
  u32                        nrprsmodes;
  VkSurfaceFormatKHR         swpsfmt;

} vkctx;

struct gfxbuf
{
  VkBuffer    buf;
  gfxdesc_buf desc;
  usize       bindoff;
};

struct gfximg
{
  VkImage     img;
  VkImageView view;
  gfxdesc_img desc;
};

struct gfxppl
{
  VkPipeline       handle;
  VkPipelineLayout layout;
  gfxdesc_ppl      desc;
};

#define GFX_MAX_DRAWS 64

struct gfxpass
{
  gfxdesc_pass    desc;
  gfxdesc_drawcmd draws[GFX_MAX_DRAWS];
  u32             nrdraws;
  int             recording;
};

// internal — init/term
static int  init(vkctx **, void *dpy, gfxdesc desc);
static void term(vkctx *ctx);
static void term_frame(vkctx *ctx, u32 idx);
static int  init_frame(vkctx *ctx);
static int  init_swp(vkctx *ctx, i32 w, i32 h);
static int  reinit_swp(vkctx *ctx, i32 w_new, i32 h_new);

// internal — pass / commands
static int  init_pass(vkctx *, gfxpass **, gfxdesc_pass);
static void term_pass(vkctx *, gfxpass *);
static void cmd_begin(gfxpass *);
static void cmd_draw(gfxpass *, gfxdesc_drawcmd);
static void cmd_end(gfxpass *);
static int  submit(vkctx *, gfxpass *);

// internal — format helpers
static VkFormat attrfmt_to_vk(gfxfmt_attr fmt);
static VkFormat imgfmt_to_vk(gfxfmt_img fmt);

// internal — pipeline
static int  init_ppl_desc(vkctx *, gfxppl **, gfxdesc_ppl);
static void term_ppl(vkctx *, gfxppl *);

// internal — memory
static int arenainit(
  vkarena *out, vkctx *ctx, usize bytes, VkMemoryPropertyFlags flags);
static void  arenaclear(vkarena *arena);
static void  arenaterm(vkctx *ctx, vkarena *arena);
static i64   arena_suballoc(vkarena *arena, usize bytes, usize align);
static i64   bufferalloc(vkctx *ctx, vkarena *arena, VkBuffer buf);
static int   upload(vkctx *ctx, gfxbuf *buf, vkarena *arena, gfxdesc_upload mem);
static usize alignup(usize addr, usize align);
static int   updata_direct(
  vkctx      *ctx,
  vkarena    *mem,
  VkBuffer    dst,
  usize       bindoff,
  usize       dstoff,
  const void *data,
  usize       bytes);
static int updata_staged(
  vkctx      *ctx,
  vkarena    *host,
  VkBuffer    dst,
  usize       dstoff,
  const void *data,
  usize       bytes);

// internal — diagnostics
static void printmeminfo(const vkctx *ctx);
static void printarenainfo(
  const vkctx *ctx, const vkarena *arena, const char *name);
