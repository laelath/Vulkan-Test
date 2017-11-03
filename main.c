#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <linmath.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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
    VkInstance       instance;
    VkPhysicalDevice physicalDevice;
    VkDevice         device;
    VkQueue          graphicsQueue;
    VkQueue          presentQueue;

    // Queue indicies
    int graphicsFamily;
    int presentFamily;

    // WSI stuff
    VkSurfaceKHR             surface;

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

    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer       indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkBuffer       uniformBuffer;
    VkDeviceMemory uniformBufferMemory;

    VkImage        textureImage;
    VkDeviceMemory textureImageMemory;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet  descriptorSet;

#ifdef VALIDATION_LAYERS
    VkDebugReportCallbackEXT callback;

    PFN_vkCreateDebugReportCallbackEXT  fpCreateDebugReportCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT fpDestroyDebugReportCallbackEXT;
#endif // VALIDATION_LAYERS
} vkData;



struct Vertex {
    vec2 pos;
    vec3 color;
};

const struct Vertex vertices[] = {
    // Position    ,  Color
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
};
const size_t vertexCount = sizeof(vertices) / sizeof(vertices[0]);

const uint16_t indices[] = {
    0, 1, 2, 2, 3, 0
};
const size_t indexCount = sizeof(indices) / sizeof(indices[0]);



struct VertexTransforms {
    mat4x4 model;
    mat4x4 view;
    mat4x4 proj;
} mats;



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
        .apiVersion         = VK_API_VERSION_1_0,
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
        .ppEnabledLayerNames     = validationLayers,
#else
        .enabledLayerCount       = 0,
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
        .pfnCallback = vulkanDebugCallback,
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
            .pQueuePriorities = &queuePriority,
        },
        {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = vkData.presentFamily,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority,
        },
    };

    if (vkData.graphicsFamily == vkData.presentFamily)
        queueCreateInfoCount = 1;

    VkPhysicalDeviceFeatures deviceFeatures = {
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
        .ppEnabledLayerNames     = validationLayers,
#else
        .enabledLayerCount       = 0,
#endif // VALIDATION_LAYERS
    };

    VK_CHECK(vkCreateDevice(vkData.physicalDevice, &createInfo, NULL, &vkData.device));

    vkGetDeviceQueue(vkData.device, vkData.graphicsFamily, 0, &vkData.graphicsQueue);
    vkGetDeviceQueue(vkData.device, vkData.presentFamily, 0, &vkData.presentQueue);
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
        .oldSwapchain     = oldSwapchain,
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

    VkImageViewCreateInfo createInfo = {
        .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format   = vkData.swapchainImageFormat.format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
        },
    };

    for (size_t i = 0; i < vkData.swapchainImageCount; i++) {
        createInfo.image = vkData.swapchainImages[i];

        VK_CHECK(vkCreateImageView(vkData.device, &createInfo, NULL, &vkData.swapchainImageViews[i]));
    }
}

void createRenderPass()
{
    VkAttachmentDescription colorAttachment = {
        .format         = vkData.swapchainImageFormat.format,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference colorAttachmentRef = {
        .attachment = 0,
        .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &colorAttachmentRef,
    };

    VkSubpassDependency dependency = {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &colorAttachment,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dependency,
    };

    VK_CHECK(vkCreateRenderPass(vkData.device, &renderPassInfo, NULL, &vkData.renderPass));
}

VkShaderModule createShaderModule(char * code, size_t codeLen)
{
    VkShaderModuleCreateInfo createInfo = {
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = codeLen,
        .pCode    = (const uint32_t *) code,
    };

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(vkData.device, &createInfo, NULL, &shaderModule));

    return shaderModule;
}

void loadShaders()
{
    size_t vertCodeLen, fragCodeLen;
    char *vertShaderCode = getFile("shaders/vert.spv", &vertCodeLen);
    char *fragShaderCode = getFile("shaders/frag.spv", &fragCodeLen);

    shaders.vert = createShaderModule(vertShaderCode, vertCodeLen);
    shaders.frag = createShaderModule(fragShaderCode, fragCodeLen);

    free(vertShaderCode);
    free(fragShaderCode);
}

void createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding transformsLayoutBinding = {
        .binding            = 0,
        .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount    = 1,
        .stageFlags         = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = NULL, // Optional
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &transformsLayoutBinding,
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
        .pName  = "main",
    };

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
        .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = shaders.frag,
        .pName  = "main",
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input stuff
    VkVertexInputBindingDescription bindingDescription = {
        .binding   = 0,
        .stride    = sizeof(struct Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkVertexInputAttributeDescription attributeDescriptions[] = {
        {
            .binding  = 0,
            .location = 0,
            .format   = VK_FORMAT_R32G32_SFLOAT,
            .offset   = offsetof(struct Vertex, pos),
        },
        {
            .binding  = 0,
            .location = 1,
            .format   = VK_FORMAT_R32G32B32_SFLOAT,
            .offset   = offsetof(struct Vertex, color),
        },
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount   = 1,
        .pVertexBindingDescriptions      = &bindingDescription,
        .vertexAttributeDescriptionCount = sizeof(attributeDescriptions) / sizeof(attributeDescriptions[0]),
        .pVertexAttributeDescriptions    = attributeDescriptions,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    // Viewport stuff
    VkViewport viewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = (float) vkData.swapchainImageExtent.width,
        .height   = (float) vkData.swapchainImageExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = vkData.swapchainImageExtent,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports    = &viewport,
        .scissorCount  = 1,
        .pScissors     = &scissor,
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
        .depthBiasSlopeFactor    = 0.0f, // Optional
    };

    // Multisample stuff
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable   = VK_FALSE,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading      = 1.0f, // Optional
        .pSampleMask           = NULL, // Optional
        .alphaToCoverageEnable = VK_FALSE, // Optional
        .alphaToOneEnable      = VK_FALSE, // Optional
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
        .alphaBlendOp        = VK_BLEND_OP_ADD, // Optional
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY, // Optional
        .attachmentCount = 1,
        .pAttachments    = &colorBlendAttachment,
        .blendConstants  = { 0.0f, 0.0f, 0.0f, 0.0f }, // Optional
    };

    // Dynamic state stuff would go here if we had any

    // Pipeline layout for uniforms and stuff
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 1,
        .pSetLayouts            = &vkData.descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = NULL,
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
        .pDepthStencilState  = NULL,
        .pColorBlendState    = &colorBlending,
        .pDynamicState       = NULL,
        .layout              = vkData.pipelineLayout,
        .renderPass          = vkData.renderPass,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE, // Optional
        .basePipelineIndex   = -1, // Optional
    };

    VK_CHECK(vkCreateGraphicsPipelines(vkData.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL,
                                      &vkData.graphicsPipeline));
}

void createFramebuffers()
{
    vkData.swapchainFramebuffers = malloc(vkData.swapchainImageCount * sizeof(VkFramebuffer));

    for (size_t i = 0; i < vkData.swapchainImageCount; i++) {
        VkFramebufferCreateInfo framebufferInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = vkData.renderPass,
            .attachmentCount = 1,
            .pAttachments    = &vkData.swapchainImageViews[i],
            .width           = vkData.swapchainImageExtent.width,
            .height          = vkData.swapchainImageExtent.height,
            .layers          = 1,
        };

        VK_CHECK(vkCreateFramebuffer(vkData.device, &framebufferInfo, NULL,
                                     &vkData.swapchainFramebuffers[i]));
    }
}

void createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo = {
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = vkData.graphicsFamily,
        .flags            = 0, // Optional
    };

    VK_CHECK(vkCreateCommandPool(vkData.device, &poolInfo, NULL, &vkData.commandPool));
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

    createImage(vkData.physicalDevice, vkData.device, texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vkData.textureImage, &vkData.textureImageMemory);

    transitionImageLayout(vkData.device, vkData.commandPool, vkData.graphicsQueue, vkData.textureImage,
                          VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    copyBufferToImage(vkData.device, vkData.commandPool, vkData.graphicsQueue,
                      stagingBuffer, vkData.textureImage, texWidth, texHeight);

    transitionImageLayout(vkData.device, vkData.commandPool, vkData.graphicsQueue, vkData.textureImage,
                          VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(vkData.device, stagingBuffer, NULL);
    vkFreeMemory(vkData.device, stagingBufferMemory, NULL);
}

void createVertexBuffer()
{
    VkDeviceSize bufferSize = sizeof(vertices);

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

    copyBuffer(vkData.device, vkData.commandPool, vkData.graphicsQueue,
               stagingBuffer, vkData.vertexBuffer, bufferSize);

    vkDestroyBuffer(vkData.device, stagingBuffer, NULL);
    vkFreeMemory(vkData.device, stagingBufferMemory, NULL);
}

void createIndexBuffer()
{
    VkDeviceSize bufferSize = sizeof(indices);

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

    copyBuffer(vkData.device, vkData.commandPool, vkData.graphicsQueue,
               stagingBuffer, vkData.indexBuffer, bufferSize);

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
    VkDescriptorPoolSize poolSize = {
        .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes    = &poolSize,
        .maxSets       = 1,
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
        .pSetLayouts        = layouts,
    };

    VK_CHECK(vkAllocateDescriptorSets(vkData.device, &allocInfo, &vkData.descriptorSet));

    VkDescriptorBufferInfo bufferInfo = {
        .buffer = vkData.uniformBuffer,
        .offset = 0,
        .range  = sizeof(struct VertexTransforms),
    };

    VkWriteDescriptorSet descriptorWrite = {
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = vkData.descriptorSet,
        .dstBinding       = 0,
        .dstArrayElement  = 0,
        .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount  = 1,
        .pBufferInfo      = &bufferInfo,
        .pImageInfo       = NULL, // Optional
        .pTexelBufferView = NULL, // Optional
    };

    vkUpdateDescriptorSets(vkData.device, 1, &descriptorWrite, 0, NULL);
}

void createCommandBuffers()
{
    VkCommandBufferAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = vkData.commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = vkData.swapchainImageCount,
    };

    vkData.swapchainCommandBuffers = malloc(vkData.swapchainImageCount * sizeof(VkCommandBuffer));

    VK_CHECK(vkAllocateCommandBuffers(vkData.device, &allocInfo, vkData.swapchainCommandBuffers));

    for (size_t i = 0; i < vkData.swapchainImageCount; i++) {
        VkCommandBufferBeginInfo beginInfo = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags            = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            .pInheritanceInfo = NULL, // Optional
        };

        vkBeginCommandBuffer(vkData.swapchainCommandBuffers[i], &beginInfo);

        VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

        VkRenderPassBeginInfo renderPassInfo = {
            .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass  = vkData.renderPass,
            .framebuffer = vkData.swapchainFramebuffers[i],
            .renderArea = {
                .offset = {0, 0},
                .extent = vkData.swapchainImageExtent,
            },
            .clearValueCount = 1,
            .pClearValues    = &clearColor,
        };

        // Record draw commands into the command buffer
        vkCmdBeginRenderPass(vkData.swapchainCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(vkData.swapchainCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                          vkData.graphicsPipeline);

        VkBuffer vertexBuffers[] = {vkData.vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(vkData.swapchainCommandBuffers[i], 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(vkData.swapchainCommandBuffers[i], vkData.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

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
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
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

    // Swapchain things
    createSwapchain(VK_NULL_HANDLE);
    createImageViews();
    createRenderPass();

    loadShaders();
    createDescriptorSetLayout();

    // Swapchain things
    createGraphicsPipeline();
    createFramebuffers();

    createCommandPool();

    createTextureImage();

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

    createSwapchain(oldSwapchain);
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandBuffers();

    // TODO: Temporary update for projection matrix
    float aspect = vkData.swapchainImageExtent.width / (float) vkData.swapchainImageExtent.height;
    mat4x4_perspective(mats.proj, M_PI / 4, aspect, 0.1f, 10.0f);
    mats.proj[1][1] *= -1;
    // End TODO

    vkQueueWaitIdle(vkData.presentQueue);

    // Clean up old swapchain data
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

void windowResizeCallback(GLFWwindow *window, int width, int height)
{
    if (width == 0 || height == 0)
        return;
    windowWidth = width;
    windowHeight = height;
    //recreateSwapchain();
}

void initWindow()
{
    if (!glfwInit())
        ERR_EXIT("Error initializing GLFW\n");

    glfwSetErrorCallback(windowErrorCallback);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(windowWidth, windowHeight, "Vulkan Hello Triangle", NULL, NULL);
    if (!window)
        ERR_EXIT("Error creating GLFW window\n");

    glfwSetKeyCallback(window, windowKeyCallback);
    //glfwSetWindowSizeCallback(window, windowResizeCallback);
}



void initMats()
{
    mat4x4_identity(mats.model);

    vec3 eye = {2.0f, 2.0f, 2.0f}, center = {0.0f, 0.0f, 0.0f}, up = {0.0f, 0.0f, 1.0f};
    mat4x4_look_at(mats.view, eye, center, up);

    float aspect = vkData.swapchainImageExtent.width / (float) vkData.swapchainImageExtent.height;
    mat4x4_perspective(mats.proj, M_PI / 4, aspect, 0.1f, 10.0f);
    mats.proj[1][1] *= -1;
}



void updateUniformBuffer(double delta)
{
    mat4x4_rotate_Z(mats.model, mats.model, delta * (2 * M_PI) / 15.0f);

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
        .pSignalSemaphores    = &vkData.renderFinishedSemaphore,
    };

    VK_CHECK(vkQueueSubmit(vkData.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

    VkPresentInfoKHR presentInfo = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &vkData.renderFinishedSemaphore,
        .swapchainCount     = 1,
        .pSwapchains        = &vkData.swapchain,
        .pImageIndices      = &imageIndex,
        .pResults           = NULL, // Optional
    };

    result = vkQueuePresentKHR(vkData.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (result != VK_SUCCESS)
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
