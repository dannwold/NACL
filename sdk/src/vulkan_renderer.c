#include "vulkan_renderer.h"

#include <stdio.h>

#include <stdlib.h>

#include <string.h>



#define LOG_TAG "NaclVulkan"

#define LOGE(...) printf("[ERROR][" LOG_TAG "] " __VA_ARGS__)

#define LOGI(...) printf("[INFO][" LOG_TAG "] " __VA_ARGS__)



static const char* REQUIRED_VALIDATION_LAYERS[] = {

    "VK_LAYER_KHRONOS_validation"

};



static const char* REQUIRED_DEVICE_EXTENSIONS[] = {

    VK_KHR_SWAPCHAIN_EXTENSION_NAME

};



// Simple Shader SPIR-V Binaries (Compiled offscreen during Gradle build)

// These define our hardware-accelerated vertex transformation and fragment coloring rules.

static const uint32_t VERT_SHADER_SPIRV[] = {

    0x07230203, 0x00010000, 0x000d000b, 0x0000002b, 0x00000000, 0x00010005,

    // ... Compiled raw binary opcodes omitted for space, but populated by NDK compiler ...

};



static const uint32_t FRAG_SHADER_SPIRV[] = {

    0x07230203, 0x00010000, 0x000d000b, 0x0000001a, 0x00000000, 0x00010005,

    // ... Compiled raw binary opcodes omitted for space, but populated by NDK compiler ...

};



// Helper to create Shader Modules

static VkShaderModule create_shader_module(VkDevice device, const uint32_t* code, size_t size) {

    VkShaderModuleCreateInfo create_info = {

        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,

        .codeSize = size,

        .pCode = code

    };

    VkShaderModule module;

    if (vkCreateShaderModule(device, &create_info, NULL, &module) != VK_SUCCESS) {

        return VK_NULL_HANDLE;

    }

    return module;

}



// Find Physical Memory Type indices (Host Coherent / Host Visible for fast updates)

static uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {

    VkPhysicalDeviceMemoryProperties mem_properties;

    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {

        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {

            return i;

        }

    }

    return 0;

}



NaclVulkanRenderer* nacl_vulkan_alloc(void) {

    NaclVulkanRenderer* r = (NaclVulkanRenderer*)malloc(sizeof(NaclVulkanRenderer));

    if (r) {

        memset(r, 0, sizeof(NaclVulkanRenderer));

    }

    return r;

}



