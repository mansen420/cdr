#include "src/vulkan/vulkan.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan_core.h>

#include "include/display.h"
#include "include/graphics.h"
#include "include/mem.h"

static const char *const VK_LAYER_VALID_NAME= "VK_LAYER_KHRONOS_validation";

static int init_swp(vkctx *ctx, i32 w, i32 h)
{
  // tear down old views (no-op if VK_NULL_HANDLE)
  for(unsigned i= 0; i < ctx->nrswpimgs; i++)
    vkDestroyImageView(ctx->dev, ctx->swpimgvs[i], NULL);
  memset(&ctx->swpimgvs, 0, sizeof ctx->swpimgvs);

  // query surface
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

  // pick format
  const VkSurfaceFormatKHR *chosen= &ctx->gpusfmts[0];
  for(unsigned i= 0; i < ctx->nrgpusfmts; i++)
    if(
      ctx->gpusfmts[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
      ctx->gpusfmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      chosen= &ctx->gpusfmts[i];
      break;
    }

  // pick extent
  VkExtent2D extent;
  if(
    ctx->gpuscaps.currentExtent.width == UINT32_MAX &&
    ctx->gpuscaps.currentExtent.height == UINT32_MAX)
  {
    extent.width= (u32)w > ctx->gpuscaps.maxImageExtent.width
                    ? ctx->gpuscaps.maxImageExtent.width
                    : (u32)w;

    extent.height= (u32)h > ctx->gpuscaps.maxImageExtent.height
                     ? ctx->gpuscaps.maxImageExtent.height
                     : (u32)h;
  }
  else
    extent= ctx->gpuscaps.currentExtent;

  // pick present mode
  const VkPresentModeKHR *mode= NULL;
  for(unsigned i= 0; i < ctx->nrprsmodes; i++)
  {
    if(ctx->prsmodes[i] == VK_PRESENT_MODE_MAILBOX_KHR) mode= &ctx->prsmodes[i];
  }
  if(!mode) mode= &ctx->prsmodes[0];

  // create swapchain
  VkSwapchainCreateInfoKHR info= {
    .sType           = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface         = ctx->surf,
    .minImageCount   = ctx->gpuscaps.minImageCount + 1,  // XXX may break
    .imageFormat     = chosen->format,
    .imageColorSpace = chosen->colorSpace,
    .imageExtent     = extent,
    .imageArrayLayers= 1,
    .imageUsage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
    .imageSharingMode=
      ctx->gfxprs ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
    .queueFamilyIndexCount= ctx->gfxprs ? 0 : 2,
    .pQueueFamilyIndices  = ctx->gfxprs ? NULL : &ctx->gfxqfamidx,
    .preTransform         = ctx->gpuscaps.currentTransform,
    .compositeAlpha       = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode          = *mode,
    .clipped              = VK_TRUE,
    .oldSwapchain         = ctx->swp};

  VkSwapchainKHR newswp;
  res= vkCreateSwapchainKHR(ctx->dev, &info, NULL, &newswp);
  OK(res, goto err;)

  if(ctx->swp) vkDestroySwapchainKHR(ctx->dev, ctx->swp, NULL);
  ctx->swp      = newswp;
  ctx->swpsfmt  = *chosen;
  ctx->swpextent= extent;

  // image views
  res= vkGetSwapchainImagesKHR(ctx->dev, ctx->swp, &ctx->nrswpimgs, NULL);
  OK(res, goto err;)
  res=
    vkGetSwapchainImagesKHR(ctx->dev, ctx->swp, &ctx->nrswpimgs, ctx->swpimgs);
  OK(res, goto err;)

  for(unsigned i= 0; i < ctx->nrswpimgs; i++)
  {
    VkImageViewCreateInfo vinfo= {
      .sType           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image           = ctx->swpimgs[i],
      .viewType        = VK_IMAGE_VIEW_TYPE_2D,
      .format          = ctx->swpsfmt.format,
      .subresourceRange= {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    res= vkCreateImageView(ctx->dev, &vinfo, NULL, &ctx->swpimgvs[i]);
    OK(res, goto err;)
  }
  return 0;
err:
  return -1;
}

static int init(vkctx **out, void *dpy, gfxdesc desc)
{
  vkctx *ctx= cdralloc(NULL, sizeof *ctx);

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
  OK(init_swp(ctx, 800, 600), goto err;)
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

  // offline command buffer + fence
  {
    VkCommandBufferAllocateInfo ai= {
      .sType             = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool       = ctx->gfxcmdpool,
      .level             = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount= 1};
    OK(vkAllocateCommandBuffers(ctx->dev, &ai, &ctx->offlinecmdbuf), goto err;)

    VkFenceCreateInfo fi= {
      .sType= VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags= VK_FENCE_CREATE_SIGNALED_BIT};
    OK(vkCreateFence(ctx->dev, &fi, NULL, &ctx->offlinefence), goto err;)
  }

  // implicit memory
  if(!desc.flags & GFX_NO_IMPLICIT_MEM)
  {
    // TODO rather than default to 2MB, do lazy allozation and scale off the
    // first request
    if(!desc.devmem) desc.devmem= (usize)(2 * MB);

    VkMemoryPropertyFlags rbar_flags= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    OK(arenainit(
         &ctx->dfltpool.base,
         ctx,
         desc.devmem,
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
       goto err;)
  }

  OK(init_frame(ctx), goto err;)

  *out= ctx;
  return 0;
err:
  gfxterm(ctx);
  puts("VK init erred");
  return -1;
}

static void term_frame(vkctx *ctx, u32 idx)
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

  if(ctx->offlinefence) vkDestroyFence(ctx->dev, ctx->offlinefence, NULL);
  arenaterm(ctx, &ctx->dfltpool.base);
  for(unsigned i= 0; i < ctx->nrframes; i++) term_frame(ctx, i);
  if(ctx->prscmdpool) vkDestroyCommandPool(ctx->dev, ctx->prscmdpool, NULL);
  if(ctx->gfxcmdpool) vkDestroyCommandPool(ctx->dev, ctx->gfxcmdpool, NULL);
  for(unsigned i= 0; i < ctx->nrswpimgs; i++)
    if(ctx->swpimgvs[i]) vkDestroyImageView(ctx->dev, ctx->swpimgvs[i], NULL);
  if(ctx->swp) vkDestroySwapchainKHR(ctx->dev, ctx->swp, NULL);
  if(ctx->dev) vkDestroyDevice(ctx->dev, NULL);
  if(ctx->surf) vkDestroySurfaceKHR(ctx->inst, ctx->surf, NULL);
  if(ctx->inst) vkDestroyInstance(ctx->inst, NULL);

  memset(ctx, 0, sizeof *ctx);
  free(ctx);
}

static VkShaderStageFlags stage_to_vk(u32 bits)
{
  VkShaderStageFlags f= 0;
  if(bits & GFXSTAGE_VERT) f|= VK_SHADER_STAGE_VERTEX_BIT;
  if(bits & GFXSTAGE_FRAG) f|= VK_SHADER_STAGE_FRAGMENT_BIT;
  return f;
}

static int init_ppl(vkctx *ctx, gfxppl **out, gfxdesc_ppl desc)
{
  gfxppl        *ppl                     = NULL;
  VkShaderModule modules[GFX_MAX_SHADERS]= {0};
  NNUL(ctx, goto err;)
  NNUL(out, goto err;)
  NNUL(desc.nrshaders, goto err;)

  ppl= cdralloc(NULL, sizeof *ppl);
  NNUL(ppl, goto err;)

  VkPipelineShaderStageCreateInfo shaders[GFX_MAX_SHADERS];
  {
    for(usize i= 0; i < desc.nrshaders; i++)
    {
      VkShaderModuleCreateInfo info= {
        .sType   = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize= desc.program[i].nrbytes,
        .pCode   = desc.program[i].spv};
      OK(vkCreateShaderModule(ctx->dev, &info, NULL, &modules[i]), goto err;)

      shaders[i]= (VkPipelineShaderStageCreateInfo){
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stage_to_vk(desc.program[i].stagebits),
        .module= modules[i],
        .pName = desc.program[i].entry};
    }
  }

  // vertex input from bindings
  VkVertexInputBindingDescription   binds[GFX_MAX_BINDINGS];
  VkVertexInputAttributeDescription attrs[GFX_MAX_BINDINGS * GFX_MAX_ATTRS];
  u32                               nrattrs= 0;

  for(u32 b= 0; b < desc.nrbindings; b++)
  {
    gfxdesc_bufbind *bd= &desc.bindings[b];
    binds[b]           = (VkVertexInputBindingDescription){
      .binding  = bd->binding,
      .stride   = bd->stride,
      .inputRate= bd->rate == GFX_RATE_INSTANCE ? VK_VERTEX_INPUT_RATE_INSTANCE
                                                : VK_VERTEX_INPUT_RATE_VERTEX};
    for(u32 a= 0; a < bd->nrattrs; a++)
    {
      attrs[nrattrs]= (VkVertexInputAttributeDescription){
        .location= bd->attrs[a].location,
        .binding = bd->binding,
        .format  = attrfmt_to_vk(bd->attrs[a].fmt),
        .offset  = bd->attrs[a].offset};
      nrattrs++;
    }
  }

  VkPipelineVertexInputStateCreateInfo inputinfo= {
    .sType= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount  = desc.nrbindings,
    .pVertexBindingDescriptions     = binds,
    .vertexAttributeDescriptionCount= nrattrs,
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

  VkPushConstantRange vkranges[GFX_MAX_RANGES];
  u32                 nrranges= desc.nrranges;
  for(u32 i= 0; i < nrranges; i++)
  {
    vkranges[i]= (VkPushConstantRange){
      .stageFlags= stage_to_vk(desc.ranges[i].stages),
      .offset    = desc.ranges[i].offset,
      .size      = desc.ranges[i].bytes};
  }

  VkPipelineLayoutCreateInfo plinfo= {
    .sType                 = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pushConstantRangeCount= nrranges,
    .pPushConstantRanges   = vkranges};
  OK(vkCreatePipelineLayout(ctx->dev, &plinfo, NULL, &ppl->layout), goto err;)

  VkPipelineRenderingCreateInfo rinfo= {
    .sType                  = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
    .colorAttachmentCount   = 1,
    .pColorAttachmentFormats= &ctx->swpsfmt.format};

  VkGraphicsPipelineCreateInfo gfxinfo= {
    .sType              = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pNext              = &rinfo,
    .stageCount         = desc.nrshaders,
    .pStages            = shaders,
    .pVertexInputState  = &inputinfo,
    .pInputAssemblyState= &asminfo,
    .pViewportState     = &vpinfo,
    .pRasterizationState= &rstate,
    .pMultisampleState  = &msinfo,
    .pColorBlendState   = &blendinfo,
    .pDynamicState      = &dyninfo,
    .layout             = ppl->layout};
  OK(vkCreateGraphicsPipelines(
       ctx->dev, VK_NULL_HANDLE, 1, &gfxinfo, NULL, &ppl->handle),
     goto err;)

  for(usize i= 0; i < desc.nrshaders; i++)
    if(modules[i]) vkDestroyShaderModule(ctx->dev, modules[i], NULL);

  ppl->desc= desc;
  *out     = ppl;

  return 0;
err:
  if(ppl && ppl->layout) vkDestroyPipelineLayout(ctx->dev, ppl->layout, NULL);
  for(usize i= 0; i < desc.nrshaders; i++)
    if(modules[i]) vkDestroyShaderModule(ctx->dev, modules[i], NULL);
  free(ppl);
  return -1;
}

static void term_ppl(vkctx *ctx, gfxppl *ppl)
{
  if(!ctx || !ppl) return;
  if(ppl->handle) vkDestroyPipeline(ctx->dev, ppl->handle, NULL);
  if(ppl->layout) vkDestroyPipelineLayout(ctx->dev, ppl->layout, NULL);
  memset(ppl, 0, sizeof *ppl);
  free(ppl);
}

int gfxinit_ppl(gfxctx *c, gfxppl **o, gfxdesc_ppl d)
{
  return init_ppl(c, o, d);
}
void gfxterm_ppl(gfxctx *c, gfxppl *p) { term_ppl(c, p); }

int gfxinit(gfxctx **out, dpyctx *dpy, gfxdesc desc)
{
  return init(out, dpy, desc);
}
void    gfxterm(gfxctx *ctx) { term(ctx); }
void    gfxpause(gfxctx *ctx) { ctx->pause= 1; }
void    gfxresume(gfxctx *ctx) { ctx->pause= 0; }
gfximg *gfxget_display(gfxctx *ctx) { return ctx->swp ? DISPLAY_IMG : NULL; }
void    gfxupdate(gfxctx *ctx, i32 w, i32 h)
{
  vkDeviceWaitIdle(ctx->dev);
  reinit_swp(ctx, w, h);
}
void gfxsync(gfxctx *ctx) { vkDeviceWaitIdle(ctx->dev); }

static void term_buf(gfxctx *ctx, gfxbuf *buf)
{
  if(!ctx || !buf) return;
  if(buf->buf) vkDestroyBuffer(ctx->dev, buf->buf, NULL);
  free(buf);
}

static int upload(vkctx *ctx, gfxbuf *buf, vkarena *arena, gfxdesc_upload mem)
{
  int rc= -1;

  NNUL(ctx, goto exit;)
  NNUL(buf, goto exit;)
  NNUL(buf->buf, goto exit;)
  NNUL(arena, goto exit;)
  NNUL(mem.data, goto exit;)
  NNUL(mem.bytes, goto exit;)

  usize bufsz= (usize)buf->desc.nrelems * buf->desc.stride;
  IF(mem.offset + mem.bytes > bufsz, goto exit;)

  VkMemoryPropertyFlags rbar=
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
  if((arena->flags & rbar) == rbar)
  {
    OK(updata_direct(
         ctx, arena, buf->buf, buf->bindoff, mem.offset, mem.data, mem.bytes),
       goto exit;)
  }
  else
  {
    // the required size may be larger than the logical size
    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(ctx->dev, buf->buf, &reqs);
    vkarena host= {0};
    OK(arenainit(&host, ctx, reqs.size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT),
       goto exit;)
    OK(updata_staged(ctx, &host, buf->buf, mem.offset, mem.data, mem.bytes),
       goto exit;)
    arenaterm(ctx, &host);
  }

  rc= 0;
exit:
  return rc;
}

int gfxupload(gfxctx *c, gfxbuf *b, gfxmem *m, gfxdesc_upload d)
{
  if(!m) m= &c->dfltpool.base;
  return upload(c, b, m, d);
}

static int init_buf(
  gfxctx *ctx, gfxbuf **out, gfxmem *mem, gfxdesc_buf desc, gfxdesc_upload up)
{
  gfxbuf *buf= NULL;
  NNUL(ctx, goto err;)
  if(!mem)
  {
    NNUL(ctx->dfltpool.base.data, goto err;)
    mem= &ctx->dfltpool.base;
  }

  buf= cdralloc(NULL, sizeof *buf);
  NNUL(buf, goto err;)

  usize bytes= (usize)desc.nrelems * desc.stride;

  // always create + bind
  {
    VkBufferCreateInfo info= {
      .sType= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bytes,
      .usage=
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT};
    OK(vkCreateBuffer(ctx->dev, &info, NULL, &buf->buf), goto err;)
    i64 off= bufferalloc(ctx, mem, buf->buf);
    OK(off, goto err;)
    buf->bindoff= (usize)off;
  }

  buf->desc= desc;

  // optional initial upload
  if(up.data) OK(upload(ctx, buf, mem, up), goto err;)

  *out= buf;
  return 0;
err:
  term_buf(ctx, buf);
  return -1;
}

int gfxinit_buf(
  gfxctx *c, gfxbuf **o, gfxmem *m, gfxdesc_buf d, gfxdesc_upload up)
{
  return init_buf(c, o, m, d, up);
}
void gfxterm_buf(gfxctx *c, gfxbuf *b) { term_buf(c, b); }

static void term_img(vkctx *ctx, gfximg *img)
{
  if(!ctx || !img) return;
  if(img->view) vkDestroyImageView(ctx->dev, img->view, NULL);
  if(img->img) vkDestroyImage(ctx->dev, img->img, NULL);
  free(img);
}

static VkFormat imgfmt_to_vk(gfxfmt_img fmt)
{
  switch(fmt)
  {
    case GFXFMT_RGBA8_SRGB:
      return VK_FORMAT_R8G8B8A8_SRGB;
    case GFXFMT_RGBA8_UNORM:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case GFXFMT_BGRA8_SRGB:
      return VK_FORMAT_B8G8R8A8_SRGB;
    case GFXFMT_BGRA8_UNORM:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case GFXFMT_R8_UNORM:
      return VK_FORMAT_R8_UNORM;
    case GFXFMT_D32_SFLOAT:
      return VK_FORMAT_D32_SFLOAT;
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

static VkFormat attrfmt_to_vk(gfxfmt_attr fmt)
{
  switch(fmt)
  {
    case GFXFMT_2F32:
      return VK_FORMAT_R32G32_SFLOAT;
    case GFXFMT_3F32:
      return VK_FORMAT_R32G32B32_SFLOAT;
    case GFXFMT_4F32:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    case GFXFMT_2U8:
      return VK_FORMAT_R8G8_UNORM;
    case GFXFMT_3U8:
      return VK_FORMAT_R8G8B8_UNORM;
    case GFXFMT_4U8:
      return VK_FORMAT_R8G8B8A8_UNORM;
    default:
      return VK_FORMAT_UNDEFINED;
  }
}

static i64 imgalloc(vkctx *ctx, vkarena *arena, VkImage img)
{
  i64 rc= -1;

  NNUL(ctx, goto exit;)
  NNUL(arena, goto exit;)
  NNUL(img, goto exit;)

  VkMemoryRequirements reqs;
  vkGetImageMemoryRequirements(ctx->dev, img, &reqs);

  IF(!(reqs.memoryTypeBits & (1U << arena->typeidx)), goto exit;)

  usize saved_offset= arena->offset;
  usize saved_slop  = arena->slop;

  i64 offset= arena_suballoc(arena, reqs.size, reqs.alignment);
  OK(offset, goto exit;)

  VkResult res= vkBindImageMemory(ctx->dev, img, arena->data, (usize)offset);
  if(res != 0)
  {
    arena->offset= saved_offset;
    arena->slop  = saved_slop;
    OK(res, goto exit;)
  }

  rc= offset;
exit:
  return rc;
}
static int init_img(vkctx *ctx, gfximg **out, gfxmem_dev *mem, gfxdesc_img desc)
{
  gfximg *img= NULL;
  NNUL(ctx, goto err;)
  NNUL(out, goto err;)
  if(!mem) mem= &ctx->dfltpool;
  NNUL(mem->base.data, goto err;)

  img= cdralloc(NULL, sizeof *img);
  NNUL(img, goto err;)

  {
    VkImageCreateInfo info= {
      .sType    = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType= VK_IMAGE_TYPE_2D,
      .extent   = {desc.w, desc.h, 1},
      .tiling   = VK_IMAGE_TILING_OPTIMAL,
      .usage    = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      .format   = imgfmt_to_vk(desc.fmt),
      .mipLevels= 1,
      .arrayLayers= 1,
      .samples    = VK_SAMPLE_COUNT_1_BIT};
    OK(vkCreateImage(ctx->dev, &info, NULL, &img->img), goto err;)
  }

  OK(imgalloc(ctx, &mem->base, img->img), goto err;)

  // default view
  {
    VkImageViewCreateInfo vi= {
      .sType           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image           = img->img,
      .viewType        = VK_IMAGE_VIEW_TYPE_2D,
      .format          = imgfmt_to_vk(desc.fmt),
      .subresourceRange= {
        .aspectMask= desc.fmt == GFXFMT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                   : VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel  = 0,
        .levelCount    = 1,
        .baseArrayLayer= 0,
        .layerCount    = 1}};
    OK(vkCreateImageView(ctx->dev, &vi, NULL, &img->view), goto err;)
  }

  img->desc= desc;
  *out     = img;
  return 0;
err:
  term_img(ctx, img);
  return -1;
}

int gfxinit_img(gfxctx *c, gfximg **o, gfxmem_dev *m, gfxdesc_img d)
{
  return init_img(c, o, m, d);
}
void gfxterm_img(gfxctx *c, gfximg *i) { term_img(c, i); }

static int init_devmem(vkctx *ctx, gfxmem_dev **out, usize bytes)
{
  gfxmem_dev *mem= NULL;
  NNUL(out, goto err;)
  NNUL(ctx, goto err;)
  IF(!bytes, goto err;)

  mem= cdralloc(NULL, sizeof *mem);
  NNUL(mem, goto err;)

  OK(arenainit(&mem->base, ctx, bytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
     goto err;)

  *out= mem;
  return 0;
err:
  free(mem);
  return -1;
}

static int init_hostmem(vkctx *ctx, gfxmem_host **out, usize bytes)
{
  gfxmem_host *mem= NULL;
  NNUL(out, goto err;)
  NNUL(ctx, goto err;)
  IF(!bytes, goto err;)

  mem= cdralloc(NULL, sizeof *mem);
  NNUL(mem, goto err;)

  // try rBAR first (device-local + host-visible), fall back to plain RAM
  VkMemoryPropertyFlags rbar= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  VkMemoryPropertyFlags ram=
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  if(arenainit(&mem->base, ctx, bytes, rbar) < 0)
    OK(arenainit(&mem->base, ctx, bytes, ram), goto err;)

  OK(vkMapMemory(ctx->dev, mem->base.data, 0, bytes, 0, &mem->mapped),
     goto err;)

  *out= mem;
  return 0;
err:
  if(mem) arenaterm(ctx, &mem->base);
  free(mem);
  return -1;
}

static void term_mem(vkctx *ctx, gfxmem *mem)
{
  if(!ctx || !mem) return;
  if(mem->flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    vkUnmapMemory(ctx->dev, mem->data);
  arenaterm(ctx, mem);
  free(mem);
}

int gfxinit_devmem(gfxctx *c, gfxmem_dev **o, usize b)
{
  return init_devmem(c, o, b);
}
int gfxinit_hostmem(gfxctx *c, gfxmem_host **o, usize b)
{
  return init_hostmem(c, o, b);
}
void *gfxmmap(gfxctx *c, gfxmem_host *m)
{
  (void)c;
  NNUL(m, return NULL;)
  return m->mapped;
}
void gfxterm_mem(gfxctx *c, gfxmem *m) { term_mem(c, m); }

static int init_frame(vkctx *ctx)
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
static int reinit_swp(vkctx *ctx, i32 w_new, i32 h_new)
{
  if(!ctx) goto err;
  OK(init_swp(ctx, w_new, h_new), goto err;)
  return 0;
err:
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
  NNUL(arena, goto err;)

  usize aligned      = alignup(arena->offset, align);
  usize alignoverhead= aligned - arena->offset;
  // coulda just checked on aligned >= size, but whatever
  IF(alignoverhead >= arena->size - arena->offset, goto err;)

  // fragmentation occurs here, would be a good place to log
  arena->slop+= alignoverhead;

  usize free= arena->size - aligned;
  IF(bytes > free, goto err;)

  i64 res= (i64)aligned;
  arena->offset+= alignoverhead + bytes;

  return res;
err:
  return -1;
}
static i64 bufferalloc(vkctx *ctx, vkarena *arena, VkBuffer buf)
{
  usize saved_offset= arena->offset;
  usize saved_slop  = arena->slop;

  if(!ctx || !arena || !buf) return -1;
  NNUL(ctx, goto err;)
  NNUL(arena, goto err;)
  NNUL(buf, goto err;)

  VkMemoryRequirements reqs;
  vkGetBufferMemoryRequirements(ctx->dev, buf, &reqs);

  IF(!(reqs.memoryTypeBits & (1U << arena->typeidx)), goto err;)

  i64 offset= arena_suballoc(arena, reqs.size, reqs.alignment);
  IF(offset < 0, goto err;)

  OK(vkBindBufferMemory(ctx->dev, buf, arena->data, (usize)offset), goto err;)

  return offset;
err:
  arena->offset= saved_offset;
  arena->slop  = saved_slop;
  return -1;
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

static int updata_staged(
  vkctx      *ctx,
  vkarena    *host,
  VkBuffer    dst,
  usize       dstoff,
  const void *data,
  usize       bytes)
{
  VkBuffer staging= VKNULL;
  VkResult res;

  NNUL(ctx, goto err;)
  NNUL(host, goto err;)
  NNUL(dst, goto err;)
  NNUL(data, goto err;)
  NNUL(ctx->offlinecmdbuf, goto err;)
  NNUL(ctx->offlinefence, goto err;)
  IF(!(host->flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), goto err;)

  // create and bind staging buffer, copy data in
  {
    VkBufferCreateInfo info= {
      .sType= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bytes,
      .usage= VK_BUFFER_USAGE_TRANSFER_SRC_BIT};
    OK(vkCreateBuffer(ctx->dev, &info, NULL, &staging), goto err;)

    i64 offset= bufferalloc(ctx, host, staging);
    IF(offset < 0, goto err;)

    void *addr;
    OK(vkMapMemory(ctx->dev, host->data, (VkDeviceSize)offset, bytes, 0, &addr),
       goto err;)
    memcpy(addr, data, bytes);
    vkUnmapMemory(ctx->dev, host->data);
  }

  // record copy
  OK(vkWaitForFences(ctx->dev, 1, &ctx->offlinefence, VK_TRUE, UINT64_MAX),
     goto err;)
  OK(vkResetFences(ctx->dev, 1, &ctx->offlinefence), goto err;)
  OK(vkResetCommandBuffer(ctx->offlinecmdbuf, 0), goto err;)
  {
    VkCommandBufferBeginInfo info= {
      .sType= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    OK(vkBeginCommandBuffer(ctx->offlinecmdbuf, &info), goto err;)
    VkBufferCopy copy= {.dstOffset= dstoff, .srcOffset= 0, .size= bytes};
    vkCmdCopyBuffer(ctx->offlinecmdbuf, staging, dst, 1, &copy);
    OK(vkEndCommandBuffer(ctx->offlinecmdbuf), goto err;)
  }

  // submit and wait
  {
    VkCommandBufferSubmitInfo cmdinfo= {
      .sType        = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
      .commandBuffer= ctx->offlinecmdbuf};
    VkSubmitInfo2 info= {
      .sType                 = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
      .commandBufferInfoCount= 1,
      .pCommandBufferInfos   = &cmdinfo};
    OK(vkQueueSubmit2(ctx->gfxq, 1, &info, ctx->offlinefence), goto err;)
  }
  OK(vkWaitForFences(ctx->dev, 1, &ctx->offlinefence, VK_TRUE, UINT64_MAX),
     goto err;)

  if(staging) vkDestroyBuffer(ctx->dev, staging, NULL);
  return 0;
err:
  if(staging) vkDestroyBuffer(ctx->dev, staging, NULL);
  return -1;
}
static int updata_direct(
  vkctx      *ctx,
  vkarena    *mem,
  VkBuffer    dst,
  usize       bindoff,
  usize       dstoff,
  const void *data,
  usize       bytes)
{
  VkResult res;
  void    *mapped= NULL;

  NNUL(ctx, goto err;)
  NNUL(ctx->dev, goto err;)
  NNUL(data, goto err;)
  NNUL(dst, goto err;)
  IF(!(mem->flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT), goto err;)

  OK(vkMapMemory(ctx->dev, mem->data, bindoff + dstoff, bytes, 0, &mapped),
     goto err;)
  memcpy(mapped, data, bytes);
  vkUnmapMemory(ctx->dev, mem->data);
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
static void printarenainfo(
  const vkctx *ctx, const vkarena *arena, const char *name)
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

// -- pass / commands ----------------------------------------------------------

static int init_pass(vkctx *ctx, gfxpass **out, gfxdesc_pass desc)
{
  (void)ctx;
  gfxpass *p= NULL;

  if(!out) goto err;

  p= cdralloc(NULL, sizeof *p);
  if(!p) goto err;

  p->desc= desc;
  *out   = p;
  return 0;
err:
  free(p);
  return -1;
}

static void term_pass(vkctx *ctx, gfxpass *p)
{
  (void)ctx;
  if(!p) return;
  memset(p, 0, sizeof *p);
  free(p);
}

// TODO command recording refactor:
//  - gfxcmd_draw takes array of gfxdesc_drawcmd
//  - cmd_end() records Vulkan commands (sort by pipeline, batch adjacent draws)
//  - submit() only re-records for display passes (swapchain index changes);
//    offline passes reuse the recorded command buffer if not dirty
static void cmd_begin(gfxpass *p)
{
  p->nrdraws  = 0;
  p->recording= 1;
}

static void cmd_draw(gfxpass *p, gfxdesc_drawcmd cmd)
{
  if(!p->recording) return;
  if(p->nrdraws >= GFX_MAX_DRAWS) return;
  p->draws[p->nrdraws++]= cmd;
}

static void cmd_end(gfxpass *p) { p->recording= 0; }

static int submit(vkctx *ctx, gfxpass *p)
{
  NNUL(ctx && p, goto err;)
  IF(!p->desc.nroutputs, goto err;)

  // use first output image to determine render area
  gfximg *out0= p->desc.outputs[0].img;
  NNUL(out0, goto err;)
  VkExtent2D extent= {out0->desc.w, out0->desc.h};

  OK(vkWaitForFences(ctx->dev, 1, &ctx->offlinefence, VK_TRUE, UINT64_MAX),
     goto err;)
  OK(vkResetFences(ctx->dev, 1, &ctx->offlinefence), goto err;)
  OK(vkResetCommandBuffer(ctx->offlinecmdbuf, 0), goto err;)

  // record commands
  {
    VkCommandBufferBeginInfo binfo= {
      .sType= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    OK(vkBeginCommandBuffer(ctx->offlinecmdbuf, &binfo), goto err;)

    VkImageMemoryBarrier2 renderbarriers[GFX_MAX_PASS_IMGS];
    for(usize i= 0; i < p->desc.nroutputs; i++)
      renderbarriers[i]= (VkImageMemoryBarrier2){
        .sType           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask    = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        .dstStageMask    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask   = VK_ACCESS_2_NONE,
        .dstAccessMask   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image           = p->desc.outputs[i].img->img,
        .subresourceRange= {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

    VkDependencyInfo renderd= {
      .sType                  = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount= p->desc.nroutputs,
      .pImageMemoryBarriers   = renderbarriers};
    vkCmdPipelineBarrier2(ctx->offlinecmdbuf, &renderd);

    VkRenderingAttachmentInfo attinfo[GFX_MAX_PASS_IMGS];
    for(usize i= 0; i < p->desc.nroutputs; i++)
      attinfo[i]= (VkRenderingAttachmentInfo){
        .sType      = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView  = p->desc.outputs[i].img->view,
        .imageLayout= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp    = p->desc.outputs[i].clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                              : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp   = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue= {
          .color.float32= {
            p->desc.outputs[i].clearval.r,
            p->desc.outputs[i].clearval.g,
            p->desc.outputs[i].clearval.b,
            p->desc.outputs[i].clearval.a,
          }}};

    // TODO handle depth attachments
    VkRenderingInfo renderinfo= {
      .sType               = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea          = {{0, 0}, extent},
      .colorAttachmentCount= p->desc.nroutputs,
      .pColorAttachments   = attinfo,
      .layerCount          = 1};

    vkCmdBeginRendering(ctx->offlinecmdbuf, &renderinfo);

    VkViewport vp= {0, 0, (f32)extent.width, (f32)extent.height, 0.f, 1.f};
    vkCmdSetViewport(ctx->offlinecmdbuf, 0, 1, &vp);
    VkRect2D scissor= {.extent= extent};
    vkCmdSetScissor(ctx->offlinecmdbuf, 0, 1, &scissor);

    for(usize i= 0; i < p->nrdraws; i++)
    {
      gfxdesc_drawcmd *dr= p->draws + i;

      vkCmdBindPipeline(
        ctx->offlinecmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, dr->ppl->handle);

      if(dr->push.mem.data && dr->push.mem.bytes)
      {
        vkCmdPushConstants(
          ctx->offlinecmdbuf,
          dr->ppl->layout,
          stage_to_vk(dr->push.stages),
          dr->push.mem.offset,
          dr->push.mem.bytes,
          dr->push.mem.data);
      }

      for(u32 b= 0; b < dr->nrbufs; b++)
      {
        VkDeviceSize off= 0;
        vkCmdBindVertexBuffers(
          ctx->offlinecmdbuf,
          dr->bufs[b].binding,
          1,
          &dr->bufs[b].buf->buf,
          &off);
      }

      u32 inst= dr->nrinst ? dr->nrinst : 1;
      if(dr->idx)
      {
        vkCmdBindIndexBuffer(
          ctx->offlinecmdbuf, dr->idx->buf, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(
          ctx->offlinecmdbuf,
          dr->nridx,
          inst,
          dr->firstidx,
          dr->firstvtx,
          dr->firstinst);
      }
      else
        vkCmdDraw(
          ctx->offlinecmdbuf, dr->nrvtx, inst, dr->firstvtx, dr->firstinst);
    }
    vkCmdEndRendering(ctx->offlinecmdbuf);
    OK(vkEndCommandBuffer(ctx->offlinecmdbuf), goto err;)
  }

  VkCommandBufferSubmitInfo cmdinfo= {
    .sType        = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .commandBuffer= ctx->offlinecmdbuf};
  VkSubmitInfo2 subinfo= {
    .sType                 = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .commandBufferInfoCount= 1,
    .pCommandBufferInfos   = &cmdinfo};
  OK(vkQueueSubmit2(ctx->gfxq, 1, &subinfo, ctx->offlinefence), goto err;)

  return 0;
err:
  return -1;
}
static int present(vkctx *ctx, gfximg *src)
{
  NNUL(ctx, goto err;)
  NNUL(src, goto err;)

  frmctx *frm= &ctx->frames[ctx->frmidx];
  NNUL(frm, goto err;)

  OK(vkWaitForFences(ctx->dev, 1, &frm->fence, VK_TRUE, UINT64_MAX), goto err;)

  u32 imgidx;
  OK(vkAcquireNextImageKHR(
       ctx->dev, ctx->swp, UINT64_MAX, frm->imggotten, VKNULL, &imgidx),
     goto err;)

  // cmd buffer
  {
    vkResetCommandBuffer(frm->cmdbuf, 0);
    VkCommandBufferBeginInfo binfo= {
      .sType= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    OK(vkBeginCommandBuffer(frm->cmdbuf, &binfo), goto err;)

    VkImageMemoryBarrier2 barriers[2]= {0};
    barriers[0]                      = (VkImageMemoryBarrier2){
      .sType           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .oldLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .newLayout       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      .srcStageMask    = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstStageMask    = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .srcAccessMask   = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
      .dstAccessMask   = VK_ACCESS_2_TRANSFER_READ_BIT,
      .image           = src->img,
      .subresourceRange= (VkImageSubresourceRange){
        .aspectMask    = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseArrayLayer= 0,
        .layerCount    = 1,
        .baseMipLevel  = 0,
        .levelCount    = 1}};
    barriers[1]= (VkImageMemoryBarrier2){
      .sType           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .oldLayout       = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcStageMask    = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
      .dstStageMask    = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .srcAccessMask   = VK_ACCESS_2_NONE,
      .dstAccessMask   = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .image           = ctx->swpimgs[imgidx],
      .subresourceRange= (VkImageSubresourceRange){
        .aspectMask    = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseArrayLayer= 0,
        .layerCount    = 1,
        .baseMipLevel  = 0,
        .levelCount    = 1}};
    VkDependencyInfo dinfo= {
      .sType                  = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount= 2,
      .pImageMemoryBarriers   = barriers};
    vkCmdPipelineBarrier2(frm->cmdbuf, &dinfo);

    VkImageBlit blitregion= {
      .srcSubresource=
        (VkImageSubresourceLayers){
          .aspectMask    = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel      = 0,
          .baseArrayLayer= 0,
          .layerCount    = 1},
      .srcOffsets= {{0, 0, 0}, {(i32)src->desc.w, (i32)src->desc.h, 1}},
      .dstSubresource=
        (VkImageSubresourceLayers){
          .aspectMask    = VK_IMAGE_ASPECT_COLOR_BIT,
          .mipLevel      = 0,
          .baseArrayLayer= 0,
          .layerCount    = 1},
      .dstOffsets= {
        {0, 0, 0}, {(i32)ctx->swpextent.width, (i32)ctx->swpextent.height, 1}}};
    vkCmdBlitImage(
      frm->cmdbuf,
      src->img,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      ctx->swpimgs[imgidx],
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &blitregion,
      VK_FILTER_NEAREST);

    VkImageMemoryBarrier2 prsbarrier= {
      .sType           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .oldLayout       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .newLayout       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .srcStageMask    = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .dstStageMask    = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
      .srcAccessMask   = VK_ACCESS_2_TRANSFER_WRITE_BIT,
      .dstAccessMask   = VK_ACCESS_2_NONE,
      .image           = ctx->swpimgs[imgidx],
      .subresourceRange= (VkImageSubresourceRange){
        .aspectMask    = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseArrayLayer= 0,
        .layerCount    = 1,
        .baseMipLevel  = 0,
        .levelCount    = 1}};
    dinfo= (VkDependencyInfo){
      .sType                  = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .imageMemoryBarrierCount= 1,
      .pImageMemoryBarriers   = &prsbarrier};
    vkCmdPipelineBarrier2(frm->cmdbuf, &dinfo);

    OK(vkEndCommandBuffer(frm->cmdbuf), goto err;)
  }

  VkSemaphoreSubmitInfo waitinfo= {
    .sType    = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .stageMask= VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    .semaphore= frm->imggotten};
  VkSemaphore           subsem = ctx->frames[imgidx].renderfin;
  VkSemaphoreSubmitInfo siginfo= {
    .sType    = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .stageMask= VK_PIPELINE_STAGE_2_TRANSFER_BIT,
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
  OK(vkResetFences(ctx->dev, 1, &frm->fence), goto err;)
  OK(vkQueueSubmit2(ctx->gfxq, 1, &subinfo, frm->fence), goto err;)

  {
    VkPresentInfoKHR info= {
      .sType             = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount= 1,
      .pWaitSemaphores   = &subsem,
      .swapchainCount    = 1,
      .pSwapchains       = &ctx->swp,
      .pImageIndices     = &imgidx};
    OK(vkQueuePresentKHR(ctx->prsq, &info), goto err;)
  }
  ctx->frmidx= (ctx->frmidx + 1) % ctx->nrframes;

  return 0;
err:
  return -1;
}

int gfxpresent(gfxctx *c, gfximg *i) { return present(c, i); }

int gfxinit_pass(gfxctx *c, gfxpass **o, gfxdesc_pass d)
{
  return init_pass(c, o, d);
}
void gfxterm_pass(gfxctx *c, gfxpass *p) { term_pass(c, p); }
void gfxcmd_begin(gfxpass *p) { cmd_begin(p); }
void gfxcmd_draw(gfxpass *p, gfxdesc_drawcmd d) { cmd_draw(p, d); }
void gfxcmd_end(gfxpass *p) { cmd_end(p); }
int  gfxsubmit(gfxctx *c, gfxpass *p) { return submit(c, p); }
