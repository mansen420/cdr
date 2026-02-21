#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
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

  i32 gfxqfamidx;  // God queues
  i32 prsqfamidx;

  // info
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

    ctx->gfxqfamidx= ctx->prsqfamidx= -1;
    for(unsigned i= 0; i < ctx->nrqfams; i++)
    {
      if(ctx->qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        ctx->gfxqfamidx= i;

      VkBool32 present= VK_FALSE;
      VkResult res    = vkGetPhysicalDeviceSurfaceSupportKHR(
        ctx->gpu, i, ctx->surf, &present);
      OK(res, goto fail;)

      if(present) ctx->prsqfamidx= i;

      if(ctx->gfxqfamidx >= 0 && ctx->prsqfamidx >= 0) break;
    }
    if(ctx->gfxqfamidx < 0 || ctx->prsqfamidx < 0) goto fail;
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
    static const char *devext[]= {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    ctx->devextensions         = devext;

    VkDeviceCreateInfo devinfo= {
      .sType                  = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount   = nrqs,
      .pQueueCreateInfos      = devqs,
      .enabledExtensionCount  = 1,
      .ppEnabledExtensionNames= devext,
      .pEnabledFeatures       = &ctx->gpufeats  // NULL for no feats
    };

    VkResult res= vkCreateDevice(ctx->gpu, &devinfo, NULL, &ctx->dev);
    OK(res, goto fail;)

    vkGetDeviceQueue(ctx->dev, ctx->gfxqfamidx, 0, &ctx->gfxq);
    vkGetDeviceQueue(ctx->dev, ctx->prsqfamidx, 0, &ctx->prsq);
  }
	
  return ctx;
fail:
  puts("VK init failed");
  return NULL;
}

gfxctx *gfxinit(void *dpy) { return init(dpy); }

int main(void)
{
  dpyctx *dpy= dpyinit("title", 800, 600, 0);
  vkctx  *ctx= init(dpy);

exit:
  free(ctx);
  dpyterm(dpy);
}