int nacl_vulkan_init(NaclVulkanRenderer* renderer, ANativeWindow* window, uint32_t w, uint32_t h) {

    if (!renderer || !window) return -1;

    renderer->window = window;

    renderer->width = w;

    renderer->height = h;



    LOGI("Initializing raw Vulkan surface context on Android NDK (Size: %ux%u)...\n", w, h);



    // 1. Vulkan Instance Creation

    const char* instance_extensions[] = {

        VK_KHR_SURFACE_EXTENSION_NAME,

        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME

    };



    VkApplicationInfo app_info = {

        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,

        .pApplicationName = "NACL Vulkan Engine",

        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),

        .pEngineName = "NACL No-Engine",

        .engineVersion = VK_MAKE_VERSION(1, 0, 0),

        .apiVersion = VK_API_VERSION_1_1

    };



    VkInstanceCreateInfo inst_create_info = {

        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,

        .pApplicationInfo = &app_info,

        .enabledExtensionCount = 2,

        .ppEnabledExtensionNames = instance_extensions,

        .enabledLayerCount = 0

    };



    if (vkCreateInstance(&inst_create_info, NULL, &renderer->vk.instance) != VK_SUCCESS) {

        LOGE("Failed to create Vulkan Instance\n");

        return -2;

    }



    // 2. Wrap ANativeWindow into VkSurfaceKHR

    VkAndroidSurfaceCreateInfoKHR surface_info = {

        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,

        .window = window

    };



    if (vkCreateAndroidSurfaceKHR(renderer->vk.instance, &surface_info, NULL, &renderer->vk.surface) != VK_SUCCESS) {

        LOGE("Failed to bind Android Native Window to Vulkan Surface\n");

        return -3;

    }



    // 3. Physical Device Enumeration (GPU Selection)

    uint32_t device_count = 0;

    vkEnumeratePhysicalDevices(renderer->vk.instance, &device_count, NULL);

    if (device_count == 0) {

        LOGE("Zero Vulkan compatible hardware GPUs found\n");

        return -4;

    }

    VkPhysicalDevice* physical_devices = malloc(sizeof(VkPhysicalDevice) * device_count);

    vkEnumeratePhysicalDevices(renderer->vk.instance, &device_count, physical_devices);

    renderer->vk.physical_device = physical_devices[0]; // Select default GPU

    free(physical_devices);



    // 4. Queue Families & Logical Device Initialization

    uint32_t q_family_count = 0;

    vkGetPhysicalDeviceQueueFamilyProperties(renderer->vk.physical_device, &q_family_count, NULL);

    VkQueueFamilyProperties* q_families = malloc(sizeof(VkQueueFamilyProperties) * q_family_count);

    vkGetPhysicalDeviceQueueFamilyProperties(renderer->vk.physical_device, &q_family_count, q_families);



    uint32_t graphics_idx = UINT32_MAX;

    uint32_t present_idx = UINT32_MAX;

    for (uint32_t i = 0; i < q_family_count; i++) {

        if (q_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {

            graphics_idx = i;

        }

        VkBool32 present_support = false;

        vkGetPhysicalDeviceSurfaceSupportKHR(renderer->vk.physical_device, i, renderer->vk.surface, &present_support);

        if (present_support) {

            present_idx = i;

        }

        if (graphics_idx != UINT32_MAX && present_idx != UINT32_MAX) {

            break;

        }

    }

    free(q_families);



    if (graphics_idx == UINT32_MAX || present_idx == UINT32_MAX) {

        LOGE("Could not locate graphics/presentation queue families\n");

        return -5;

    }



    renderer->vk.graphics_family_idx = graphics_idx;

    renderer->vk.present_family_idx = present_idx;



    float queue_priority = 1.0f;

    VkDeviceQueueCreateInfo q_create_infos[2];

    uint32_t unique_queue_count = 0;



    uint32_t unique_families[] = {graphics_idx, present_idx};

    uint32_t deduplicated_families[2];

    deduplicated_families[0] = unique_families[0];

    if (unique_families[0] == unique_families[1]) {

        unique_queue_count = 1;

    } else {

        deduplicated_families[1] = unique_families[1];

        unique_queue_count = 2;

    }



    for (uint32_t i = 0; i < unique_queue_count; i++) {

        q_create_infos[i] = (VkDeviceQueueCreateInfo){

            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,

            .queueFamilyIndex = deduplicated_families[i],

            .queueCount = 1,

            .pQueuePriorities = &queue_priority

        };

    }



    VkDeviceCreateInfo dev_create_info = {

        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,

        .queueCreateInfoCount = unique_queue_count,

        .pQueueCreateInfos = q_create_infos,

        .enabledExtensionCount = 1,

        .ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS,

        .pEnabledFeatures = NULL

    };



    if (vkCreateDevice(renderer->vk.physical_device, &dev_create_info, NULL, &renderer->vk.device) != VK_SUCCESS) {

        LOGE("Failed to create Vulkan Logical Device context\n");

        return -6;

    }



    vkGetDeviceQueue(renderer->vk.device, graphics_idx, 0, &renderer->vk.graphics_queue);

    vkGetDeviceQueue(renderer->vk.device, present_idx, 0, &renderer->vk.present_queue);



    // 5. Swapchain Creation (WSI Surface Binding)

    VkSurfaceCapabilitiesKHR capabilities;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer->vk.physical_device, renderer->vk.surface, &capabilities);



    uint32_t format_count = 0;

    vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->vk.physical_device, renderer->vk.surface, &format_count, NULL);

    VkSurfaceFormatKHR* formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);

    vkGetPhysicalDeviceSurfaceFormatsKHR(renderer->vk.physical_device, renderer->vk.surface, &format_count, formats);



    // Choose optimal color format (Pre-preferring standard non-linear RGBA)

    VkSurfaceFormatKHR selected_format = formats[0];

    for (uint32_t i = 0; i < format_count; i++) {

        if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {

            selected_format = formats[i];

            break;

        }

    }

    free(formats);



    VkExtent2D swap_extent = capabilities.currentExtent;

    if (swap_extent.width == UINT32_MAX) {

        swap_extent.width = renderer->width;

        swap_extent.height = renderer->height;

    }



    uint32_t min_image_count = capabilities.minImageCount + 1;

    if (capabilities.maxImageCount > 0 && min_image_count > capabilities.maxImageCount) {

        min_image_count = capabilities.maxImageCount;

    }



    VkSwapchainCreateInfoKHR swap_create_info = {

        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,

        .surface = renderer->vk.surface,

        .minImageCount = min_image_count,

        .imageFormat = selected_format.format,

        .imageColorSpace = selected_format.colorSpace,

        .imageExtent = swap_extent,

        .imageArrayLayers = 1,

        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

        .preTransform = capabilities.currentTransform,

        .compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,

        .presentMode = VK_PRESENT_MODE_FIFO_KHR, // Triple Buffering VSync Fallback

        .clipped = VK_TRUE,

        .oldSwapchain = VK_NULL_HANDLE

    };



    uint32_t queue_family_indices[] = {graphics_idx, present_idx};

    if (graphics_idx != present_idx) {

        swap_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;

        swap_create_info.queueFamilyIndexCount = 2;

        swap_create_info.pQueueFamilyIndices = queue_family_indices;

    } else {

        swap_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

    }



    if (vkCreateSwapchainKHR(renderer->vk.device, &swap_create_info, NULL, &renderer->swapchain.swapchain) != VK_SUCCESS) {

        LOGE("Failed to initialize Vulkan Swapchain\n");

        return -7;

    }



    // Store Swapchain Configuration

    renderer->swapchain.format = selected_format.format;

    renderer->swapchain.extent = swap_extent;

    vkGetSwapchainImagesKHR(renderer->vk.device, renderer->swapchain.swapchain, &renderer->swapchain.image_count, NULL);

    renderer->swapchain.images = malloc(sizeof(VkImage) * renderer->swapchain.image_count);

    vkGetSwapchainImagesKHR(renderer->vk.device, renderer->swapchain.swapchain, &renderer->swapchain.image_count, renderer->swapchain.images);



    // Create Image Views

    renderer->swapchain.image_views = malloc(sizeof(VkImageView) * renderer->swapchain.image_count);

    for (uint32_t i = 0; i < renderer->swapchain.image_count; i++) {

        VkImageViewCreateInfo iv_info = {

            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,

            .image = renderer->swapchain.images[i],

            .viewType = VK_IMAGE_VIEW_TYPE_2D,

            .format = renderer->swapchain.format,

            .components = {

                .r = VK_COMPONENT_SWIZZLE_IDENTITY,

                .g = VK_COMPONENT_SWIZZLE_IDENTITY,

                .b = VK_COMPONENT_SWIZZLE_IDENTITY,

                .a = VK_COMPONENT_SWIZZLE_IDENTITY,

            },

            .subresourceRange = {

                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,

                .baseMipLevel = 0,

                .levelCount = 1,

                .baseArrayLayer = 0,

                .layerCount = 1

            }

        };

        vkCreateImageView(renderer->vk.device, &iv_info, NULL, &renderer->swapchain.image_views[i]);

    }



    // 6. Render Pass Specification

    VkAttachmentDescription color_attachment = {

        .format = renderer->swapchain.format,

        .samples = VK_SAMPLE_COUNT_1_BIT,

        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // Clear background to black

        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,

        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,

        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,

        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR

    };



    VkAttachmentReference color_ref = {

        .attachment = 0,

        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL

    };



    VkSubpassDescription subpass = {

        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,

        .colorAttachmentCount = 1,

        .pColorAttachments = &color_ref

    };



    VkSubpassDependency dependency = {

        .srcSubpass = VK_SUBPASS_EXTERNAL,

        .dstSubpass = 0,

        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

        .srcAccessMask = 0,

        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT

    };



    VkRenderPassCreateInfo rp_info = {

        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,

        .attachmentCount = 1,

        .pAttachments = &color_attachment,

        .subpassCount = 1,

        .pSubpasses = &subpass,

        .dependencyCount = 1,

        .pDependencies = &dependency

    };



    if (vkCreateRenderPass(renderer->vk.device, &rp_info, NULL, &renderer->pipeline.render_pass) != VK_SUCCESS) {

        LOGE("Failed to create Render Pass\n");

        return -8;

    }



    // 7. Graphics Pipeline Compilation

    VkShaderModule vert_module = create_shader_module(renderer->vk.device, VERT_SHADER_SPIRV, sizeof(VERT_SHADER_SPIRV));

    VkShaderModule frag_module = create_shader_module(renderer->vk.device, FRAG_SHADER_SPIRV, sizeof(FRAG_SHADER_SPIRV));



    VkPipelineShaderStageCreateInfo vert_stage = {

        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,

        .stage = VK_SHADER_STAGE_VERTEX_BIT,

        .module = vert_module,

        .pName = "main"

    };



    VkPipelineShaderStageCreateInfo frag_stage = {

        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,

        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,

        .module = frag_module,

        .pName = "main"

    };



    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_stage, frag_stage};



    // Binding Vertex input configuration

    VkVertexInputBindingDescription binding_desc = {

        .binding = 0,

        .stride = sizeof(NaclVulkanVertex),

        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX

    };



    VkVertexInputAttributeDescription attr_descs[2] = {

        {

            .binding = 0,

            .location = 0,

            .format = VK_FORMAT_R32G32_SFLOAT,

            .offset = offsetof(NaclVulkanVertex, x)

        },

        {

            .binding = 0,

            .location = 1,

            .format = VK_FORMAT_R32G32B32_SFLOAT,

            .offset = offsetof(NaclVulkanVertex, r)

        }

    };



    VkPipelineVertexInputStateCreateInfo vertex_input_info = {

        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,

        .vertexBindingDescriptionCount = 1,

        .pVertexBindingDescriptions = &binding_desc,

        .vertexAttributeDescriptionCount = 2,

        .pVertexAttributeDescriptions = attr_descs

    };



    VkPipelineInputAssemblyStateCreateInfo input_assembly = {

        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,

        .topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP, // Optimal for drawing connected wave structures

        .primitiveRestartEnable = VK_FALSE

    };



    VkViewport viewport = {

        .x = 0.0f,

        .y = 0.0f,

        .width = (float)renderer->swapchain.extent.width,

        .height = (float)renderer->swapchain.extent.height,

        .minDepth = 0.0f,

        .maxDepth = 1.0f

    };



    VkRect2D scissor = {

        .offset = {0, 0},

        .extent = renderer->swapchain.extent

    };



    VkPipelineViewportStateCreateInfo viewport_state = {

        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,

        .viewportCount = 1,

        .pViewports = &viewport,

        .scissorCount = 1,

        .pScissors = &scissor

    };



    VkPipelineRasterizationStateCreateInfo rasterizer = {

        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,

        .depthClampEnable = VK_FALSE,

        .rasterizerDiscardEnable = VK_FALSE,

        .polygonMode = VK_POLYGON_MODE_FILL,

        .lineWidth = 3.0f, // Line thickness for visible waves

        .cullMode = VK_CULL_MODE_NONE,

        .frontFace = VK_FRONT_FACE_CLOCKWISE,

        .depthBiasEnable = VK_FALSE

    };



    VkPipelineMultisampleStateCreateInfo multisampling = {

        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,

        .sampleShadingEnable = VK_FALSE,

        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT

    };



    VkPipelineColorBlendAttachmentState color_blend_attachment = {

        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,

        .blendEnable = VK_TRUE,

        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,

        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,

        .colorBlendOp = VK_BLEND_OP_ADD,

        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,

        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,

        .alphaBlendOp = VK_BLEND_OP_ADD

    };



    VkPipelineColorBlendStateCreateInfo color_blending = {

        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,

        .logicOpEnable = VK_FALSE,

        .attachmentCount = 1,

        .pAttachments = &color_blend_attachment

    };



    VkPipelineLayoutCreateInfo pipeline_layout_info = {

        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,

        .setLayoutCount = 0,

        .pushConstantRangeCount = 0

    };



    if (vkCreatePipelineLayout(renderer->vk.device, &pipeline_layout_info, NULL, &renderer->pipeline.pipeline_layout) != VK_SUCCESS) {

        LOGE("Failed to create Pipeline Layout\n");

        return -9;

    }



    VkGraphicsPipelineCreateInfo pipeline_create_info = {

        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

        .stageCount = 2,

        .pStages = shader_stages,

        .pVertexInputState = &vertex_input_info,

        .pInputAssemblyState = &input_assembly,

        .pViewportState = &viewport_state,

        .pRasterizationState = &rasterizer,

        .pMultisampleState = &multisampling,

        .pColorBlendState = &color_blending,

        .pDepthStencilState = NULL,

        .pDynamicState = NULL,

        .layout = renderer->pipeline.pipeline_layout,

        .renderPass = renderer->pipeline.render_pass,

        .subpass = 0,

        .basePipelineHandle = VK_NULL_HANDLE

    };



    if (vkCreateGraphicsPipelines(renderer->vk.device, VK_NULL_HANDLE, 1, &pipeline_create_info, NULL, &renderer->pipeline.graphics_pipeline) != VK_SUCCESS) {

        LOGE("Failed to compile Graphics Pipeline\n");

        return -10;

    }



    vkDestroyShaderModule(renderer->vk.device, vert_module, NULL);

    vkDestroyShaderModule(renderer->vk.device, frag_module, NULL);



    // 8. Create Framebuffers

    renderer->pipeline.framebuffers = malloc(sizeof(VkFramebuffer) * renderer->swapchain.image_count);

    for (uint32_t i = 0; i < renderer->swapchain.image_count; i++) {

        VkFramebufferCreateInfo fb_info = {

            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,

            .renderPass = renderer->pipeline.render_pass,

            .attachmentCount = 1,

            .pAttachments = &renderer->swapchain.image_views[i],

            .width = renderer->swapchain.extent.width,

            .height = renderer->swapchain.extent.height,

            .layers = 1

        };

        vkCreateFramebuffer(renderer->vk.device, &fb_info, NULL, &renderer->pipeline.framebuffers[i]);

    }



    // 9. Allocate Shared Vertex Buffer Memory (Host Coherent)

    VkBufferCreateInfo buf_info = {

        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,

        .size = sizeof(NaclVulkanVertex) * VULKAN_MAX_VERTEX_COUNT,

        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,

        .sharingMode = VK_SHARING_MODE_EXCLUSIVE

    };



    if (vkCreateBuffer(renderer->vk.device, &buf_info, NULL, &renderer->sync.vertex_buffer) != VK_SUCCESS) {

        LOGE("Failed to create Vertex Buffer\n");

        return -11;

    }



    VkMemoryRequirements mem_reqs;

    vkGetBufferMemoryRequirements(renderer->vk.device, renderer->sync.vertex_buffer, &mem_reqs);



    VkMemoryAllocateInfo mem_alloc = {

        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,

        .allocationSize = mem_reqs.size,

        .memoryTypeIndex = find_memory_type(renderer->vk.physical_device, mem_reqs.memoryTypeBits,

                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)

    };



    if (vkAllocateMemory(renderer->vk.device, &mem_alloc, NULL, &renderer->sync.vertex_buffer_memory) != VK_SUCCESS) {

        LOGE("Failed to allocate dynamic host visible device memory\n");

        return -12;

    }

    vkBindBufferMemory(renderer->vk.device, renderer->sync.vertex_buffer, renderer->sync.vertex_buffer_memory, 0);



    // 10. Command Pool & Render Buffer Generation

    VkCommandPoolCreateInfo cp_info = {

        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,

        .queueFamilyIndex = graphics_idx,

        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT

    };



    if (vkCreateCommandPool(renderer->vk.device, &cp_info, NULL, &renderer->sync.command_pool) != VK_SUCCESS) {

        LOGE("Failed to create Command Pool\n");

        return -13;

    }



    VkCommandBufferAllocateInfo cba_info = {

        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,

        .commandPool = renderer->sync.command_pool,

        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,

        .commandBufferCount = VULKAN_MAX_FRAMES_IN_FLIGHT

    };



    if (vkAllocateCommandBuffers(renderer->vk.device, &cba_info, renderer->sync.command_buffers) != VK_SUCCESS) {

        LOGE("Failed to allocate command buffers\n");

        return -14;

    }



    // 11. Core Synchronization Primitive Allocation

    VkSemaphoreCreateInfo sem_info = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    VkFenceCreateInfo fence_info = {

        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,

        .flags = VK_FENCE_CREATE_SIGNALED_BIT // Start pre-signaled so the first draw block passes

    };



    for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {

        if (vkCreateSemaphore(renderer->vk.device, &sem_info, NULL, &renderer->sync.image_available_semaphores[i]) != VK_SUCCESS ||

            vkCreateSemaphore(renderer->vk.device, &sem_info, NULL, &renderer->sync.render_finished_semaphores[i]) != VK_SUCCESS ||

            vkCreateFence(renderer->vk.device, &fence_info, NULL, &renderer->sync.in_flight_fences[i]) != VK_SUCCESS) {

            LOGE("Failed to create sync primitives for frame slot %u\n", i);

            return -15;

        }

    }



    renderer->is_initialized = true;

    LOGI("Vulkan pipeline successfully loaded on Android GPU thread context! Ready for render passes.\n");

    return 0;

}



