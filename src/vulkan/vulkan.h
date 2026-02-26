#pragma once

#define _GNU_SOURCE

#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_wayland.h>

#include "include/defs.h"
#include "include/display.h"
#include "include/graphics.h"

#define OK(_expr, DO)                                             \
  if((_expr) != 0)                                                \
  {                                                               \
    printf("Negative: %s (%s:%d)\n", #_expr, __FILE__, __LINE__); \
    DO                                                            \
  }
#define NNUL(_expr, DO)                                               \
  if(!(_expr))                                                        \
  {                                                                   \
    printf("Null pointer: %s (%s:%d)\n", #_expr, __FILE__, __LINE__); \
    DO                                                                \
  }

#define SHADERS_MAX 16
#define BUFFERS_MAX 64
#define ARENAS_MAX 32
#define MB (1024 * 1024)

#define VKNULL VK_NULL_HANDLE

typedef struct vkfrmctx
{
  VkCommandBuffer cmdbuf;
  VkSemaphore     renderfin, imggotten;
  VkFence         fence;
} frmctx;

typedef struct
{
  VkDeviceMemory data;
  usize          size;
  usize          offset;
  u32            typeidx;
  u32            flags;

  // diagnostic
  u32 slop;
} vkarena;

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

  // pipeline stuff
  VkShaderModule   shaders[SHADERS_MAX];
  u32              nrshaders;
  VkPipelineLayout layout;
  VkPipeline       gfxppl;

  // cmdpool stuff
  VkCommandPool gfxcmdpool;
  VkCommandPool prscmdpool;

  u32 gfxqfamidx;
  u32 prsqfamidx;

  VkExtent2D swpextent;

  // frames
  frmctx frames[8];
  u32    nrframes;

  // memory
  vkarena  arenas[32];
  u32      nrarenas;
  VkBuffer buffers[BUFFERS_MAX];

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

  u32 alive;
} vkctx;

// public API (mirrored from graphics.h)
gfxctx *gfxinit(void *dpy);
void    gfxterm(gfxctx *ctx);
int     gfxinit_q(gfxctx *ctx);

// internal — init/term
vkctx *init(void *dpy);
void   term(vkctx *ctx);
void   frmterm(vkctx *ctx, u32 idx);
int    frminit(vkctx *ctx);
int    initgfxppl(vkctx *ctx, const u32 *shcode, u32 shlen);
int    reinitswp(vkctx *ctx, i32 w_new, i32 h_new);
int    render(vkctx *ctx);

// internal — memory
vkarena *arenainit(
  vkctx *ctx, u32 idx, usize bytes, VkMemoryPropertyFlags flags);
void     arenaclear(vkarena *arena);
void     arenaterm(vkctx *ctx, vkarena *arena);
i64      memalloc(vkarena *arena, usize bytes, usize align);
usize    alignup(usize addr, usize align);
VkBuffer updata_direct(
  vkctx *ctx, vkarena *rbar, const void *data, usize bytes);

// internal — diagnostics
void printmeminfo(const vkctx *ctx);
void printarenainfo(const vkctx *ctx, const vkarena *arena, const char *name);
