#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_wayland.h>

#include "include/display.h"
#include "include/graphics.h"

#define OK(_expr, DO)                                             \
  if((_expr) < 0)                                                 \
  {                                                               \
    printf("Negative: %s (%s:%d)\n", #_expr, __FILE__, __LINE__); \
    DO                                                            \
  }

static const char *const VK_LAYER_VALID_NAME=
  "VK_LAYER_KHRONOS_validation";

typedef struct gfxctx
{
  // handles
  VkInstance       inst;
  VkSurfaceKHR     surf;
  VkPhysicalDevice gpu;
  VkDevice         dev;
  VkQueue          gfxq;
  VkQueue          prsq;
  VkSwapchainKHR   swp;
  u32              nrswpimgs;
  VkImage          swpimgs[16];
  VkImageView      swpimgvs[16];

  // XXX keep these in contiguous memory and keep gfx first. see:
  // swapchain creation
  u32 gfxqfamidx;  // God queues
  u32 prsqfamidx;

  // info

  // 1 if graphics and present queues indices are the same
  u32                        gfxprs;
  VkPhysicalDeviceFeatures   gpufeats;
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

static vkctx *init(void *dpy)
{
  vkctx *ctx= calloc(1, sizeof *ctx);

  // instance
  {
    VkInstanceCreateInfo info= {0};
    info.sType               = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    static const char *layers[]= {VK_LAYER_VALID_NAME};
    info.ppEnabledLayerNames   = layers;
    info.enabledLayerCount     = 1;

    static const char *extensions[]= {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,  // TODO abstract this
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    info.ppEnabledExtensionNames= extensions;
    info.enabledExtensionCount  = 3;

    VkApplicationInfo appInfo= {
      .sType     = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .apiVersion= VK_API_VERSION_1_4};
    info.pApplicationInfo= &appInfo;

    ctx->layers= layers, ctx->extensions= extensions;
    ctx->nrlayers= 1, ctx->nrextensions= 3;
    ctx->apiver= VK_API_VERSION_1_4;

    VkResult res= vkCreateInstance(&info, NULL, &ctx->inst);
    OK(res, goto fail;)
  }
  // wayland surface
  {
    VkWaylandSurfaceCreateInfoKHR info= {
      .sType  = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
      .display= dpygetncon(dpy),
      .surface= dpygetnwin(dpy),
    };
    VkResult res=
      vkCreateWaylandSurfaceKHR(ctx->inst, &info, NULL, &ctx->surf);
    OK(res, goto fail;)
  }

  // pick GPU
  {
    u32              nphy= 0;
    VkPhysicalDevice phy[8];  // at most 8 devices

    VkResult res= vkEnumeratePhysicalDevices(ctx->inst, &nphy, NULL);
    OK(res, goto fail;)
    if(nphy == 0) goto fail;

    nphy= nphy > 8 ? 8 : nphy;  // clamp

    res= vkEnumeratePhysicalDevices(ctx->inst, &nphy, phy);
    OK(res, goto fail;)

    VkPhysicalDevice *discrete= NULL, *integrated= NULL, *cpu= NULL;
    for(unsigned i= 0; i < nphy; i++)
    {
      VkPhysicalDeviceProperties props;
      vkGetPhysicalDeviceProperties(phy[i], &props);
      if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        discrete= &phy[i];
      if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        integrated= &phy[i];
      if(props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) cpu= &phy[i];
    }

    if(discrete)
    {
      ctx->gpu= *discrete;
      vkGetPhysicalDeviceProperties(*discrete, &ctx->gpuprops);
      vkGetPhysicalDeviceFeatures(*discrete, &ctx->gpufeats);
    }
    else if(integrated)
    {
      ctx->gpu= *integrated;
      vkGetPhysicalDeviceProperties(*integrated, &ctx->gpuprops);
      vkGetPhysicalDeviceFeatures(*integrated, &ctx->gpufeats);
    }
    else if(cpu)
    {
      ctx->gpu= *cpu;  // lmao
      vkGetPhysicalDeviceProperties(*cpu, &ctx->gpuprops);
      vkGetPhysicalDeviceFeatures(*cpu, &ctx->gpufeats);
    }
    else
      goto fail;
  }

  // queues
  {
    vkGetPhysicalDeviceQueueFamilyProperties(
      ctx->gpu, &ctx->nrqfams, NULL);
    vkGetPhysicalDeviceQueueFamilyProperties(
      ctx->gpu, &ctx->nrqfams, ctx->qfams);

    ctx->gfxqfamidx= ctx->prsqfamidx= UINT32_MAX;
    for(unsigned i= 0; i < ctx->nrqfams; i++)
    {
      if(ctx->qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        ctx->gfxqfamidx= i;

      VkBool32 present= VK_FALSE;
      VkResult res    = vkGetPhysicalDeviceSurfaceSupportKHR(
        ctx->gpu, i, ctx->surf, &present);
      OK(res, goto fail;)

      if(present) ctx->prsqfamidx= i;

      if(ctx->gfxqfamidx != UINT32_MAX && ctx->prsqfamidx != UINT32_MAX)
        break;
    }
    if(ctx->gfxqfamidx == UINT32_MAX || ctx->prsqfamidx == UINT32_MAX)
      goto fail;

    if(ctx->gfxqfamidx != ctx->prsqfamidx)
      ctx->gfxprs= 0;
    else
      ctx->gfxprs= 1;
  }

  // device
  {
    static f32 priorities[]= {1.f};

    VkDeviceQueueCreateInfo devqs[2];
    u32                     nrqs= 1;

    devqs[0]= (VkDeviceQueueCreateInfo){
      .sType           = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex= ctx->gfxqfamidx,
      .queueCount      = 1,
      .pQueuePriorities= priorities};
    if(ctx->gfxqfamidx != ctx->prsqfamidx)
    {
      nrqs++;
      devqs[1]= (VkDeviceQueueCreateInfo){
        .sType           = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex= ctx->prsqfamidx,
        .queueCount      = 1,
        .pQueuePriorities= priorities};
    }
    static const char *devext[]= {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME};
    ctx->devextensions= devext;

    VkDeviceCreateInfo devinfo= {
      .sType                  = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount   = nrqs,
      .pQueueCreateInfos      = devqs,
      .enabledExtensionCount  = 2,
      .ppEnabledExtensionNames= devext,
      .pEnabledFeatures       = &ctx->gpufeats  // NULL for no feats
    };

    VkResult res= vkCreateDevice(ctx->gpu, &devinfo, NULL, &ctx->dev);
    OK(res, goto fail;)

    vkGetDeviceQueue(ctx->dev, ctx->gfxqfamidx, 0, &ctx->gfxq);
    vkGetDeviceQueue(ctx->dev, ctx->prsqfamidx, 0, &ctx->prsq);
  }

  // swapchain
  {
    VkResult res= vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      ctx->gpu, ctx->surf, &ctx->gpuscaps);
    OK(res, goto fail;)

    res= vkGetPhysicalDeviceSurfaceFormatsKHR(
      ctx->gpu, ctx->surf, &ctx->nrgpusfmts, NULL);
    OK(res, goto fail;)
    res= vkGetPhysicalDeviceSurfaceFormatsKHR(
      ctx->gpu, ctx->surf, &ctx->nrgpusfmts, ctx->gpusfmts);
    OK(res, goto fail;)

    res= vkGetPhysicalDeviceSurfacePresentModesKHR(
      ctx->gpu, ctx->surf, &ctx->nrprsmodes, NULL);
    OK(res, goto fail;)
    if(ctx->nrprsmodes > 8) ctx->nrprsmodes= 8;

    res= vkGetPhysicalDeviceSurfacePresentModesKHR(
      ctx->gpu, ctx->surf, &ctx->nrprsmodes, ctx->prsmodes);
    OK(res, goto fail;)

    const VkSurfaceFormatKHR *chosen= &ctx->gpusfmts[0];
    for(unsigned i= 0; i < ctx->nrgpusfmts; i++)
      if(
        ctx->gpusfmts[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
        ctx->gpusfmts[i].colorSpace ==
          VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      {
        chosen= &ctx->gpusfmts[i];
        break;
      }

    VkExtent2D extent;
    if(
      ctx->gpuscaps.currentExtent.width == UINT32_MAX &&
      ctx->gpuscaps.currentExtent.height == UINT32_MAX)
    {
      extent.width= 800 > ctx->gpuscaps.maxImageExtent.width
                      ? ctx->gpuscaps.maxImageExtent.width
                      : 800;

      extent.height= 600 > ctx->gpuscaps.maxImageExtent.height
                       ? ctx->gpuscaps.maxImageExtent.height
                       : 600;
    }
    else
      extent= ctx->gpuscaps.currentExtent;
    const VkPresentModeKHR *mode= NULL;
    for(unsigned i= 0; i < ctx->nrprsmodes; i++)
    {
      if(ctx->prsmodes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        mode= &ctx->prsmodes[i];
    }
    if(!mode) mode= &ctx->prsmodes[0];

    VkSwapchainCreateInfoKHR info= {
      .sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface         = ctx->surf,
      .minImageCount   = ctx->gpuscaps.minImageCount + 1,
      .imageFormat     = chosen->format,
      .imageColorSpace = chosen->colorSpace,
      .imageExtent     = extent,
      .imageArrayLayers= 1,
      .imageUsage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode= ctx->gfxprs ? VK_SHARING_MODE_EXCLUSIVE
                                     : VK_SHARING_MODE_CONCURRENT,
      .queueFamilyIndexCount= ctx->gfxprs ? 0 : 2,
      .pQueueFamilyIndices  = ctx->gfxprs ? NULL : &ctx->gfxqfamidx,
      .preTransform         = ctx->gpuscaps.currentTransform,
      .compositeAlpha       = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode          = *mode,
      .clipped              = VK_TRUE,
      .oldSwapchain         = VK_NULL_HANDLE};

    res= vkCreateSwapchainKHR(ctx->dev, &info, NULL, &ctx->swp);
    OK(res, goto fail;)
    ctx->swpsfmt= *chosen;
  }

  // img views
  {
    VkResult res= vkGetSwapchainImagesKHR(
      ctx->dev, ctx->swp, &ctx->nrswpimgs, NULL);
    OK(res, goto fail;)
    res= vkGetSwapchainImagesKHR(
      ctx->dev, ctx->swp, &ctx->nrswpimgs, ctx->swpimgs);
    OK(res, goto fail;)

    for(unsigned i= 0; i < ctx->nrswpimgs; i++)
    {
      VkImageViewCreateInfo info= {
        .sType           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image           = ctx->swpimgs[i],
        .viewType        = VK_IMAGE_VIEW_TYPE_2D,
        .format          = ctx->swpsfmt.format,
        .subresourceRange= {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
      res= vkCreateImageView(ctx->dev, &info, NULL, &ctx->swpimgvs[i]);
      OK(res, goto fail;)
    }
  }
  return ctx;
fail:
  puts("VK init failed");
  return NULL;
}

static void term(vkctx *ctx)
{
  if(!ctx) return;

  for(unsigned i= 0; i < ctx->nrswpimgs; i++)
    if(ctx->swpimgvs[i])
      vkDestroyImageView(ctx->dev, ctx->swpimgvs[i], NULL);
  if(ctx->swp) vkDestroySwapchainKHR(ctx->dev, ctx->swp, NULL);
  if(ctx->dev) vkDestroyDevice(ctx->dev, NULL);
  if(ctx->surf) vkDestroySurfaceKHR(ctx->inst, ctx->surf, NULL);
  if(ctx->inst) vkDestroyInstance(ctx->inst, NULL);
  free(ctx);
}

gfxctx *gfxinit(void *dpy) { return init(dpy); }
void    gfxterm(gfxctx *ctx) {  term(ctx); }

int main(void)
{
  dpyctx *dpy= dpyinit("title", 800, 600, 0);
  vkctx  *ctx= init(dpy);
exit:
  free(ctx);
  dpyterm(dpy);
}
