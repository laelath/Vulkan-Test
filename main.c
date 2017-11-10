#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define LINMATH_VULKAN_PROJECTIONS
#include <linmath.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define VMD_LOADER_IMPLEMENTATION
#include <vmd_loader.h>

#include "vktools.h"



#ifndef NDEBUG
#define VALIDATION_LAYERS
const char *validationLayers[] = {
    "VK_LAYER_LUNARG_standard_validation"
};
const size_t layerCount = sizeof(validationLayers) / sizeof(validationLayers[0]);
#endif // NDEBUG

const char *deviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
const size_t deviceExtensionCount = sizeof(deviceExtensions) / sizeof(deviceExtensions[0]);



int windowWidth  = 800, windowHeight = 600;
GLFWwindow *window;



struct ShaderCache {
    VkShaderModule vert;
    VkShaderModule frag;
} shaders;

struct VulkanData {
    // Core Vulkan stuff
    VkInstance                 instance;
    VkPhysicalDevice           physicalDevice;
    VkPhysicalDeviceProperties physicalDeviceProps;
    VkDevice                   device;
    VkQueue                    graphicsQueue;
    VkQueue                    presentQueue;

    // Queue indicies
    int graphicsFamily;
    int presentFamily;

    // WSI stuff
    VkSurfaceKHR surface;

    // Swapchain stuff
    VkExtent2D               swapchainImageExtent;
    VkSurfaceFormatKHR       swapchainImageFormat;
    VkPresentModeKHR         swapchainPresentMode;
    VkSwapchainKHR           swapchain;

    // Swapchain render target data
    uint32_t         swapchainImageCount;
    VkImage         *swapchainImages;
    VkImageView     *swapchainImageViews;
    VkFramebuffer   *swapchainFramebuffers;
    VkCommandBuffer *swapchainCommandBuffers;

    // Graphics pipline data
    VkRenderPass     renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline       graphicsPipeline;

    VkDescriptorSetLayout descriptorSetLayout;

    VkCommandPool commandPool;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;

    // Depth buffer data
    VkFormat       depthFormat;
    VkImage        depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView    depthImageView;

    // Multisample buffers
    VkSampleCountFlags samples;
    VkImage            msImage;
    VkDeviceMemory     msImageMemory;
    VkImageView        msImageView;
    VkImage            msDepthImage;
    VkDeviceMemory     msDepthImageMemory;
    VkImageView        msDepthImageView;

    // Vertex and index buffers
    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer       indexBuffer;
    VkDeviceMemory indexBufferMemory;

    // Uniform buffer stuff
    VkBuffer       uniformBuffer;
    VkDeviceMemory uniformBufferMemory;

    // Texture stuff
    uint32_t       textureMipLevels;
    VkImage        textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView    textureImageView;
    VkSampler      textureSampler;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet  descriptorSet;

#ifdef VALIDATION_LAYERS
    VkDebugReportCallbackEXT callback;

    PFN_vkCreateDebugReportCallbackEXT  fpCreateDebugReportCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT fpDestroyDebugReportCallbackEXT;
#endif // VALIDATION_LAYERS
} vkData;



typedef struct {
    vec3 pos;
    vec2 texCoord;
    vec3 color;
} Vertex;

Vertex   *vertices;
uint32_t *indices;

size_t vertexCount;
size_t indexCount;



struct Positions {
    float distance;
    vec2  direction;
} positions;

struct VertexTransforms {
    mat4x4 model;
    mat4x4 view;
    mat4x4 proj;
} mats;



struct InputInfo {
    double mouseX;
    double mouseY;
    bool mouse2Pressed;
} inputs;



