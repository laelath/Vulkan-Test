#ifndef VKTOOLS_H
#define VKTOOLS_H

#include <stdbool.h>
#include <assert.h>

#include <vulkan/vulkan.h>

const char * getVkResultString(VkResult err);

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                           VkCommandBuffer commandBuffer);

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties);

void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                  VkBuffer *buffer, VkDeviceMemory *bufferMemory);

void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

void createImage(VkPhysicalDevice physicalDevice, VkDevice device, uint32_t width, uint32_t height,
                 VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                 VkMemoryPropertyFlags properties, VkImage *image, VkDeviceMemory *imageMemory);

void transitionImageLayout(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                           VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

void copyBufferToImage(VkDevice device, VkCommandPool commandPool, VkQueue queue,
                       VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

VkFormat findSupportedFormat(VkPhysicalDevice, const VkFormat *candidates, size_t numCandidates,
                             VkImageTiling tiling, VkFormatFeatureFlags features);

bool hasStencilComponent(VkFormat format);

VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);

char * getFile(const char *fileName, size_t *length);

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
