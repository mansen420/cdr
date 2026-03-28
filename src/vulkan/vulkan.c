#include "src/vulkan/vulkan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan_core.h>

#include "include/display.h"

static const char *const VK_LAYER_VALID_NAME= "VK_LAYER_KHRONOS_validation";

static int init(vkctx **out, void *dpy, gfxdesc desc)
{
  vkctx *ctx= calloc(1, sizeof *ctx);

  // instance
  {
    VkInstanceCreateInfo info= {0};
    info.sType               = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    // XXX these are created once statically, if we have multiple
    // contexts, they will share the same layer info. just copy the layers
    // into the context
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
      .display= dpyget_ncon(dpy),
      .surface= dpyget_nwin(dpy),
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

  // default memory pool
  if(desc.devmem)
  {
    VkMemoryPropertyFlags rbar_flags=
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if(arenainit(&ctx->dfltpool, ctx, desc.devmem, rbar_flags) < 0)
      OK(arenainit(&ctx->dfltpool, ctx, desc.devmem,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
         goto err;)
  }

  ctx->alive= 1;
  *out      = ctx;
  return 0;
err:
  gfxterm(ctx);
  puts("VK init erred");
  return -1;
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

  if(ctx->vtxbuf) vkDestroyBuffer(ctx->dev, ctx->vtxbuf, NULL);
  arenaterm(ctx, &ctx->dfltpool);
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

static int initgfxppl(vkctx *ctx, const u32 *shcode, u32 shlen)
{
  if(!ctx || !shcode) goto err;
  // shader
  {
    VkShaderModuleCreateInfo info= {
      .sType   = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize= shlen,
      .pCode   = shcode};
    if(ctx->nrshaders >= SHADERS_MAX) goto err;

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

  VkVertexInputBindingDescription bind= {
    .binding  = 0,
    .stride   = sizeof(vertex),
    .inputRate= VK_VERTEX_INPUT_RATE_VERTEX};
  VkVertexInputAttributeDescription attrs[2];
  attrs[0]= (VkVertexInputAttributeDescription){
    .location= 0,
    .binding = 0,
    .format  = VK_FORMAT_R32G32_SFLOAT,
    offsetof(vertex, pos)};
  attrs[1]= (VkVertexInputAttributeDescription){
    .location= 1,
    .binding = 0,
    .format  = VK_FORMAT_R32G32B32_SFLOAT,
    offsetof(vertex, color)};

  VkPipelineVertexInputStateCreateInfo inputinfo= {
    .sType= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount  = 1,
    .pVertexBindingDescriptions     = &bind,
    .vertexAttributeDescriptionCount= 2,
    .pVertexAttributeDescriptions   = attrs};

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

int  gfxinit(gfxctx **out, dpyctx *dpy, gfxdesc desc)
{ return init(out, dpy, desc); }
void gfxterm(gfxctx *ctx) { term(ctx); }
int  gfxqry_init(gfxctx *ctx) { return ctx && ctx->alive; }
void gfxpause(gfxctx *ctx) { ctx->pause= 1; }
void gfxresume(gfxctx *ctx) { ctx->pause= 0; }
int  gfxrender(gfxctx *ctx, gfxpass *passes, u32 nrpasses)
{
  (void)passes;
  (void)nrpasses;
  return render(ctx);
}
void gfxupdate(gfxctx *ctx, i32 w, i32 h)
{
  vkDeviceWaitIdle(ctx->dev);
  reinitswp(ctx, w, h);
}

static void term_vtxbuf(gfxctx *ctx, gfxbuf *buf)
{
  if(!ctx || !buf) return;
  if(buf->buf) vkDestroyBuffer(ctx->dev, buf->buf, NULL);
  free(buf);
}

static VkBuffer updata(vkctx *ctx, vkarena *dev, void *data, usize bytes)
{
  vkarena host= {0};
  int     has_host= 0;
  VkBuffer buf= VKNULL;

  if(!ctx || !dev || !data) goto err;
  if(!bytes) goto exit;

  VkMemoryPropertyFlags rbar=
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  if((dev->flags & rbar) == rbar)
    buf= updata_direct(ctx, dev, data, bytes);
  else
  {
    OK(arenainit(&host, ctx, bytes, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT),
       goto err;)
    has_host= 1;
    buf= updata_staged(ctx, &host, dev, data, bytes);
  }
exit:
  if(has_host) arenaterm(ctx, &host);
  return buf;
err:
  if(has_host) arenaterm(ctx, &host);
  return VKNULL;
}

static int init_vtxbuf(
  gfxbuf **out, gfxctx *ctx, gfxmem *mem, gfxdesc_vtxbuf desc, const void *data)
{
  gfxbuf *buf= NULL;
  if(!ctx || !mem || !data) goto err;

  buf= calloc(1, sizeof *buf);
  if(!buf) goto err;

  buf->buf= updata(ctx, mem, (void *)data, (usize)desc.nrvtx * desc.vtxstride);
  if(!buf->buf) goto err;

  buf->desc= desc;

  *out= buf;
  return 0;
err:
  term_vtxbuf(ctx, buf);
  return -1;
}

int gfxinit_vtxbuf(
  gfxbuf **o, gfxctx *c, gfxmem *m, gfxdesc_vtxbuf d, const void *data)
{ return init_vtxbuf(o, c, m, d, data); }
void gfxterm_vtxbuf(gfxctx *c, gfxbuf *b) { term_vtxbuf(c, b); }

static int init_mem(gfxmem **out, vkctx *ctx, usize bytes)
{
  gfxmem *mem= NULL;
  if(!out || !ctx || !bytes) goto err;

  mem= calloc(1, sizeof *mem);
  if(!mem) goto err;

  VkMemoryPropertyFlags rbar_flags=
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  if(arenainit(mem, ctx, bytes, rbar_flags) < 0)
    OK(arenainit(mem, ctx, bytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
       goto err;)

  *out= mem;
  return 0;
err:
  free(mem);
  return -1;
}

static void term_mem(vkctx *ctx, gfxmem *mem)
{
  if(!ctx || !mem) return;
  arenaterm(ctx, mem);
  free(mem);
}

int  gfxinit_mem(gfxmem **o, gfxctx *c, usize bytes)
{ return init_mem(o, c, bytes); }
void gfxterm_mem(gfxctx *c, gfxmem *m) { term_mem(c, m); }

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

// caller must synchronise
static int reinitswp(vkctx *ctx, i32 w_new, i32 h_new)
{
  if(!ctx) goto err;

  for(unsigned i= 0; i < ctx->nrswpimgs; i++)
    vkDestroyImageView(ctx->dev, ctx->swpimgvs[i], NULL);
  memset(&ctx->swpimgvs, 0, sizeof ctx->swpimgvs);

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

    // TODO replace (800, 600) with paramterized defaults
    VkExtent2D extent;
    if(
      ctx->gpuscaps.currentExtent.width == UINT32_MAX &&
      ctx->gpuscaps.currentExtent.height == UINT32_MAX)
    {
      extent.width= w_new > ctx->gpuscaps.maxImageExtent.width
                      ? ctx->gpuscaps.maxImageExtent.width
                      : w_new;

      extent.height= h_new > ctx->gpuscaps.maxImageExtent.height
                       ? ctx->gpuscaps.maxImageExtent.height
                       : h_new;
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
      .oldSwapchain         = ctx->swp};

    VkSwapchainKHR temp;
    res= vkCreateSwapchainKHR(ctx->dev, &info, NULL, &temp);
    OK(res, goto err;)
    vkDestroySwapchainKHR(ctx->dev, ctx->swp, NULL);
    ctx->swp      = temp;
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
  return 0;
err:
  return -1;
}

static int render(vkctx *ctx)
{
  if(ctx->pause) return 0;

  VkResult res= VK_SUCCESS;

  frmctx *frm= &ctx->frames[ctx->frmidx];

  vkWaitForFences(ctx->dev, 1, &frm->fence, VK_TRUE, UINT64_MAX);

  // print diagnostics
  {
    static const u64 PRINT_INTERVAL_NS= (1000000000ULL / 15);
    struct timespec  t_now;
    clock_gettime(CLOCK_MONOTONIC, &t_now);
    if(ctx->diag_t_last.tv_sec || ctx->diag_t_last.tv_nsec)
    {
      u64 delta_ns=
        (u64)(t_now.tv_sec - ctx->diag_t_last.tv_sec) * 1000000000ULL +
        (u64)(t_now.tv_nsec - ctx->diag_t_last.tv_nsec);
      ctx->diag_acc_ns+= delta_ns;
      ctx->diag_acc_count++;
      if(ctx->diag_acc_ns >= PRINT_INTERVAL_NS)
      {
        double avg_ms = (double)ctx->diag_acc_ns / ctx->diag_acc_count / 1e6;
        double avg_fps= 1e9 / ((double)ctx->diag_acc_ns / ctx->diag_acc_count);
        // comment out the print. later just write this to some frame
        // diagnostics struct
        // printf(
        //   "frame=%-6u  ft=%-4.3fms  fps=%-8.1f\r",
        //   ctx->absfrmidx,
        //   avg_ms,
        //   avg_fps);
        // fflush(stdout);
        ctx->diag_acc_ns   = 0;
        ctx->diag_acc_count= 0;
      }
    }
    ctx->diag_t_last= t_now;
  }

  VkSemaphore acqsem= frm->imggotten;
  u32         imgidx;
  res= vkAcquireNextImageKHR(
    ctx->dev, ctx->swp, UINT64_MAX, acqsem, VKNULL, &imgidx);
  OK(res, goto err;)

  VkSemaphore subsem= ctx->frames[imgidx].renderfin;

  // submit work
  {
    // record cmds
    {
      NNUL(ctx->vtxbuf, goto err;)

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

      VkDeviceSize vtxoff= 0;
      vkCmdBindVertexBuffers(frm->cmdbuf, 0, 1, &ctx->vtxbuf, &vtxoff);
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

    vkResetFences(ctx->dev, 1, &frm->fence);  // unsignal the fence
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
    res= vkQueuePresentKHR(ctx->prsq, &presinfo);
    OK(res, goto err;)
  }

  ctx->frmidx= (ctx->frmidx + 1) % ctx->nrframes;
  ctx->absfrmidx++;
  return 0;
err:
  if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) return 1;
  return -1;
}

static int arenainit(
  vkarena *out, vkctx *ctx, usize bytes, VkMemoryPropertyFlags flags)
{
  if(!out || !ctx) goto err;

  memset(out, 0, sizeof *out);

  VkPhysicalDeviceMemoryProperties memprops;
  vkGetPhysicalDeviceMemoryProperties(ctx->gpu, &memprops);

  // find memory
  int typeidx= -1;
  {
    for(unsigned i= 0; i < memprops.memoryTypeCount; i++)
      if((memprops.memoryTypes[i].propertyFlags & flags) == flags)
      {
        typeidx= i;
        flags  = memprops.memoryTypes[i].propertyFlags;
        break;
      }
  }
  if(typeidx == -1) goto err;

  VkMemoryAllocateInfo allocinfo= {
    .sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = bytes,
    .memoryTypeIndex= typeidx};

  VkDeviceMemory handle= VKNULL;
  VkResult       res   = vkAllocateMemory(ctx->dev, &allocinfo, NULL, &handle);
  OK(res, goto err;)

  out->data   = handle;
  out->size   = bytes;
  out->typeidx= typeidx;
  out->offset = 0;
  out->flags  = flags;

  return 0;
err:
  return -1;
}

static usize alignup(usize addr, usize align)
{
  return (addr + align - 1) & ~(align - 1);
}
static i64 arena_suballoc(vkarena *arena, usize bytes, usize align)
{
  if(!arena) goto err;

  usize aligned      = alignup(arena->offset, align);
  usize alignoverhead= aligned - arena->offset;
  // coulda just checked on aligned >= size, but whatever
  if(alignoverhead >= arena->size - arena->offset) goto err;

  // fragmentation occurs here, would be a good place to log
  arena->slop+= alignoverhead;

  usize free= arena->size - aligned;
  if(bytes > free) goto err;

  i64 res= (i64)aligned;
  arena->offset+= alignoverhead + bytes;

  return res;
err:
  return -1;
}
static i64 memalloc(vkctx *ctx, vkarena *arena, VkBuffer buf)
{
  if(!ctx || !arena || !buf) return -1;

  VkMemoryRequirements reqs;
  vkGetBufferMemoryRequirements(ctx->dev, buf, &reqs);

  if(!(reqs.memoryTypeBits & (1U << arena->typeidx))) return -1;

  usize saved_offset= arena->offset;
  usize saved_slop  = arena->slop;

  i64 offset= arena_suballoc(arena, reqs.size, reqs.alignment);
  if(offset < 0) return -1;

  VkResult res= vkBindBufferMemory(ctx->dev, buf, arena->data, (usize)offset);
  if(res != 0)
  {
    arena->offset= saved_offset;
    arena->slop  = saved_slop;
    return -1;
  }

  return offset;
}

static void arenaclear(vkarena *arena)
{
  arena->offset= 0;
  arena->slop  = 0;
}
static void arenaterm(vkctx *ctx, vkarena *arena)
{
  if(!arena || !ctx) return;
  if(arena->data) vkFreeMemory(ctx->dev, arena->data, NULL);
  memset(arena, 0, sizeof *arena);
}

static VkBuffer updata_staged(
  vkctx *ctx, vkarena *host, vkarena *dev, const void *data, usize bytes)
{
  // function scope
  VkBuffer        staging= VKNULL, buf= VKNULL;
  VkCommandBuffer cmdbuf= VKNULL;
  VkFence         fence = VKNULL;
  VkResult        res;

  if(!ctx || !host || !dev || !data) goto err;

  if(!ctx->gfxcmdpool) goto err;

  if(!(host->flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) goto err;
  if(!(dev->flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) goto err;

  // create and bind device buffer
  {
    VkBufferCreateInfo info= {
      .sType= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bytes,
      .usage=
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
    OK(vkCreateBuffer(ctx->dev, &info, NULL, &buf), goto err;)

    i64 offset= memalloc(ctx, dev, buf);
    if(offset < 0) goto err;
  }
  // create and bind staging buffer, and copy data
  {
    VkBufferCreateInfo info= {
      .sType= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bytes,
      .usage= VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
    OK(vkCreateBuffer(ctx->dev, &info, NULL, &staging), goto err;)

    i64 offset= memalloc(ctx, host, staging);
    if(offset < 0) goto err;

    void *addr;
    OK(vkMapMemory(ctx->dev, host->data, (VkDeviceSize)offset, bytes, 0, &addr),
       goto err;)
    memcpy(addr, data, bytes);
    vkUnmapMemory(ctx->dev, host->data);
  }

  // create command buffer
  {
    VkCommandBufferAllocateInfo info= {
      .sType             = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool       = ctx->gfxcmdpool,
      .commandBufferCount= 1};
    OK(vkAllocateCommandBuffers(ctx->dev, &info, &cmdbuf), goto err;)
  }
  // record cmds
  {
    VkCommandBufferBeginInfo info= {
      .sType= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    OK(vkBeginCommandBuffer(cmdbuf, &info), goto err;)
    VkBufferCopy copy= {.dstOffset= 0, .srcOffset= 0, .size= bytes};
    vkCmdCopyBuffer(cmdbuf, staging, buf, 1, &copy);
    vkEndCommandBuffer(cmdbuf);
  }
  // create fence
  {
    VkFenceCreateInfo info= {.sType= VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    OK(vkCreateFence(ctx->dev, &info, NULL, &fence), goto err;)
  }
  // submit cmds
  {
    VkCommandBufferSubmitInfo cmdinfo= {
      .sType        = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .commandBuffer= cmdbuf};
    VkSubmitInfo2 info= {
      .sType                 = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .commandBufferInfoCount= 1,
      .pCommandBufferInfos   = &cmdinfo};
    OK(vkQueueSubmit2(ctx->gfxq, 1, &info, fence), goto err;)
  }

  OK(vkWaitForFences(ctx->dev, 1, &fence, VK_TRUE, UINT64_MAX), goto err;)

  if(staging) vkDestroyBuffer(ctx->dev, staging, NULL);
  if(cmdbuf) vkFreeCommandBuffers(ctx->dev, ctx->gfxcmdpool, 1, &cmdbuf);
  if(fence) vkDestroyFence(ctx->dev, fence, NULL);
  return buf;
err:
  if(staging) vkDestroyBuffer(ctx->dev, staging, NULL);
  if(cmdbuf) vkFreeCommandBuffers(ctx->dev, ctx->gfxcmdpool, 1, &cmdbuf);
  if(fence) vkDestroyFence(ctx->dev, fence, NULL);
  if(buf) vkDestroyBuffer(ctx->dev, buf, NULL);
  return VKNULL;
}
static VkBuffer updata_direct(vkctx *ctx, vkarena *rbar, const void *data, usize bytes)
{
  VkMemoryPropertyFlags flags=
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

  VkBuffer buf= VKNULL;
  VkResult res;

  if(!ctx || !data || ((rbar->flags & flags) != flags)) goto err;

  VkBufferCreateInfo info= {
    .sType      = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .usage      = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    .size       = bytes,
    .sharingMode= VK_SHARING_MODE_EXCLUSIVE};
  res= vkCreateBuffer(ctx->dev, &info, NULL, &buf);
  OK(res, goto err;)

  i64 offset= memalloc(ctx, rbar, buf);
  if(offset < 0) goto err;

  void *mapped;
  res= vkMapMemory(ctx->dev, rbar->data, (usize)offset, bytes, 0, &mapped);
  OK(res, goto err;)
  memcpy(mapped, data, bytes);
  vkUnmapMemory(ctx->dev, rbar->data);

  return buf;
err:
  if(buf) vkDestroyBuffer(ctx->dev, buf, NULL);
  return VKNULL;
}

int gfxprep(gfxctx *ctx, const u32 *spv, usize nrbytes)
{
  if(!ctx || !spv || !nrbytes) goto err;
  // upload
  {
    vertex triangle[3]= {
      {.pos= {.x= 0.0f, .y= -0.5f}, .color= {.r= 1.f, .g= 0.f, .b= 0.f}},
      {.pos= {.x= 0.5f, .y= 0.5f}, .color= {.r= 0.f, .g= 1.f, .b= 0.f}},
      {.pos= {.x= -0.5f, .y= 0.5f}, .color= {.r= 0.f, .g= 0.f, .b= 1.f}}};

    ctx->vtxbuf= updata(ctx, &ctx->dfltpool, triangle, sizeof triangle);
  }

  NNUL(ctx->vtxbuf, goto err;)
  OK(initgfxppl(ctx, spv, nrbytes), goto err;)
  OK(frminit(ctx), goto err;)

  return 0;
err:
  return -1;
}

static void printmeminfo(const vkctx *ctx)
{
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(ctx->gpu, &props);

  puts("=== memory heaps ===");
  for(u32 i= 0; i < props.memoryHeapCount; i++)
  {
    VkMemoryHeap h= props.memoryHeaps[i];
    printf(
      "  heap[%u]  size=%.0f MiB  flags=%s\n",
      i,
      (double)h.size / (1024.0 * 1024.0),
      h.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ? "DEVICE_LOCAL" : "(none)");
  }

  puts("=== memory types ===");
  for(u32 i= 0; i < props.memoryTypeCount; i++)
  {
    VkMemoryType          t= props.memoryTypes[i];
    VkMemoryPropertyFlags f= t.propertyFlags;
    printf("type[%u]  heap=%u  flags=", i, t.heapIndex);
    if(!f)
    {
      puts("(none)");
      continue;
    }
    if(f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) printf("DEVICE_LOCAL ");
    if(f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) printf("HOST_VISIBLE ");
    if(f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) printf("HOST_COHERENT ");
    if(f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) printf("HOST_CACHED ");
    if(f & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) printf("LAZILY_ALLOCATED ");
    if(f & VK_MEMORY_PROPERTY_PROTECTED_BIT) printf("PROTECTED ");
    putchar('\n');
  }
}
static void printarenainfo(const vkctx *ctx, const vkarena *arena, const char *name)
{
  VkPhysicalDeviceMemoryProperties props;
  vkGetPhysicalDeviceMemoryProperties(ctx->gpu, &props);

  VkMemoryType t      = props.memoryTypes[arena->typeidx];
  VkMemoryHeap heap   = props.memoryHeaps[t.heapIndex];
  double       heapmib= (double)heap.size / (1024.0 * 1024.0);
  double       sizemib= (double)arena->size / (1024.0 * 1024.0);

  printf("=== arena: %s ===\n", name ? name : "?");
  printf("  size     = %.2f MiB\n", sizemib);
  printf("  used     = %zu B  slop=%zu B\n", arena->offset, arena->slop);
  printf("  typeidx  = %u\n", arena->typeidx);
  printf(
    "  heap     = %u  (%.0f MiB%s)\n",
    t.heapIndex,
    heapmib,
    heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT ? ", DEVICE_LOCAL" : "");
  printf("  flags    =");
  VkMemoryPropertyFlags f= arena->flags;
  if(f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) printf(" DEVICE_LOCAL");
  if(f & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) printf(" HOST_VISIBLE");
  if(f & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) printf(" HOST_COHERENT");
  if(f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) printf(" HOST_CACHED");
  if(f & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) printf(" LAZILY_ALLOCATED");
  if(f & VK_MEMORY_PROPERTY_PROTECTED_BIT) printf(" PROTECTED");
  putchar('\n');
}