#ifdef VALIDATION_LAYERS
bool checkValidationLayerSupport()
{
    uint32_t availableLayerCount;
    vkEnumerateInstanceLayerProperties(&availableLayerCount, NULL);

    VkLayerProperties *availableLayers = malloc(availableLayerCount * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers);

    for (size_t i = 0; i < layerCount; i++) {
        bool layerFound = false;

        for (size_t j = 0; j < availableLayerCount; j++) {
            if (strcmp(validationLayers[i], availableLayers[j].layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            free(availableLayers);
            return false;
        }
    }

    free(availableLayers);
    return true;
}
#endif // VALIDATION_LAYERS

void createInstance()
{
    VkApplicationInfo appInfo = {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "Custom",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_0
    };

    // Get the required extensions based on whether validation layers are enabled
    unsigned int glfwExtensionCount = 0;
    const char **glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    unsigned int extensionCount = glfwExtensionCount;
    const char **extensions;

#ifdef VALIDATION_LAYERS
    extensionCount += 1;
#endif // VALIDATION_LAYERS

    extensions = malloc(extensionCount * sizeof(char*));

    for (size_t i = 0; i < glfwExtensionCount; i++)
        extensions[i] = glfwExtensions[i];

#ifdef VALIDATION_LAYERS
    extensions[glfwExtensionCount] = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;
#endif // VALIDATION_LAYERS

    VkInstanceCreateInfo createInfo = {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &appInfo,
        .enabledExtensionCount   = extensionCount,
        .ppEnabledExtensionNames = extensions,
#ifdef VALIDATION_LAYERS
        .enabledLayerCount       = layerCount,
        .ppEnabledLayerNames     = validationLayers
#else
        .enabledLayerCount       = 0
#endif // VALIDATION_LAYERS
    };

#ifdef VALIDATION_LAYERS
    if (!checkValidationLayerSupport())
        ERR_EXIT("Requested validation layers not available\n");
#endif // VALIDATION_LAYERS

    VK_CHECK(vkCreateInstance(&createInfo, NULL, &vkData.instance));
    free(extensions);
}

void loadInstanceFunctions()
{
#ifdef VALIDATION_LAYERS
    GET_INSTANCE_PROC_ADDR(vkData.instance, vkData.fpCreateDebugReportCallbackEXT,
                           vkCreateDebugReportCallbackEXT);
    GET_INSTANCE_PROC_ADDR(vkData.instance, vkData.fpDestroyDebugReportCallbackEXT,
                           vkDestroyDebugReportCallbackEXT);
#endif // VALIDATION_LAYERS
}

#ifdef VALIDATION_LAYERS
VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char *layerPrefix,
    const char *msg,
    void *userData)
{
    fprintf(stderr, "Validation layer: %s\n", msg);
    return VK_FALSE;
}

void setupDebugCallback()
{
    VkDebugReportCallbackCreateInfoEXT createInfo = {
        .sType       = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
        .flags       = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT,
        .pfnCallback = vulkanDebugCallback
    };

    VK_CHECK(vkData.fpCreateDebugReportCallbackEXT(vkData.instance, &createInfo, NULL, &vkData.callback));
}
#endif // VALIDATION_LAYERS

void createSurface()
{
    VK_CHECK(glfwCreateWindowSurface(vkData.instance, window, NULL, &vkData.surface));
}

void pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vkData.instance, &deviceCount, NULL);

    if (deviceCount == 0)
        ERR_EXIT("Failed to find a GPU that supports Vulkan\n");

    VkPhysicalDevice *physicalDevices = malloc(deviceCount * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(vkData.instance, &deviceCount, physicalDevices);

    vkData.physicalDevice = VK_NULL_HANDLE;

    for (size_t i = 0; i < deviceCount; i++) {
        // Ensure the physical device supports the required features
        // Currently this is just anisotropy
        //      TODO: maybe make anisotropy optional?
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevices[i], &supportedFeatures);

        if (!supportedFeatures.samplerAnisotropy)
            continue;

        // Query for indices of the graphics and present queues
        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyCount, NULL);

        VkQueueFamilyProperties *queueFamilies = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyCount, queueFamilies);

        int graphicsFamily = -1, presentFamily = -1;

        for (size_t j = 0; j < queueFamilyCount; j++) {
            if (queueFamilies[j].queueCount > 0) {
                bool graphicsFamilyCandidate = false, presentFamilyCandidate = false;

                if (queueFamilies[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    graphicsFamilyCandidate = true;

                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevices[i], j, vkData.surface, &presentSupport);
                if (presentSupport)
                    presentFamilyCandidate = true;

                // Check if both graphics and present are supported in current queue
                if (graphicsFamilyCandidate && presentFamilyCandidate) {
                    graphicsFamily = j;
                    presentFamily = j;
                    break;
                } else {
                    if (graphicsFamily == -1 && graphicsFamilyCandidate)
                        graphicsFamily = j;
                    if (presentFamily == -1 && presentFamilyCandidate)
                        presentFamily = j;
                }
            }
        }

        free(queueFamilies);

        // If either the graphicsFamily or presentFamily queue indicies aren't valid
        // then the physical device can't be used, so we select another
        if (graphicsFamily == -1 || presentFamily == -1)
            continue;

        // Check that the physical device supports all the required extensions
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(physicalDevices[i], NULL, &extensionCount, NULL);

        VkExtensionProperties *availableExtensions = malloc(extensionCount * sizeof(VkExtensionProperties));
        vkEnumerateDeviceExtensionProperties(physicalDevices[i], NULL, &extensionCount, availableExtensions);

        bool foundMissing = false;

        for (size_t i = 0; i < deviceExtensionCount; i++) {
            bool found = false;
            for (size_t j = 0; j < extensionCount; j++) {
                if (strcmp(deviceExtensions[i], availableExtensions[j].extensionName) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                foundMissing = true;
                break;
            }
        }

        free(availableExtensions);

        // If there's an extension missing the physical device can't be used
        if (foundMissing)
            continue;

        // Check that the physical device has a valid surface format and present mode
        uint32_t formatCount, presentModeCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevices[i], vkData.surface, &formatCount, NULL);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevices[i], vkData.surface, &presentModeCount, NULL);

        // If there aren't any valid surface formats or present modes then the
        // physical device can't be used
        if (formatCount == 0 || presentModeCount == 0)
            continue;

        vkData.physicalDevice = physicalDevices[i];
        vkGetPhysicalDeviceProperties(physicalDevices[i], &vkData.physicalDeviceProps);

        vkData.graphicsFamily = graphicsFamily;
        vkData.presentFamily  = presentFamily;
        break;
    }

    free(physicalDevices);

    if (vkData.physicalDevice == VK_NULL_HANDLE)
        ERR_EXIT("Failed to find a suitable GPU\n");
}

void createLogicalDevice()
{
    float queuePriority = 1.0f;
    uint32_t queueCreateInfoCount = 2;
    VkDeviceQueueCreateInfo queueCreateInfos[2] = {
        {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = vkData.graphicsFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority
        }, {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = vkData.presentFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority
        }
    };

    if (vkData.graphicsFamily == vkData.presentFamily)
        queueCreateInfoCount = 1;

    VkPhysicalDeviceFeatures deviceFeatures = {
        .samplerAnisotropy = VK_TRUE
    };

    VkDeviceCreateInfo createInfo = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos       = queueCreateInfos,
        .queueCreateInfoCount    = queueCreateInfoCount,
        .pEnabledFeatures        = &deviceFeatures,
        .enabledExtensionCount   = deviceExtensionCount,
        .ppEnabledExtensionNames = deviceExtensions,
#ifdef VALIDATION_LAYERS
        .enabledLayerCount       = layerCount,
        .ppEnabledLayerNames     = validationLayers
#else
        .enabledLayerCount       = 0
#endif // VALIDATION_LAYERS
    };

    VK_CHECK(vkCreateDevice(vkData.physicalDevice, &createInfo, NULL, &vkData.device));

    vkGetDeviceQueue(vkData.device, vkData.graphicsFamily, 0, &vkData.graphicsQueue);
    vkGetDeviceQueue(vkData.device, vkData.presentFamily, 0, &vkData.presentQueue);
}

void getMultisampleCount()
{
    VkSampleCountFlags colorSamples = vkData.physicalDeviceProps.limits.framebufferColorSampleCounts;
    VkSampleCountFlags depthSamples = vkData.physicalDeviceProps.limits.framebufferDepthSampleCounts;

    VkSampleCountFlags samples = colorSamples > depthSamples ? depthSamples : colorSamples;

    if (samples & VK_SAMPLE_COUNT_16_BIT) vkData.samples = VK_SAMPLE_COUNT_16_BIT;
    else if (samples & VK_SAMPLE_COUNT_8_BIT) vkData.samples = VK_SAMPLE_COUNT_8_BIT;
    else if (samples & VK_SAMPLE_COUNT_4_BIT) vkData.samples = VK_SAMPLE_COUNT_4_BIT;
    else if (samples & VK_SAMPLE_COUNT_2_BIT) vkData.samples = VK_SAMPLE_COUNT_2_BIT;
    else vkData.samples = VK_SAMPLE_COUNT_1_BIT;
}

