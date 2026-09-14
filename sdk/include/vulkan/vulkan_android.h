#ifndef VULKAN_ANDROID_H_
#define VULKAN_ANDROID_H_

#include <vulkan/vulkan.h>
#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VkAndroidSurfaceCreateInfoKHR {
    VkStructureType                   sType;
    const void*                       pNext;
    uint32_t                          flags;
    ANativeWindow*                    window;
} VkAndroidSurfaceCreateInfoKHR;

#ifdef __cplusplus
}
#endif

#endif // VULKAN_ANDROID_H_
