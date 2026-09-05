#ifndef NACL_VULKAN_RENDERER_H

#define NACL_VULKAN_RENDERER_H



#include <vulkan/vulkan.h>

#include <vulkan/vulkan_android.h>

#include <android/native_window.h>

#include <stdbool.h>

#include <stdint.h>



#ifdef __cplusplus

extern "C" {

#endif



// Limits for Waveform Overlay Resolution

#define VULKAN_MAX_FRAMES_IN_FLIGHT 2

#define VULKAN_MAX_VERTEX_COUNT     512



typedef struct {

    float x, y;     // Position in Normalized Device Coordinates (NDC) [-1.0, 1.0]

    float r, g, b;  // Vertex Color

} NaclVulkanVertex;



typedef struct {

    VkInstance       instance;

    VkSurfaceKHR     surface;

    VkPhysicalDevice physical_device;

    VkDevice         device;

    VkQueue          graphics_queue;

    VkQueue          present_queue;

    uint32_t         graphics_family_idx;

    uint32_t         present_family_idx;

} NaclVulkanContext;



typedef struct {

    VkSwapchainKHR   swapchain;

    uint32_t         image_count;

    VkImage*         images;

    VkImageView*     image_views;

    VkFormat         format;

    VkExtent2D       extent;

} NaclVulkanSwapchain;



typedef struct {

    VkRenderPass     render_pass;

    VkPipelineLayout pipeline_layout;

    VkPipeline       graphics_pipeline;

    VkFramebuffer*   framebuffers;

} NaclVulkanPipeline;



typedef struct {

    VkBuffer         vertex_buffer;

    VkDeviceMemory   vertex_buffer_memory;

    VkCommandPool    command_pool;

    VkCommandBuffer  command_buffers[VULKAN_MAX_FRAMES_IN_FLIGHT];

    VkSemaphore      image_available_semaphores[VULKAN_MAX_FRAMES_IN_FLIGHT];

    VkSemaphore      render_finished_semaphores[VULKAN_MAX_FRAMES_IN_FLIGHT];

    VkFence          in_flight_fences[VULKAN_MAX_FRAMES_IN_FLIGHT];

    uint32_t         current_frame;

} NaclVulkanSync;



typedef struct {

    NaclVulkanContext   vk;

    NaclVulkanSwapchain swapchain;

    NaclVulkanPipeline  pipeline;

    NaclVulkanSync      sync;

    ANativeWindow*      window;

    bool                is_initialized;

    uint32_t            width;

    uint32_t            height;

} NaclVulkanRenderer;



// Core C ABI Controls

NaclVulkanRenderer* nacl_vulkan_alloc(void);

int  nacl_vulkan_init(NaclVulkanRenderer* renderer, ANativeWindow* window, uint32_t w, uint32_t h);

void nacl_vulkan_update_vertices(NaclVulkanRenderer* renderer, const float* normalized_amplitudes, uint32_t count);

int  nacl_vulkan_draw_frame(NaclVulkanRenderer* renderer);

void nacl_vulkan_recreate_swapchain(NaclVulkanRenderer* renderer, uint32_t new_w, uint32_t new_h);

void nacl_vulkan_shutdown(NaclVulkanRenderer* renderer);

void nacl_vulkan_free(NaclVulkanRenderer* renderer);



#ifdef __cplusplus

}

#endif



#endif // NACL_VULKAN_RENDERER_H