VkExtent2D selectExtent(VkSurfaceCapabilitiesKHR capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
        return capabilities.currentExtent;
    else {
        VkExtent2D extent = {windowWidth, windowHeight};

        if (extent.width < capabilities.minImageExtent.width)
            extent.width = capabilities.minImageExtent.width;
        else if (extent.width > capabilities.maxImageExtent.width)
            extent.width = capabilities.maxImageExtent.width;

        if (extent.height < capabilities.minImageExtent.height)
            extent.height = capabilities.minImageExtent.height;
        else if (extent.height > capabilities.maxImageExtent.height)
            extent.height = capabilities.maxImageExtent.height;

        return extent;
    }
}

VkSurfaceFormatKHR selectSurfaceFormat()
{
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkData.physicalDevice, vkData.surface, &formatCount, NULL);

    VkSurfaceFormatKHR *formats = malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkData.physicalDevice, vkData.surface, &formatCount, formats);

    if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        VkSurfaceFormatKHR format = {
            .format     = VK_FORMAT_B8G8R8A8_UNORM,
            .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        };
        free(formats);
        return format;
    }

    for (size_t i = 0; i < formatCount; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM
          && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            VkSurfaceFormatKHR format = {
                .format     = VK_FORMAT_B8G8R8A8_UNORM,
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            };
            free(formats);
            return format;
        }
    }

    VkSurfaceFormatKHR format = formats[0];
    free(formats);
    return format;
}

VkPresentModeKHR selectPresentMode()
{
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkData.physicalDevice, vkData.surface, &presentModeCount, NULL);

    VkPresentModeKHR *presentModes = malloc(presentModeCount * sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkData.physicalDevice, vkData.surface,
                                              &presentModeCount, presentModes);

    VkPresentModeKHR fallback = VK_PRESENT_MODE_FIFO_KHR;

    for (size_t i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            fallback = presentModes[i];
            free(presentModes);
            return fallback;
        } else if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
            fallback = presentModes[i];
    }

    free(presentModes);

    return fallback;
}

void createSwapchain(VkSwapchainKHR oldSwapchain)
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkData.physicalDevice, vkData.surface, &capabilities);

    vkData.swapchainImageExtent = selectExtent(capabilities);
    vkData.swapchainImageFormat = selectSurfaceFormat();
    vkData.swapchainPresentMode = selectPresentMode();

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        imageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = vkData.surface,
        .minImageCount    = imageCount,
        .imageFormat      = vkData.swapchainImageFormat.format,
        .imageColorSpace  = vkData.swapchainImageFormat.colorSpace,
        .imageExtent      = vkData.swapchainImageExtent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

        .preTransform     = capabilities.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = vkData.swapchainPresentMode,
        .clipped          = VK_TRUE,
        .oldSwapchain     = oldSwapchain
    };

    uint32_t queueFamilyIndices[] = {(uint32_t) vkData.graphicsFamily,
                                     (uint32_t) vkData.presentFamily};
    if (vkData.graphicsFamily != vkData.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = NULL;
    }

    VK_CHECK(vkCreateSwapchainKHR(vkData.device, &createInfo, NULL, &vkData.swapchain));

    vkGetSwapchainImagesKHR(vkData.device, vkData.swapchain, &vkData.swapchainImageCount, NULL);

    vkData.swapchainImages = malloc(vkData.swapchainImageCount * sizeof(VkImage));
    vkGetSwapchainImagesKHR(vkData.device, vkData.swapchain, &vkData.swapchainImageCount,
                            vkData.swapchainImages);
}