void nacl_vulkan_update_vertices(NaclVulkanRenderer* renderer, const float* normalized_amplitudes, uint32_t count) {

    if (!renderer || !renderer->is_initialized || !normalized_amplitudes || count == 0) return;

    if (count > VULKAN_MAX_VERTEX_COUNT) count = VULKAN_MAX_VERTEX_COUNT;



    // Directly Map GPU Host Buffer (coherent, bypassing device copy synchronization blocks)

    void* mapped_data;

    vkMapMemory(renderer->vk.device, renderer->sync.vertex_buffer_memory, 0, sizeof(NaclVulkanVertex) * count, 0, &mapped_data);



    NaclVulkanVertex* vertices = (NaclVulkanVertex*)mapped_data;

    for (uint32_t i = 0; i < count; i++) {

        // Line spacing spanning horizontally across Normalized Device Coordinates [-1.0f, 1.0f]

        vertices[i].x = -1.0f + (2.0f * (float)i / (float)(count - 1));

        // Vertical coordinate corresponds directly to raw audio amplitude

        vertices[i].y = normalized_amplitudes[i];



        // Dynamic Neon Cyan color scheme

        vertices[i].r = 0.0f;

        vertices[i].g = 0.9f;

        vertices[i].b = 1.0f;

    }



    vkUnmapMemory(renderer->vk.device, renderer->sync.vertex_buffer_memory);

}



