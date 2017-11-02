#ifndef VKTOOLS_H
#define VKTOOLS_H

#include <stdbool.h>
#include <assert.h>

#include <vulkan/vulkan.h>

const char * getVkResultString(VkResult err);

char * getFile(const char * fileName, size_t * length);

#define ERR_EXIT(err_msg...) \
{ \
    fprintf(stderr, err_msg); \
    fflush(stderr); \
    exit(1); \
}

#define VK_CHECK(f) \
{ \
    VkResult err = (f); \
    if (err != VK_SUCCESS) \
    { \
        fprintf(stderr, "Fatal: VkResult is \"%s\" in %s at line %d \n", getVkResultString(err), __FILE__, __LINE__); \
        assert(err == VK_SUCCESS); \
    } \
}

#define GET_INSTANCE_PROC_ADDR(instance, target, entrypoint) \
{ \
    target = (PFN_##entrypoint) vkGetInstanceProcAddr(instance, #entrypoint); \
    if (target == NULL) \
        ERR_EXIT("vkGetInstanceProcAddr failed to find " #entrypoint "\n"); \
}

#define GET_DEVICE_PROC_ADDR(device, target, entrypoint) \
{ \
    target = (PFN_##entrypoint) vkGetDeviceProcAddr(device, #entrypoint); \
    if (target == NULL) \
        ERR_EXIT("vkGetDeviceProcAddr failed to find " #entrypoint "\n"); \
}

#endif //VKTOOLS_H