void createImageViews()
{
    vkData.swapchainImageViews = malloc(vkData.swapchainImageCount * sizeof(VkImageView));

    for (size_t i = 0; i < vkData.swapchainImageCount; i++) {
        vkData.swapchainImageViews[i] = createImageView(vkData.device, vkData.swapchainImages[i],
                                                        vkData.swapchainImageFormat.format,
                                                        VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}

void createRenderPass()
{
    // TODO: Properly find and save the depth format rather than calling this function several times
    VkFormat depthFormat = findDepthFormat(vkData.physicalDevice);

    VkAttachmentDescription attachments[] = {
        { // Multisample render target
            .format         = vkData.swapchainImageFormat.format,
            .samples        = vkData.samples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        }, { // Image to be resolved to for presenting
            .format         = vkData.swapchainImageFormat.format,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        }, { // Multisampled depth buffer
            .format         = depthFormat,
            .samples        = vkData.samples,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }, { // Resolved depth buffer
            .format         = depthFormat,
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        }
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference depthAttachmentRef = {
        .attachment = 2,
        .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    // For resolving the color attachment
    VkAttachmentReference resolveAttachmentRef = {
        .attachment = 1,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &colorAttachmentRef,
        .pResolveAttachments     = &resolveAttachmentRef,
        .pDepthStencilAttachment = &depthAttachmentRef
    };

    VkSubpassDependency dependencies[] = {
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        }, {
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        }
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = sizeof(attachments) / sizeof(attachments[0]),
        .pAttachments    = attachments,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = sizeof(dependencies) / sizeof(dependencies[0]),
        .pDependencies   = dependencies
    };

    VK_CHECK(vkCreateRenderPass(vkData.device, &renderPassInfo, NULL, &vkData.renderPass));
}

VkShaderModule createShaderModule(char * code, size_t codeLen)
{
    VkShaderModuleCreateInfo createInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = codeLen,
        .pCode    = (const uint32_t *) code
    };

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(vkData.device, &createInfo, NULL, &shaderModule));

    return shaderModule;
}

void loadShaders()
{
    size_t vertCodeLen, fragCodeLen;
    char *vertShaderCode = getFileData("shaders/vert.spv", &vertCodeLen);
    char *fragShaderCode = getFileData("shaders/frag.spv", &fragCodeLen);

    shaders.vert = createShaderModule(vertShaderCode, vertCodeLen);
    shaders.frag = createShaderModule(fragShaderCode, fragCodeLen);

    free(vertShaderCode);
    free(fragShaderCode);
}

void createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding transformsLayoutBinding = {
        .binding            = 0,
        .descriptorCount    = 1,
        .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImmutableSamplers = NULL, // Optional
        .stageFlags         = VK_SHADER_STAGE_VERTEX_BIT
    };

    VkDescriptorSetLayoutBinding samplerLayoutBinding = {
        .binding            = 1,
        .descriptorCount    = 1,
        .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImmutableSamplers = NULL, // Optional
        .stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutBinding bindings[] = {transformsLayoutBinding, samplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = sizeof(bindings) / sizeof(bindings[0]),
        .pBindings    = bindings
    };

    VK_CHECK(vkCreateDescriptorSetLayout(vkData.device, &layoutInfo, NULL, &vkData.descriptorSetLayout));
}

void createGraphicsPipeline()
{
    // Shader stuff
    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
        .module = shaders.vert,
        .pName  = "main"
    };

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = shaders.frag,
        .pName  = "main"
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input stuff
    VkVertexInputBindingDescription bindingDescription = {
        .binding   = 0,
        .stride    = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription attributeDescriptions[] = {
        {
            .binding  = 0,
            .location = 0,
            .format   = VK_FORMAT_R32G32B32_SFLOAT,
            .offset   = offsetof(Vertex, pos)
        }, {
            .binding  = 0,
            .location = 1,
            .format   = VK_FORMAT_R32G32B32_SFLOAT,
            .offset   = offsetof(Vertex, color)
        }, {
            .binding  = 0,
            .location = 2,
            .format   = VK_FORMAT_R32G32_SFLOAT,
            .offset   = offsetof(Vertex, texCoord)
        }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &bindingDescription,
        .vertexAttributeDescriptionCount = sizeof(attributeDescriptions) / sizeof(attributeDescriptions[0]),
        .pVertexAttributeDescriptions    = attributeDescriptions
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    // Viewport stuff
    VkViewport viewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float) vkData.swapchainImageExtent.width,
        .height   = (float) vkData.swapchainImageExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = vkData.swapchainImageExtent
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor
    };

    // Rasterizer stuff
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .lineWidth               = 1.0f,
        .cullMode                = VK_CULL_MODE_BACK_BIT,
        .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0.0f, // Optional
        .depthBiasClamp          = 0.0f, // Optional
        .depthBiasSlopeFactor    = 0.0f // Optional
    };

    // Multisample stuff
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable   = VK_FALSE,
        .rasterizationSamples  = vkData.samples,
        .minSampleShading      = 1.0f, // Optional
        .pSampleMask           = NULL, // Optional
        .alphaToCoverageEnable = VK_FALSE, // Optional
        .alphaToOneEnable      = VK_FALSE // Optional
    };

    // Depth stuff
    VkPipelineDepthStencilStateCreateInfo depthStencil = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable       = VK_TRUE,
        .depthWriteEnable      = VK_TRUE,
        .depthCompareOp        = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .minDepthBounds        = 0.0f, // Optional
        .maxDepthBounds        = 1.0f, // Optional
        .stencilTestEnable     = VK_FALSE,
        .front                 = {},
        .back                  = {}
    };

    // Color blending/transparency stuff
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
                             | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable         = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
        .colorBlendOp        = VK_BLEND_OP_ADD, // Optional
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE, // Optional
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
        .alphaBlendOp        = VK_BLEND_OP_ADD // Optional
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY, // Optional
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttachment,
        .blendConstants  = { 0.0f, 0.0f, 0.0f, 0.0f } // Optional
    };

    // Dynamic state stuff would go here if we had any

    // Pipeline layout for uniforms and stuff
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &vkData.descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = NULL
    };

    // Create pipeline layout
    VK_CHECK(vkCreatePipelineLayout(vkData.device, &pipelineLayoutInfo, NULL, &vkData.pipelineLayout));

    // Graphics pipeline creation info
    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2,
        .pStages             = shaderStages,
        .pVertexInputState   = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState      = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState   = &multisampling,
        .pDepthStencilState  = &depthStencil,
        .pColorBlendState    = &colorBlending,
        .pDynamicState       = NULL,
        .layout              = vkData.pipelineLayout,
        .renderPass          = vkData.renderPass,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE, // Optional
        .basePipelineIndex   = -1 // Optional
    };

    VK_CHECK(vkCreateGraphicsPipelines(vkData.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL,
                                      &vkData.graphicsPipeline));
}

void createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = vkData.graphicsFamily,
        .flags            = 0 // Optional
    };

    VK_CHECK(vkCreateCommandPool(vkData.device, &poolInfo, NULL, &vkData.commandPool));
}

void createDepthResources()
{
    vkData.depthFormat = findDepthFormat(vkData.physicalDevice);

    createImage(vkData.physicalDevice, vkData.device,
                vkData.swapchainImageExtent.width, vkData.swapchainImageExtent.height, vkData.depthFormat,
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT,
                1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vkData.depthImage, &vkData.depthImageMemory);

    vkData.depthImageView = createImageView(vkData.device, vkData.depthImage, vkData.depthFormat,
                                            VK_IMAGE_ASPECT_DEPTH_BIT, 1);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vkData.device, vkData.commandPool);

    VkImageSubresourceRange subresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };

    if (hasStencilComponent(vkData.depthFormat))
        subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    cmdTransitionImageLayout(commandBuffer, vkData.depthImage, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, subresourceRange);
                          //VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);

    endSingleTimeCommands(vkData.device, vkData.commandPool, vkData.graphicsQueue, commandBuffer);
}