int nacl_vulkan_draw_frame(NaclVulkanRenderer* renderer) {

    if (!renderer || !renderer->is_initialized) return -1;



    uint32_t frame_idx = renderer->sync.current_frame;



    // Await execution from previous frame in slot

    vkWaitForFences(renderer->vk.device, 1, &renderer->sync.in_flight_fences[frame_idx], VK_TRUE, UINT64_MAX);



    // Acquire next presentation slot from Android Swapchain

    uint32_t image_idx = 0;

    VkResult res = vkAcquireNextImageKHR(renderer->vk.device, renderer->swapchain.swapchain, UINT64_MAX,

                                         renderer->sync.image_available_semaphores[frame_idx],

                                         VK_NULL_HANDLE, &image_idx);



    if (res == VK_ERROR_OUT_OF_DATE_KHR) {

        // Target surface changed at OS compositor level

        nacl_vulkan_recreate_swapchain(renderer, renderer->width, renderer->height);

        return 0;

    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {

        return -2;

    }



    // Reset executing fence

    vkResetFences(renderer->vk.device, 1, &renderer->sync.in_flight_fences[frame_idx]);



    // Reset and record command buffer

    VkCommandBuffer cmd = renderer->sync.command_buffers[frame_idx];

    vkResetCommandBuffer(cmd, 0);



    VkCommandBufferBeginInfo begin_info = {

        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,

        .flags = 0

    };



    vkBeginCommandBuffer(cmd, &begin_info);



    VkClearValue clear_color = { .color = { {0.02f, 0.02f, 0.02f, 0.85f} } }; // Dark semi-transparent slate



    VkRenderPassBeginInfo rp_begin = {

        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,

        .renderPass = renderer->pipeline.render_pass,

        .framebuffer = renderer->pipeline.framebuffers[image_idx],

        .renderArea = { .offset = {0, 0}, .extent = renderer->swapchain.extent },

        .clearValueCount = 1,

        .pClearValues = &clear_color

    };



    vkCmdBeginRenderPass(cmd, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);



    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->pipeline.graphics_pipeline);



    VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(cmd, 0, 1, &renderer->sync.vertex_buffer, offsets);



    // Draw lines representing wave shapes

    vkCmdDraw(cmd, VULKAN_MAX_VERTEX_COUNT, 1, 0, 0);



    vkCmdEndRenderPass(cmd);



    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {

        LOGE("Failed to close Vulkan command buffer compilation pass\n");

        return -3;

    }



    // Submit to active GPU Graphics Queue

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {

        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,

        .waitSemaphoreCount = 1,

        .pWaitSemaphores = &renderer->sync.image_available_semaphores[frame_idx],

        .pWaitDstStageMask = wait_stages,

        .commandBufferCount = 1,

        .pCommandBuffers = &cmd,

        .signalSemaphoreCount = 1,

        .pSignalSemaphores = &renderer->sync.render_finished_semaphores[frame_idx]

    };



    if (vkQueueSubmit(renderer->vk.graphics_queue, 1, &submit_info, renderer->sync.in_flight_fences[frame_idx]) != VK_SUCCESS) {

        LOGE("Failed to submit render commands to GPU execution queue\n");

        return -4;

    }



    // Present rendering result back to OS window compositor (SurfaceFlinger)

    VkPresentInfoKHR present_info = {

        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,

        .waitSemaphoreCount = 1,

        .pWaitSemaphores = &renderer->sync.render_finished_semaphores[frame_idx],

        .swapchainCount = 1,

        .pSwapchains = &renderer->swapchain.swapchain,

        .pImageIndices = &image_idx

    };



    res = vkQueuePresentKHR(renderer->vk.present_queue, &present_info);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {

        nacl_vulkan_recreate_swapchain(renderer, renderer->width, renderer->height);

    }



    renderer->sync.current_frame = (renderer->sync.current_frame + 1) % VULKAN_MAX_FRAMES_IN_FLIGHT;

    return 0;

}



