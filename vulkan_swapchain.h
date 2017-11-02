#ifndef vulkan_swapchain_h_INCLUDED
#define vulkan_swapchain_h_INCLUDED

#include <vulkan/vulkan.h>

struct VulkanSwapchain {
    VkExtent2D         imageExtent;
    VkSurfaceFormatKHR imageFormat;
    VkPresentModeKHR   presentMode;
    VkSwapchain        swapchain;

    uint32_t         imageCount;
    VkImage         *images;
    VkImageView     *imageViews;
    VkFramebuffer   *framebuffers;
    VkCommandBuffer *commandBuffers;

    VkRenderPass     renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline       graphicsPipeline;

    VkPhysicalDevice physicalDevice;
    VkDevice         device;
};

struct VulkanSwapchain createSwapchain(VkDevice device, VkPhysicalDevice physicalDevice);

#endif // vulkan_swapchain_h_INCLUDED