void createMultisampleTarget()
{
    createImage(vkData.physicalDevice, vkData.device,
                vkData.swapchainImageExtent.width, vkData.swapchainImageExtent.height,
                vkData.swapchainImageFormat.format, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                vkData.samples, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &vkData.msImage, &vkData.msImageMemory);

    vkData.msImageView = createImageView(vkData.device, vkData.msImage, vkData.swapchainImageFormat.format,
                                         VK_IMAGE_ASPECT_COLOR_BIT, 1);

    createImage(vkData.physicalDevice, vkData.device,
                vkData.swapchainImageExtent.width, vkData.swapchainImageExtent.height,
                vkData.depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                vkData.samples, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &vkData.msDepthImage, &vkData.msDepthImageMemory);

    vkData.msDepthImageView = createImageView(vkData.device, vkData.msDepthImage,
                                              vkData.depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}

void createFramebuffers()
{
    vkData.swapchainFramebuffers = malloc(vkData.swapchainImageCount * sizeof(VkFramebuffer));

    for (size_t i = 0; i < vkData.swapchainImageCount; i++) {
        VkImageView attachments[] = {
            vkData.msImageView, vkData.swapchainImageViews[i],
            vkData.msDepthImageView, vkData.depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = vkData.renderPass,
            .attachmentCount = sizeof(attachments) / sizeof(attachments[0]),
            .pAttachments    = attachments,
            .width           = vkData.swapchainImageExtent.width,
            .height          = vkData.swapchainImageExtent.height,
            .layers          = 1
        };

        VK_CHECK(vkCreateFramebuffer(vkData.device, &framebufferInfo, NULL,
                                     &vkData.swapchainFramebuffers[i]));
    }
}

void createTextureImage()
{
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load("textures/steamsad_santa.png", &texWidth, &texHeight, &texChannels,
                                STBI_rgb_alpha);

    if (!pixels)
        ERR_EXIT("Unable to load texture image\n");

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(vkData.physicalDevice, vkData.device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &stagingBuffer, &stagingBufferMemory);

    void *data;
    vkMapMemory(vkData.device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(vkData.device, stagingBufferMemory);

    stbi_image_free(pixels);

    vkData.textureMipLevels = 1;

    createImage(vkData.physicalDevice, vkData.device, texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_SAMPLE_COUNT_1_BIT, vkData.textureMipLevels, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &vkData.textureImage, &vkData.textureImageMemory);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vkData.device, vkData.commandPool);

    VkImageSubresourceRange subresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };

    cmdTransitionImageLayout(commandBuffer, vkData.textureImage, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

    cmdCopyBufferToImage(commandBuffer, stagingBuffer, vkData.textureImage, texWidth, texHeight);

    cmdTransitionImageLayout(commandBuffer, vkData.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);

    endSingleTimeCommands(vkData.device, vkData.commandPool, vkData.graphicsQueue, commandBuffer);

    vkDestroyBuffer(vkData.device, stagingBuffer, NULL);
    vkFreeMemory(vkData.device, stagingBufferMemory, NULL);
}

void createTextureImageMipMapped()
{
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels = stbi_load("textures/chalet.jpg", &texWidth, &texHeight, &texChannels,
                                STBI_rgb_alpha);

    if (!pixels)
        ERR_EXIT("Unable to load texture image\n");

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(vkData.physicalDevice, vkData.device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &stagingBuffer, &stagingBufferMemory);

    void *data;
    vkMapMemory(vkData.device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, imageSize);
    vkUnmapMemory(vkData.device, stagingBufferMemory);

    stbi_image_free(pixels);

    vkData.textureMipLevels = floor(log2(texWidth > texHeight ? texWidth : texHeight)) + 1;

    createImage(vkData.physicalDevice, vkData.device, texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_SAMPLE_COUNT_1_BIT, vkData.textureMipLevels, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &vkData.textureImage, &vkData.textureImageMemory);

    VkCommandBuffer copyCommandBuffer = beginSingleTimeCommands(vkData.device, vkData.commandPool);

    VkImageSubresourceRange subresourceRange = {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1,
    };

    cmdTransitionImageLayout(copyCommandBuffer, vkData.textureImage, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

    cmdCopyBufferToImage(copyCommandBuffer, stagingBuffer, vkData.textureImage, texWidth, texHeight);

    cmdTransitionImageLayout(copyCommandBuffer, vkData.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange);

    endSingleTimeCommands(vkData.device, vkData.commandPool, vkData.graphicsQueue, copyCommandBuffer);

    vkDestroyBuffer(vkData.device, stagingBuffer, NULL);
    vkFreeMemory(vkData.device, stagingBufferMemory, NULL);

    // Generate the mip chain
    VkCommandBuffer blitCommandBuffer = beginSingleTimeCommands(vkData.device, vkData.commandPool);

    for (int32_t i = 1; i < vkData.textureMipLevels; i++) {
        VkImageBlit imageBlit = {
            .srcSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .mipLevel   = i - 1
            },
            .srcOffsets[1] = {
                .x = texWidth >> (i - 1),
                .y = texHeight >> (i - 1),
                .z = 1
            },
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .mipLevel   = i
            },
            .dstOffsets[1] = {
                .x = texWidth >> i,
                .y = texHeight >> i,
                .z = 1
            }
        };

        VkImageSubresourceRange mipSubRange = {
            .aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = i,
            .levelCount   = 1,
            .layerCount   = 1
        };

        cmdTransitionImageLayout(blitCommandBuffer, vkData.textureImage, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipSubRange);

        vkCmdBlitImage(blitCommandBuffer, vkData.textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       vkData.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &imageBlit, VK_FILTER_LINEAR);

        cmdTransitionImageLayout(blitCommandBuffer, vkData.textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mipSubRange);
    }

    subresourceRange.levelCount = vkData.textureMipLevels;
    cmdTransitionImageLayout(blitCommandBuffer, vkData.textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);

    endSingleTimeCommands(vkData.device, vkData.commandPool, vkData.graphicsQueue, blitCommandBuffer);
}

void createTextureImageView()
{
    vkData.textureImageView = createImageView(vkData.device, vkData.textureImage, VK_FORMAT_R8G8B8A8_UNORM,
                                              VK_IMAGE_ASPECT_COLOR_BIT, vkData.textureMipLevels);
}

void createTextureSampler()
{
    VkSamplerCreateInfo samplerInfo = {
        .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter               = VK_FILTER_LINEAR,
        .minFilter               = VK_FILTER_LINEAR,
        //.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        //.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        //.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .anisotropyEnable        = VK_TRUE,
        .maxAnisotropy           = 16,
        .borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable           = VK_FALSE,
        .compareOp               = VK_COMPARE_OP_ALWAYS,
        .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .mipLodBias              = 0.0f,
        .minLod                  = 0.0f,
        .maxLod                  = vkData.textureMipLevels
    };

    VK_CHECK(vkCreateSampler(vkData.device, &samplerInfo, NULL, &vkData.textureSampler));
}

void loadModel()
{
    size_t dataLen;
    char *data = getFileData("models/chalet.vmd", &dataLen);

    VmdData vmd;
    loadVmd(&vmd, data, dataLen);

    vertexCount = vmd.vertexCount;
    indexCount  = vmd.indexCount;

    vertices = malloc(vmd.vertexCount * sizeof(Vertex));
    indices  = malloc(vmd.indexCount * sizeof(uint32_t));

    size_t vertFloats = 5; // 3 position, 2 texcoord

    for (size_t i = 0; i < vmd.vertexCount; i++) {
        size_t offset = vertFloats * i;
        Vertex v = {
            .pos      = {vmd.vertices[offset + 0], vmd.vertices[offset + 1], vmd.vertices[offset + 2]},
            .texCoord = {vmd.vertices[offset + 3], vmd.vertices[offset + 4]},
            .color    = {1.0f, 1.0f, 1.0f}
        };
        vertices[i] = v;
    }

    memcpy(indices, vmd.indices, vmd.indexCount * sizeof(uint32_t));

    vmdFree(&vmd);
    free(data);
}