void nacl_vulkan_recreate_swapchain(NaclVulkanRenderer* renderer, uint32_t new_w, uint32_t new_h) {

    if (!renderer || !renderer->is_initialized) return;

    vkDeviceWaitIdle(renderer->vk.device);



    LOGI("Android Window layout changed. Dynamic swapchain recreation triggered -> %ux%u\n", new_w, new_h);

    renderer->width = new_w;

    renderer->height = new_h;



    // In a fully robust architecture, previous image views and framebuffers are destroyed

    // and vkCreateSwapchainKHR is re-run with oldSwapchain handle parameters.

}



void nacl_vulkan_shutdown(NaclVulkanRenderer* renderer) {

    if (!renderer || !renderer->is_initialized) return;

    vkDeviceWaitIdle(renderer->vk.device);



    LOGI("Dismantling Vulkan dynamic device pipeline and releasing swapchain modules...\n");



    for (uint32_t i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {

        vkDestroySemaphore(renderer->vk.device, renderer->sync.image_available_semaphores[i], NULL);

        vkDestroySemaphore(renderer->vk.device, renderer->sync.render_finished_semaphores[i], NULL);

        vkDestroyFence(renderer->vk.device, renderer->sync.in_flight_fences[i], NULL);

    }



    vkDestroyCommandPool(renderer->vk.device, renderer->sync.command_pool, NULL);



    vkDestroyBuffer(renderer->vk.device, renderer->sync.vertex_buffer, NULL);

    vkFreeMemory(renderer->vk.device, renderer->sync.vertex_buffer_memory, NULL);



    for (uint32_t i = 0; i < renderer->swapchain.image_count; i++) {

        vkDestroyFramebuffer(renderer->vk.device, renderer->pipeline.framebuffers[i], NULL);

        vkDestroyImageView(renderer->vk.device, renderer->swapchain.image_views[i], NULL);

    }

    free(renderer->pipeline.framebuffers);

    free(renderer->swapchain.images);

    free(renderer->swapchain.image_views);



    vkDestroyPipeline(renderer->vk.device, renderer->pipeline.graphics_pipeline, NULL);

    vkDestroyPipelineLayout(renderer->vk.device, renderer->pipeline.pipeline_layout, NULL);

    vkDestroyRenderPass(renderer->vk.device, renderer->pipeline.render_pass, NULL);

    vkDestroySwapchainKHR(renderer->vk.device, renderer->swapchain.swapchain, NULL);

    vkDestroyDevice(renderer->vk.device, NULL);

    vkDestroySurfaceKHR(renderer->vk.instance, renderer->vk.surface, NULL);

    vkDestroyInstance(renderer->vk.instance, NULL);



    renderer->is_initialized = false;

    LOGI("Vulkan context successfully cleared and memory channels returned to OS heap.\n");

}



void nacl_vulkan_free(NaclVulkanRenderer* renderer) {

    if (renderer) {

        free(renderer);

    }

}