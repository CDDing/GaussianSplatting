#include "Context.h"

// VMA implementation - exactly once in the entire project
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

// ---------------------------------------------------------------------------
// Validation layers & device extensions (static in .cpp, NOT in header)
// ---------------------------------------------------------------------------

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

static const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// ---------------------------------------------------------------------------
// Debug callback
// ---------------------------------------------------------------------------

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    std::cerr << "[Validation] " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

Context::Context(GLFWwindow* window) {
    createInstance();
    setupDebugMessenger();
    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
    createAllocator();
}

Context::~Context() {
    if (*device_) {
        device_.waitIdle();
    }
    if (allocator_) {
        vmaDestroyAllocator(allocator_);
        allocator_ = nullptr;
    }
    // vk::raii handles destroy the rest in reverse declaration order
}

// ---------------------------------------------------------------------------
// createInstance
// ---------------------------------------------------------------------------

void Context::createInstance() {
    // Check validation layer support
    if (enableValidationLayers) {
        auto availableLayers = context_.enumerateInstanceLayerProperties();
        for (const char* layerName : validationLayers) {
            bool found = false;
            for (const auto& layerProps : availableLayers) {
                if (std::strcmp(layerName, layerProps.layerName) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error(
                    std::string("Validation layer not available: ") + layerName);
            }
        }
    }

    vk::ApplicationInfo appInfo{};
    appInfo.pApplicationName   = "GaussianSplatting";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    // Gather required extensions from GLFW
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    vk::InstanceCreateInfo createInfo{};
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    instance_ = vk::raii::Instance(context_, createInfo);
}

// ---------------------------------------------------------------------------
// setupDebugMessenger
// ---------------------------------------------------------------------------

void Context::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.messageSeverity =
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    createInfo.messageType =
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    createInfo.pfnUserCallback = reinterpret_cast<vk::PFN_DebugUtilsMessengerCallbackEXT>(debugCallback);

    debugMessenger_ = vk::raii::DebugUtilsMessengerEXT(instance_, createInfo);
}

// ---------------------------------------------------------------------------
// createSurface
// ---------------------------------------------------------------------------

void Context::createSurface(GLFWwindow* window) {
    VkSurfaceKHR rawSurface;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(*instance_), window, nullptr, &rawSurface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface");
    }
    surface_ = vk::raii::SurfaceKHR(instance_, rawSurface);
}

// ---------------------------------------------------------------------------
// pickPhysicalDevice
// ---------------------------------------------------------------------------

void Context::pickPhysicalDevice() {
    auto devices = instance_.enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    for (auto& device : devices) {
        if (isDeviceSuitable(*device)) {
            physical_ = std::move(device);
            return;
        }
    }

    throw std::runtime_error("Failed to find a suitable GPU");
}

// ---------------------------------------------------------------------------
// createLogicalDevice
// ---------------------------------------------------------------------------

void Context::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(*physical_);

    graphicsQueueFamily_ = indices.graphicsFamily.value();
    presentQueueFamily_  = indices.presentFamily.value();

    // Use a set to ensure unique queue families
    std::set<uint32_t> uniqueQueueFamilies = {
        graphicsQueueFamily_, presentQueueFamily_
    };

    float queuePriority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::PhysicalDeviceFeatures deviceFeatures{};

    vk::DeviceCreateInfo createInfo{};
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    device_ = vk::raii::Device(physical_, createInfo);

    graphicsQueue_ = (*device_).getQueue(graphicsQueueFamily_, 0);
    presentQueue_  = (*device_).getQueue(presentQueueFamily_, 0);
}

// ---------------------------------------------------------------------------
// createAllocator
// ---------------------------------------------------------------------------

void Context::createAllocator() {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = static_cast<VkPhysicalDevice>(*physical_);
    allocatorInfo.device         = static_cast<VkDevice>(*device_);
    allocatorInfo.instance       = static_cast<VkInstance>(*instance_);

    if (vmaCreateAllocator(&allocatorInfo, &allocator_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator");
    }
}

// ---------------------------------------------------------------------------
// findQueueFamilies
// ---------------------------------------------------------------------------

Context::QueueFamilyIndices Context::findQueueFamilies(vk::PhysicalDevice device) const {
    QueueFamilyIndices indices;

    auto queueFamilies = device.getQueueFamilyProperties();

    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilies.size()); i++) {
        // Use bitwise AND (&), not logical AND (&&) for flag check
        if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        // Use the C API through the raw handle for present support query
        vkGetPhysicalDeviceSurfaceSupportKHR(
            static_cast<VkPhysicalDevice>(device),
            i,
            static_cast<VkSurfaceKHR>(*surface_),
            &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) break;
    }

    return indices;
}

// ---------------------------------------------------------------------------
// isDeviceSuitable
// ---------------------------------------------------------------------------

bool Context::isDeviceSuitable(vk::PhysicalDevice device) const {
    QueueFamilyIndices indices = findQueueFamilies(device);

    // Check device extension support
    auto availableExtensions = device.enumerateDeviceExtensionProperties();
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return indices.isComplete() && requiredExtensions.empty();
}