void createVertexBuffer()
{
    VkDeviceSize bufferSize = vertexCount * sizeof(Vertex);

    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(vkData.physicalDevice, vkData.device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &stagingBuffer, &stagingBufferMemory);

    void *data;
    vkMapMemory(vkData.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices, bufferSize);
    vkUnmapMemory(vkData.device, stagingBufferMemory);

    createBuffer(vkData.physicalDevice, vkData.device, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 &vkData.vertexBuffer, &vkData.vertexBufferMemory);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vkData.device, vkData.commandPool);

    cmdCopyBuffer(commandBuffer, stagingBuffer, vkData.vertexBuffer, bufferSize);

    endSingleTimeCommands(vkData.device, vkData.commandPool, vkData.graphicsQueue, commandBuffer);

    vkDestroyBuffer(vkData.device, stagingBuffer, NULL);
    vkFreeMemory(vkData.device, stagingBufferMemory, NULL);
}

void createIndexBuffer()
{
    VkDeviceSize bufferSize = indexCount * sizeof(uint32_t);

    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(vkData.physicalDevice, vkData.device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &stagingBuffer, &stagingBufferMemory);

    void *data;
    vkMapMemory(vkData.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices, bufferSize);
    vkUnmapMemory(vkData.device, stagingBufferMemory);

    createBuffer(vkData.physicalDevice, vkData.device, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 &vkData.indexBuffer, &vkData.indexBufferMemory);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vkData.device, vkData.commandPool);

    cmdCopyBuffer(commandBuffer, stagingBuffer, vkData.indexBuffer, bufferSize);

    endSingleTimeCommands(vkData.device, vkData.commandPool, vkData.graphicsQueue, commandBuffer);

    vkDestroyBuffer(vkData.device, stagingBuffer, NULL);
    vkFreeMemory(vkData.device, stagingBufferMemory, NULL);
}

void createUniformBuffer()
{
    VkDeviceSize bufferSize = sizeof(struct VertexTransforms);
    createBuffer(vkData.physicalDevice, vkData.device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &vkData.uniformBuffer, &vkData.uniformBufferMemory);
}

void createDescriptorPool()
{
    VkDescriptorPoolSize poolSizes[] = {
        {
            .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1
        }, {
            .type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1
        }
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = sizeof(poolSizes) / sizeof(poolSizes[0]),
        .pPoolSizes    = poolSizes,
        .maxSets       = 1
    };

    VK_CHECK(vkCreateDescriptorPool(vkData.device, &poolInfo, NULL, &vkData.descriptorPool));
}

void createDescriptorSet()
{
    VkDescriptorSetLayout layouts[] = {vkData.descriptorSetLayout};

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = vkData.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts        = layouts
    };

    VK_CHECK(vkAllocateDescriptorSets(vkData.device, &allocInfo, &vkData.descriptorSet));

    VkDescriptorBufferInfo bufferInfo = {
        .buffer = vkData.uniformBuffer,
        .offset = 0,
        .range  = sizeof(struct VertexTransforms)
    };

    VkDescriptorImageInfo imageInfo = {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .imageView   = vkData.textureImageView,
        .sampler     = vkData.textureSampler
    };

    VkWriteDescriptorSet descriptorWrites[] = {
        {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet           = vkData.descriptorSet,
            .dstBinding       = 0,
            .dstArrayElement  = 0,
            .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount  = 1,
            .pBufferInfo      = &bufferInfo
        }, {
            .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet           = vkData.descriptorSet,
            .dstBinding       = 1,
            .dstArrayElement  = 0,
            .descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount  = 1,
            .pImageInfo       = &imageInfo
        }
    };

    vkUpdateDescriptorSets(vkData.device, sizeof(descriptorWrites) / sizeof(descriptorWrites[0]),
                           descriptorWrites, 0, NULL);
}

