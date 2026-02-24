#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#define MAX_SHADERS 16

#define VKNULL VK_NULL_HANDLE

static const char *const VK_LAYER_VALID_NAME= "VK_LAYER_KHRONOS_validation";

typedef struct vkfrmctx
{
  VkCommandBuffer cmdbuf;
  VkSemaphore     renderfin, imggotten;
  VkFence         fence;
} frmctx;

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
  VkShaderModule   shaders[MAX_SHADERS];  // who needs more than 16 shaders
  u32              nrshaders;
  VkPipelineLayout layout;
  VkPipeline       gfxppl;

  // cmdpool stuff
  VkCommandPool gfxcmdpool;
  // may be VK_NULL_HANDLE, in which case
  // gfxpool supports presentation
  VkCommandPool prscmdpool;

  // XXX keep these in contiguous memory and keep gfx first. see:
  // swapchain creation
  u32 gfxqfamidx;  // God queues
  u32 prsqfamidx;

  VkExtent2D swpextent;

  // frames
  frmctx frames[8];
  u32    nrframes;

  // info

  // 1 if graphics and present queues indices are the same
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
    OK(res, goto err;)
  }
  // wayland surface
  {
    VkWaylandSurfaceCreateInfoKHR info= {
      .sType  = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
      .display= dpygetncon(dpy),
      .surface= dpygetnwin(dpy),
    };
    VkResult res= vkCreateWaylandSurfaceKHR(ctx->inst, &info, NULL, &ctx->surf);
    OK(res, goto err;)
  }
  // pick GPU
  {
    u32              nphy= 0;
    VkPhysicalDevice phy[8];  // at most 8 devices

    VkResult res= vkEnumeratePhysicalDevices(ctx->inst, &nphy, NULL);
    OK(res, goto err;)
    if(nphy == 0) goto err;

    nphy= nphy > 8 ? 8 : nphy;  // clamp

    res= vkEnumeratePhysicalDevices(ctx->inst, &nphy, phy);
    OK(res, goto err;)

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

      if(discrete) break;
    }

    ctx->gpufeats= (VkPhysicalDeviceFeatures2){
      .sType= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      .pNext= &ctx->gpufeats11};
    ctx->gpufeats11= (VkPhysicalDeviceVulkan11Features){
      .sType= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .pNext= &ctx->gpufeats12};
    ctx->gpufeats12= (VkPhysicalDeviceVulkan12Features){
      .sType= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
      .pNext= &ctx->gpufeats13};
    ctx->gpufeats13= (VkPhysicalDeviceVulkan13Features){
      .sType= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext= &ctx->gpufeats14};
    ctx->gpufeats14= (VkPhysicalDeviceVulkan14Features){
      .sType= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};

    if(discrete)
    {
      ctx->gpu= *discrete;
      vkGetPhysicalDeviceProperties(*discrete, &ctx->gpuprops);
      vkGetPhysicalDeviceFeatures2(*discrete, &ctx->gpufeats);
    }
    else if(integrated)
    {
      ctx->gpu= *integrated;
      vkGetPhysicalDeviceProperties(*integrated, &ctx->gpuprops);
      vkGetPhysicalDeviceFeatures2(*integrated, &ctx->gpufeats);
    }
    else if(cpu)
    {
      ctx->gpu= *cpu;  // lmao
      vkGetPhysicalDeviceProperties(*cpu, &ctx->gpuprops);
      vkGetPhysicalDeviceFeatures2(*cpu, &ctx->gpufeats);
    }
    else
      goto err;
  }
  // queues
  {
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->gpu, &ctx->nrqfams, NULL);
    vkGetPhysicalDeviceQueueFamilyProperties(
      ctx->gpu, &ctx->nrqfams, ctx->qfams);

    ctx->gfxqfamidx= ctx->prsqfamidx= UINT32_MAX;
    for(unsigned i= 0; i < ctx->nrqfams; i++)
    {
      if(ctx->qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) ctx->gfxqfamidx= i;

      VkBool32 present= VK_FALSE;
      VkResult res=
        vkGetPhysicalDeviceSurfaceSupportKHR(ctx->gpu, i, ctx->surf, &present);
      OK(res, goto err;)

      if(present) ctx->prsqfamidx= i;

      if(ctx->gfxqfamidx != UINT32_MAX && ctx->prsqfamidx != UINT32_MAX) break;
    }
    if(ctx->gfxqfamidx == UINT32_MAX || ctx->prsqfamidx == UINT32_MAX) goto err;

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
    static const char *devext[]= {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    ctx->devextensions         = devext;

    VkPhysicalDeviceVulkan11Features feats11= {
      .sType= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
      .shaderDrawParameters= VK_TRUE,
    };
    VkPhysicalDeviceVulkan13Features feats13= {
      .sType           = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext           = &feats11,
      .dynamicRendering= VK_TRUE,
      .synchronization2= VK_TRUE};

    VkDeviceCreateInfo devinfo= {
      .sType                  = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount   = nrqs,
      .pQueueCreateInfos      = devqs,
      .enabledExtensionCount  = 1,
      .ppEnabledExtensionNames= devext,
      .pEnabledFeatures       = NULL,
      .pNext                  = &feats13};

    VkResult res= vkCreateDevice(ctx->gpu, &devinfo, NULL, &ctx->dev);
    OK(res, goto err;)

    vkGetDeviceQueue(ctx->dev, ctx->gfxqfamidx, 0, &ctx->gfxq);
    vkGetDeviceQueue(ctx->dev, ctx->prsqfamidx, 0, &ctx->prsq);
  }
  // swapchain
  {
    VkResult res= vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      ctx->gpu, ctx->surf, &ctx->gpuscaps);
    OK(res, goto err;)

    res= vkGetPhysicalDeviceSurfaceFormatsKHR(
      ctx->gpu, ctx->surf, &ctx->nrgpusfmts, NULL);
    OK(res, goto err;)
    res= vkGetPhysicalDeviceSurfaceFormatsKHR(
      ctx->gpu, ctx->surf, &ctx->nrgpusfmts, ctx->gpusfmts);
    OK(res, goto err;)

    res= vkGetPhysicalDeviceSurfacePresentModesKHR(
      ctx->gpu, ctx->surf, &ctx->nrprsmodes, NULL);
    OK(res, goto err;)
    if(ctx->nrprsmodes > 8) ctx->nrprsmodes= 8;

    res= vkGetPhysicalDeviceSurfacePresentModesKHR(
      ctx->gpu, ctx->surf, &ctx->nrprsmodes, ctx->prsmodes);
    OK(res, goto err;)

    const VkSurfaceFormatKHR *chosen= &ctx->gpusfmts[0];
    for(unsigned i= 0; i < ctx->nrgpusfmts; i++)
      if(
        ctx->gpusfmts[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
        ctx->gpusfmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
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
      .minImageCount   = ctx->gpuscaps.minImageCount + 1,  // XXX may break
      .imageFormat     = chosen->format,
      .imageColorSpace = chosen->colorSpace,
      .imageExtent     = extent,
      .imageArrayLayers= 1,
      .imageUsage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode=
        ctx->gfxprs ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
      .queueFamilyIndexCount= ctx->gfxprs ? 0 : 2,
      .pQueueFamilyIndices  = ctx->gfxprs ? NULL : &ctx->gfxqfamidx,
      .preTransform         = ctx->gpuscaps.currentTransform,
      .compositeAlpha       = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode          = *mode,
      .clipped              = VK_TRUE,
      .oldSwapchain         = VK_NULL_HANDLE};

    res= vkCreateSwapchainKHR(ctx->dev, &info, NULL, &ctx->swp);
    OK(res, goto err;)
    ctx->swpsfmt  = *chosen;
    ctx->swpextent= extent;
  }
  // img views
  {
    VkResult res=
      vkGetSwapchainImagesKHR(ctx->dev, ctx->swp, &ctx->nrswpimgs, NULL);
    OK(res, goto err;)
    res= vkGetSwapchainImagesKHR(
      ctx->dev, ctx->swp, &ctx->nrswpimgs, ctx->swpimgs);
    OK(res, goto err;)

    for(unsigned i= 0; i < ctx->nrswpimgs; i++)
    {
      VkImageViewCreateInfo info= {
        .sType           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image           = ctx->swpimgs[i],
        .viewType        = VK_IMAGE_VIEW_TYPE_2D,
        .format          = ctx->swpsfmt.format,
        .subresourceRange= {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
      res= vkCreateImageView(ctx->dev, &info, NULL, &ctx->swpimgvs[i]);
      OK(res, goto err;)
    }
  }
  // cmd pools
  {
    VkResult                res;
    VkCommandPoolCreateInfo info= {
      .sType           = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags           = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex= ctx->gfxqfamidx};
    res= vkCreateCommandPool(ctx->dev, &info, NULL, &ctx->gfxcmdpool);
    OK(res, goto err;)

    if(!ctx->gfxprs)
    {
      info= (VkCommandPoolCreateInfo){
        .sType           = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags           = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex= ctx->prsqfamidx};
      res= vkCreateCommandPool(ctx->dev, &info, NULL, &ctx->prscmdpool);
      OK(res, goto err;)
    }
    else
      ctx->prscmdpool= VKNULL;
  }

  return ctx;
err:
	gfxterm(ctx);
  puts("VK init erred");
  return NULL;
}

static void frmterm(vkctx *ctx, u32 idx)
{
  if(idx >= ctx->nrframes) goto err;

  frmctx *frm= &ctx->frames[idx];
  if(frm->fence) vkDestroyFence(ctx->dev, frm->fence, NULL);
  if(frm->renderfin) vkDestroySemaphore(ctx->dev, frm->renderfin, NULL);
  if(frm->imggotten) vkDestroySemaphore(ctx->dev, frm->imggotten, NULL);

  return;
err:
  return;
}

static void term(vkctx *ctx)
{
  if(!ctx) return;
  vkDeviceWaitIdle(ctx->dev);

  for(unsigned i= 0; i < ctx->nrframes; i++) frmterm(ctx, i);
  if(ctx->prscmdpool) vkDestroyCommandPool(ctx->dev, ctx->prscmdpool, NULL);
  if(ctx->gfxcmdpool) vkDestroyCommandPool(ctx->dev, ctx->gfxcmdpool, NULL);
  for(unsigned i= 0; i < ctx->nrshaders; i++)
    vkDestroyShaderModule(ctx->dev, ctx->shaders[i], NULL);
  if(ctx->gfxppl) vkDestroyPipeline(ctx->dev, ctx->gfxppl, NULL);
  if(ctx->layout) vkDestroyPipelineLayout(ctx->dev, ctx->layout, NULL);
  for(unsigned i= 0; i < ctx->nrswpimgs; i++)
    if(ctx->swpimgvs[i]) vkDestroyImageView(ctx->dev, ctx->swpimgvs[i], NULL);
  if(ctx->swp) vkDestroySwapchainKHR(ctx->dev, ctx->swp, NULL);
  if(ctx->dev) vkDestroyDevice(ctx->dev, NULL);
  if(ctx->surf) vkDestroySurfaceKHR(ctx->inst, ctx->surf, NULL);
  if(ctx->inst) vkDestroyInstance(ctx->inst, NULL);

	memset(ctx, 0, sizeof *ctx);
  free(ctx);
}

static int mkgfxppl(vkctx *ctx, const u32 *shcode, u32 shlen)
{
  if(!ctx || !shcode) goto err;
  // shader
  {
    VkShaderModuleCreateInfo info= {
      .sType   = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize= shlen,
      .pCode   = shcode};
    if(ctx->nrshaders >= MAX_SHADERS) goto err;

    VkResult res= vkCreateShaderModule(
      ctx->dev, &info, NULL, &ctx->shaders[ctx->nrshaders++]);
    OK(res, goto err;)
  }
  VkShaderModule module= ctx->shaders[ctx->nrshaders - 1];

  VkPipelineShaderStageCreateInfo stages[2];

  stages[0]= (VkPipelineShaderStageCreateInfo){
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module= module,
    .pName = "vertMain"};
  stages[1]= (VkPipelineShaderStageCreateInfo){
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module= module,
    .pName = "fragMain"};

  VkPipelineVertexInputStateCreateInfo inputinfo= {
    .sType= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

  VkPipelineInputAssemblyStateCreateInfo asminfo= {
    .sType   = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

  VkDynamicState dynstate[]= {
    VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dyninfo= {
    .sType            = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
    .dynamicStateCount= 2,
    .pDynamicStates   = dynstate};

  VkPipelineViewportStateCreateInfo vpinfo= {
    .sType        = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount= 1,
    .scissorCount = 1};

  VkPipelineRasterizationStateCreateInfo rstate= {
    .sType      = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode= VK_POLYGON_MODE_FILL,
    .cullMode   = VK_CULL_MODE_BACK_BIT,
    .frontFace  = VK_FRONT_FACE_CLOCKWISE,
    .lineWidth  = 1.f};

  VkPipelineMultisampleStateCreateInfo msinfo= {
    .sType= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples= VK_SAMPLE_COUNT_1_BIT};

  VkPipelineColorBlendAttachmentState blendattinfo= {
    .blendEnable   = VK_FALSE,
    .colorWriteMask= VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT |
                     VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_R_BIT};
  VkPipelineColorBlendStateCreateInfo blendinfo= {
    .sType          = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable  = VK_FALSE,
    .attachmentCount= 1,
    .pAttachments   = &blendattinfo};

  VkPipelineLayoutCreateInfo plinfo= {
    .sType= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  vkCreatePipelineLayout(ctx->dev, &plinfo, NULL, &ctx->layout);

  VkPipelineRenderingCreateInfo rinfo= {
    .sType                  = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .colorAttachmentCount   = 1,
    .pColorAttachmentFormats= &ctx->swpsfmt.format};

  VkGraphicsPipelineCreateInfo gfxinfo= {
    .sType              = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext              = &rinfo,
    .stageCount         = 2,
    .pStages            = stages,
    .pVertexInputState  = &inputinfo,
    .pInputAssemblyState= &asminfo,
    .pViewportState     = &vpinfo,
    .pRasterizationState= &rstate,
    .pMultisampleState  = &msinfo,
    .pColorBlendState   = &blendinfo,
    .pDynamicState      = &dyninfo,
    .layout             = ctx->layout};
  VkResult res= vkCreateGraphicsPipelines(
    ctx->dev, VK_NULL_HANDLE, 1, &gfxinfo, NULL, &ctx->gfxppl);
  OK(res, goto err;);

  return 0;
err:
  puts("Vulkan pipeline creation failed");
  return -1;
}

gfxctx *gfxinit(void *dpy) { return init(dpy); }
void    gfxterm(gfxctx *ctx) { term(ctx); }

static int frminit(vkctx *ctx)
{
  if(!ctx) goto err;

  u32 n= ctx->nrswpimgs;
  for(unsigned i= 0; i < n; i++)
  {
    frmctx *frm= &ctx->frames[i];

    VkCommandBufferAllocateInfo allocinfo= {
      .sType             = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool       = ctx->gfxcmdpool,
      .level             = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount= 1};
    VkResult res= vkAllocateCommandBuffers(ctx->dev, &allocinfo, &frm->cmdbuf);
    OK(res, goto err;)

    VkSemaphoreCreateInfo seminfo= {
      .sType= VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    res= vkCreateSemaphore(ctx->dev, &seminfo, NULL, &frm->imggotten);
    OK(res, goto err;)
    res= vkCreateSemaphore(ctx->dev, &seminfo, NULL, &frm->renderfin);
    OK(res, goto err;)

    VkFenceCreateInfo feninfo= {
      .sType= VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags= VK_FENCE_CREATE_SIGNALED_BIT};
    res= vkCreateFence(ctx->dev, &feninfo, NULL, &frm->fence);
    OK(res, goto err;)
  }
  ctx->nrframes= n;
  return 0;
err:
  return -1;
}

static int render(vkctx *ctx)
{
  static u32 frmidx= 0;
  frmctx    *frm   = &ctx->frames[frmidx];

  vkWaitForFences(ctx->dev, 1, &frm->fence, VK_TRUE, UINT64_MAX);
  vkResetFences(ctx->dev, 1, &frm->fence);

  VkSemaphore acqsem= frm->imggotten;
  u32         imgidx;
  VkResult    res= vkAcquireNextImageKHR(
    ctx->dev, ctx->swp, UINT64_MAX, acqsem, VKNULL, &imgidx);
  OK(res, goto err;)

  VkSemaphore subsem= ctx->frames[imgidx].renderfin;
  // submit work
  {
    // record cmds
    {
      vkResetCommandBuffer(frm->cmdbuf, 0);
      VkCommandBufferBeginInfo binfo= {
        .sType= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
      vkBeginCommandBuffer(frm->cmdbuf, &binfo);

      VkImageMemoryBarrier2 renderb= {
        .sType              = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask       = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .dstStageMask       = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask      = VK_ACCESS_2_NONE,
        .dstAccessMask      = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout          = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout          = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex= VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex= VK_QUEUE_FAMILY_IGNORED,
        .image              = ctx->swpimgs[imgidx],
        .subresourceRange   = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
      VkDependencyInfo renderd= {
        .sType                  = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount= 1,
        .pImageMemoryBarriers   = &renderb};

      vkCmdPipelineBarrier2(frm->cmdbuf, &renderd);

      VkRenderingAttachmentInfo attinfo= {
        .sType      = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView  = ctx->swpimgvs[imgidx],
        .imageLayout= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp    = VK_ATTACHMENT_STORE_OP_STORE};
      VkRenderingInfo renderinfo= {
        .sType               = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea          = {{0, 0}, ctx->swpextent},
        .colorAttachmentCount= 1,
        .pColorAttachments   = &attinfo,
        .layerCount          = 1};
      vkCmdBeginRendering(frm->cmdbuf, &renderinfo);

      vkCmdBindPipeline(
        frm->cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, ctx->gfxppl);
      VkViewport vp= {
        0, 0, (f32)ctx->swpextent.width, (f32)ctx->swpextent.height, 0.f, 1.f};
      vkCmdSetViewport(frm->cmdbuf, 0, 1, &vp);
      VkRect2D scissor= {.extent= ctx->swpextent};
      vkCmdSetScissor(frm->cmdbuf, 0, 1, &scissor);

      vkCmdDraw(frm->cmdbuf, 3, 1, 0, 0);

      vkCmdEndRendering(frm->cmdbuf);

      VkImageMemoryBarrier2 prsb= {
        .sType           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask    = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask   = VK_ACCESS_2_NONE,
        .oldLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image           = ctx->swpimgs[imgidx],
        .subresourceRange= {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
      VkDependencyInfo prsd= {
        .sType                  = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount= 1,
        .pImageMemoryBarriers   = &prsb};

      vkCmdPipelineBarrier2(frm->cmdbuf, &prsd);

      vkEndCommandBuffer(frm->cmdbuf);
    }
    VkSemaphoreSubmitInfo waitinfo= {
      .sType    = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .stageMask= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .semaphore= acqsem};
    VkSemaphoreSubmitInfo siginfo= {
      .sType    = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
      .stageMask= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .semaphore= subsem};
    VkCommandBufferSubmitInfo cmdinfo= {
      .sType        = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .commandBuffer= frm->cmdbuf};
    VkSubmitInfo2 subinfo= {
      .sType                   = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .waitSemaphoreInfoCount  = 1,
      .pWaitSemaphoreInfos     = &waitinfo,
      .signalSemaphoreInfoCount= 1,
      .pSignalSemaphoreInfos   = &siginfo,
      .commandBufferInfoCount  = 1,
      .pCommandBufferInfos     = &cmdinfo};
    res= vkQueueSubmit2(ctx->gfxq, 1, &subinfo, frm->fence);
    OK(res, goto err;)
  }
  // present
  {
    VkPresentInfoKHR presinfo= {
      .sType             = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount= 1,
      .pWaitSemaphores   = &subsem,
      .swapchainCount    = 1,
      .pSwapchains       = &ctx->swp,
      .pImageIndices     = &imgidx};
    vkQueuePresentKHR(ctx->prsq, &presinfo);
  }

  frmidx= (frmidx + 1) % ctx->nrframes;
  return 0;
err:
  puts("Failure during frame rendering");
  return -1;
}

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv)
{
  int     rc = 0;
  dpyctx *dpy= dpyinit("title", 800, 600, 0);
  vkctx  *ctx= gfxinit(dpy);

  u32 *shader= NULL;
  u32  nbytes= 0;
  {
    const char *path= argc > 1 ? argv[1] : "./assets/shader.spv";

    int fd= open(path, O_RDONLY);
    if(fd == -1)
    {
      puts("IO failure");
      goto exit;
    }
    struct stat st;
    fstat(fd, &st);
    nbytes = st.st_size;
    shader = malloc(nbytes);
    i64 rdb= read(fd, shader, nbytes);
    if(rdb != nbytes) puts("IO warning");
    close(fd);
  }
  rc= mkgfxppl(ctx, shader, nbytes);
  rc= frminit(ctx);
  OK(rc, goto exit;)

  while(dpypoll(dpy) == 0) render(ctx);

  vkDeviceWaitIdle(ctx->dev);
exit:
  gfxterm(ctx);
  dpyterm(dpy);
  free(shader);
  return rc;
}