void createCommandBuffers()
{
    VkCommandBufferAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = vkData.commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = vkData.swapchainImageCount
    };

    vkData.swapchainCommandBuffers = malloc(vkData.swapchainImageCount * sizeof(VkCommandBuffer));

    VK_CHECK(vkAllocateCommandBuffers(vkData.device, &allocInfo, vkData.swapchainCommandBuffers));

    for (size_t i = 0; i < vkData.swapchainImageCount; i++) {
        VkCommandBufferBeginInfo beginInfo = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags            = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            .pInheritanceInfo = NULL // Optional
        };

        vkBeginCommandBuffer(vkData.swapchainCommandBuffers[i], &beginInfo);

        VkClearValue clearValues[] = {
            {
                .color = {0.0f, 0.0f, 0.0f, 1.0f}
            }, {
                .color = {0.0f, 0.0f, 0.0f, 1.0f}
            }, {
                .depthStencil = {1.0f, 0}
            }
        };

        VkRenderPassBeginInfo renderPassInfo = {
            .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass  = vkData.renderPass,
            .framebuffer = vkData.swapchainFramebuffers[i],
            .renderArea = {
                .offset = {0, 0},
                .extent = vkData.swapchainImageExtent
            },
            .clearValueCount = sizeof(clearValues) / sizeof(clearValues[0]),
            .pClearValues    = clearValues
        };

        // Record draw commands into the command buffer
        vkCmdBeginRenderPass(vkData.swapchainCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(vkData.swapchainCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          vkData.graphicsPipeline);

        VkBuffer vertexBuffers[] = {vkData.vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(vkData.swapchainCommandBuffers[i], 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(vkData.swapchainCommandBuffers[i], vkData.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdBindDescriptorSets(vkData.swapchainCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                vkData.pipelineLayout, 0, 1, &vkData.descriptorSet, 0, NULL);

        vkCmdDrawIndexed(vkData.swapchainCommandBuffers[i], indexCount, 1, 0, 0, 0);
        vkCmdEndRenderPass(vkData.swapchainCommandBuffers[i]);

        VK_CHECK(vkEndCommandBuffer(vkData.swapchainCommandBuffers[i]));
    }
}

void createSemaphores()
{
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VK_CHECK(vkCreateSemaphore(vkData.device, &semaphoreInfo, NULL, &vkData.imageAvailableSemaphore));
    VK_CHECK(vkCreateSemaphore(vkData.device, &semaphoreInfo, NULL, &vkData.renderFinishedSemaphore));
}

void initVulkan()
{
    createInstance();
    loadInstanceFunctions();

#ifdef VALIDATION_LAYERS
    setupDebugCallback();
#endif // VALIDATION_LAYERS

    createSurface();

    pickPhysicalDevice();
    createLogicalDevice();

    getMultisampleCount();

    // Swapchain things
    createSwapchain(VK_NULL_HANDLE);
    createImageViews();
    createRenderPass();

    loadShaders();
    createDescriptorSetLayout();

    // Swapchain things
    createGraphicsPipeline();

    createCommandPool();

    // Swapchain things
    createDepthResources();
    createMultisampleTarget();
    createFramebuffers();

    //createTextureImage();
    createTextureImageMipMapped();
    createTextureImageView();
    createTextureSampler();

    loadModel();

    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffer();

    createDescriptorPool();
    createDescriptorSet();

    // Swapchain things
    createCommandBuffers();

    createSemaphores();
}



void recreateSwapchain()
{
    VkSwapchainKHR oldSwapchain = vkData.swapchain;

    uint32_t         oldImageCount     = vkData.swapchainImageCount;
    VkImage         *oldImages         = vkData.swapchainImages;
    VkImageView     *oldImageViews     = vkData.swapchainImageViews;
    VkFramebuffer   *oldFramebuffers   = vkData.swapchainFramebuffers;
    VkCommandBuffer *oldCommandBuffers = vkData.swapchainCommandBuffers;

    VkRenderPass     oldRenderPass       = vkData.renderPass;
    VkPipelineLayout oldPipelineLayout   = vkData.pipelineLayout;
    VkPipeline       oldGraphicsPipeline = vkData.graphicsPipeline;

    VkImageView    oldDepthImageView   = vkData.depthImageView;
    VkImage        oldDepthImage       = vkData.depthImage;
    VkDeviceMemory oldDepthImageMemory = vkData.depthImageMemory;

    VkImage        oldMsImage            = vkData.msImage;
    VkDeviceMemory oldMsImageMemory      = vkData.msImageMemory;
    VkImageView    oldMsImageView        = vkData.msImageView;
    VkImage        oldMsDepthImage       = vkData.msDepthImage;
    VkDeviceMemory oldMsDepthImageMemory = vkData.msDepthImageMemory;
    VkImageView    oldMsDepthImageView   = vkData.msDepthImageView;

    createSwapchain(oldSwapchain);
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createDepthResources();
    createMultisampleTarget();
    createFramebuffers();
    createCommandBuffers();

    // TODO: Temporary update for projection matrix
    float aspect = vkData.swapchainImageExtent.width / (float) vkData.swapchainImageExtent.height;
    mat4x4_perspective(mats.proj, M_PI / 4, aspect, 0.1f, 10.0f);
    // End TODO

    vkQueueWaitIdle(vkData.presentQueue);

    // Clean up old swapchain data
    vkDestroyImageView(vkData.device, oldDepthImageView, NULL);
    vkDestroyImage(vkData.device, oldDepthImage, NULL);
    vkFreeMemory(vkData.device, oldDepthImageMemory, NULL);

    vkDestroyImageView(vkData.device, oldMsImageView, NULL);
    vkDestroyImage(vkData.device, oldMsImage, NULL);
    vkFreeMemory(vkData.device, oldMsImageMemory, NULL);
    vkDestroyImageView(vkData.device, oldMsDepthImageView, NULL);
    vkDestroyImage(vkData.device, oldMsDepthImage, NULL);
    vkFreeMemory(vkData.device, oldMsDepthImageMemory, NULL);

    vkFreeCommandBuffers(vkData.device, vkData.commandPool, oldImageCount, oldCommandBuffers);
    free(oldCommandBuffers);

    vkDestroyPipeline(vkData.device, oldGraphicsPipeline, NULL);
    vkDestroyPipelineLayout(vkData.device, oldPipelineLayout, NULL);
    vkDestroyRenderPass(vkData.device, oldRenderPass, NULL);

    for (size_t i = 0; i < oldImageCount; i++) {
        vkDestroyFramebuffer(vkData.device, oldFramebuffers[i], NULL);
        vkDestroyImageView(vkData.device, oldImageViews[i], NULL);
    }

    free(oldFramebuffers);
    free(oldImageViews);
    free(oldImages);

    vkDestroySwapchainKHR(vkData.device, oldSwapchain, NULL);
}



void windowErrorCallback(int error, const char *desc)
{
    fprintf(stderr, "GLFW Error: %s\n", desc);
}

void windowKeyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void windowCursorCallback(GLFWwindow *window, double x, double y)
{
    if (inputs.mouse2Pressed) {
        double dx = x - inputs.mouseX;
        double dy = y - inputs.mouseY;

        inputs.mouseX = x;
        inputs.mouseY = y;

        positions.direction[0] -= dx * 0.004f;
        positions.direction[1] += dy * 0.004f;

        if (positions.direction[1] > 0.5 * M_PI)
            positions.direction[1] = 0.5 * M_PI;
        else if (positions.direction[1] < -0.5 * M_PI)
            positions.direction[1] = -0.5 * M_PI;

        if (positions.direction[0] > M_PI)
            positions.direction[0] -= 2 * M_PI;
        else if (positions.direction[0] < -M_PI)
            positions.direction[0] += 2 * M_PI;
    }
}

void windowButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            inputs.mouse2Pressed = true;
            glfwGetCursorPos(window, &inputs.mouseX, &inputs.mouseY);
        } else
            inputs.mouse2Pressed = false;
    }
}

void windowScrollCallback(GLFWwindow *window, double dx, double dy)
{
    positions.distance -= dy * positions.distance * 0.1f;
}

void windowResizeCallback(GLFWwindow *window, int width, int height)
{
    windowWidth = width;
    windowHeight = height;
}

void initWindow()
{
    if (!glfwInit())
        ERR_EXIT("Error initializing GLFW\n");

    glfwSetErrorCallback(windowErrorCallback);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(windowWidth, windowHeight, "Vulkan Test Program", NULL, NULL);
    if (!window)
        ERR_EXIT("Error creating GLFW window\n");

    glfwSetKeyCallback(window, windowKeyCallback);
    glfwSetCursorPosCallback(window, windowCursorCallback);
    glfwSetMouseButtonCallback(window, windowButtonCallback);
    glfwSetScrollCallback(window, windowScrollCallback);
    glfwSetWindowSizeCallback(window, windowResizeCallback);
}



void initMats()
{
    mat4x4_translate(mats.model, 0, -0.3, 0);

    positions.distance = 4.0f;
    positions.direction[0] = -M_PI / 4.0;
    positions.direction[1] =  M_PI / 12.0;

    float aspect = vkData.swapchainImageExtent.width / (float) vkData.swapchainImageExtent.height;
    mat4x4_perspective(mats.proj, M_PI / 4, aspect, 0.1f, 1000.0f);
}



void updateUniformBuffer(double delta)
{
    vec3 eye = {0.0f, 0.0f, -1.0f}, center = {0.0f, 0.0f, 0.0f}, up = {0.0f, 1.0f, 0.0f};
    vec3 horiz = {1.0f, 0.0f, 0.0f};
    quat vRot, hRot;

    quat_rotate(vRot, positions.direction[1], horiz);
    quat_rotate(hRot, positions.direction[0], up);
    quat_mul_vec3(eye, vRot, eye);
    quat_mul_vec3(eye, hRot, eye);

    vec3_scale(eye, eye, positions.distance);
    mat4x4_look_at(mats.view, eye, center, up);

    //mat4x4_rotate_Z(mats.model, mats.model, delta * (2 * M_PI) / 15.0f);

    void *data;
    vkMapMemory(vkData.device, vkData.uniformBufferMemory, 0, sizeof(mats), 0, &data);
    memcpy(data, &mats, sizeof(mats));
    vkUnmapMemory(vkData.device, vkData.uniformBufferMemory);
}

void renderFrame()
{
    // QueueWaitIdle is here before rendering so the CPU doesn't wait while the
    // GPU is rendering the previous frame
    vkQueueWaitIdle(vkData.presentQueue);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(vkData.device, vkData.swapchain, UINT64_MAX,
                                            vkData.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        ERR_EXIT("%s\n", getVkResultString(result));

    VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &vkData.imageAvailableSemaphore,
        .pWaitDstStageMask    = &waitStages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &vkData.swapchainCommandBuffers[imageIndex],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &vkData.renderFinishedSemaphore
    };

    VK_CHECK(vkQueueSubmit(vkData.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

    VkPresentInfoKHR presentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &vkData.renderFinishedSemaphore,
        .swapchainCount     = 1,
        .pSwapchains        = &vkData.swapchain,
        .pImageIndices      = &imageIndex,
        .pResults           = NULL // Optional
    };

    result = vkQueuePresentKHR(vkData.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        recreateSwapchain();
    else if (result != VK_SUCCESS)
        ERR_EXIT("%s\n", getVkResultString(result));
}

void mainLoop()
{
    double prevTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double currTime = glfwGetTime();
        double delta = currTime - prevTime;
        prevTime = currTime;

        // process state stuff here if there ever is any

        updateUniformBuffer(delta);
        renderFrame();
    }

    vkDeviceWaitIdle(vkData.device);
}



void cleanupSwapchain()
{
    vkDestroyImageView(vkData.device, vkData.depthImageView, NULL);
    vkDestroyImage(vkData.device, vkData.depthImage, NULL);
    vkFreeMemory(vkData.device, vkData.depthImageMemory, NULL);

    vkDestroyImageView(vkData.device, vkData.msImageView, NULL);
    vkDestroyImage(vkData.device, vkData.msImage, NULL);
    vkFreeMemory(vkData.device, vkData.msImageMemory, NULL);
    vkDestroyImageView(vkData.device, vkData.msDepthImageView, NULL);
    vkDestroyImage(vkData.device, vkData.msDepthImage, NULL);
    vkFreeMemory(vkData.device, vkData.msDepthImageMemory, NULL);

    vkFreeCommandBuffers(vkData.device, vkData.commandPool, vkData.swapchainImageCount,
                         vkData.swapchainCommandBuffers);
    free(vkData.swapchainCommandBuffers);

    vkDestroyPipeline(vkData.device, vkData.graphicsPipeline, NULL);
    vkDestroyPipelineLayout(vkData.device, vkData.pipelineLayout, NULL);
    vkDestroyRenderPass(vkData.device, vkData.renderPass, NULL);

    for (size_t i = 0; i < vkData.swapchainImageCount; i++) {
        vkDestroyFramebuffer(vkData.device, vkData.swapchainFramebuffers[i], NULL);
        vkDestroyImageView(vkData.device, vkData.swapchainImageViews[i], NULL);
    }

    free(vkData.swapchainFramebuffers);
    free(vkData.swapchainImageViews);
    free(vkData.swapchainImages);

    vkDestroySwapchainKHR(vkData.device, vkData.swapchain, NULL);
}

void cleanup()
{
    cleanupSwapchain();

    vkDestroySampler(vkData.device, vkData.textureSampler, NULL);
    vkDestroyImageView(vkData.device, vkData.textureImageView, NULL);
    vkDestroyImage(vkData.device, vkData.textureImage, NULL);
    vkFreeMemory(vkData.device, vkData.textureImageMemory, NULL);

    vkDestroyDescriptorPool(vkData.device, vkData.descriptorPool, NULL);

    vkDestroyDescriptorSetLayout(vkData.device, vkData.descriptorSetLayout, NULL);
    vkDestroyBuffer(vkData.device, vkData.uniformBuffer, NULL);
    vkFreeMemory(vkData.device, vkData.uniformBufferMemory, NULL);

    vkDestroyBuffer(vkData.device, vkData.indexBuffer, NULL);
    vkFreeMemory(vkData.device, vkData.indexBufferMemory, NULL);

    vkDestroyBuffer(vkData.device, vkData.vertexBuffer, NULL);
    vkFreeMemory(vkData.device, vkData.vertexBufferMemory, NULL);

    vkDestroyShaderModule(vkData.device, shaders.vert, NULL);
    vkDestroyShaderModule(vkData.device, shaders.frag, NULL);

    vkDestroySemaphore(vkData.device, vkData.imageAvailableSemaphore, NULL);
    vkDestroySemaphore(vkData.device, vkData.renderFinishedSemaphore, NULL);

    vkDestroyCommandPool(vkData.device, vkData.commandPool, NULL);

    vkDestroyDevice(vkData.device, NULL);

#ifdef VALIDATION_LAYERS
    vkData.fpDestroyDebugReportCallbackEXT(vkData.instance, vkData.callback, NULL);
#endif // VALIDATION_LAYERS

    vkDestroySurfaceKHR(vkData.instance, vkData.surface, NULL);
    vkDestroyInstance(vkData.instance, NULL);

    glfwDestroyWindow(window);
    glfwTerminate();
}



int main(int argc, char *argv[])
{
    initWindow();
    initVulkan();

    initMats();

    mainLoop();

    cleanup();
}
