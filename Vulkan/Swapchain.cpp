#include "Swapchain.h"
#include "Context.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

Swapchain::Swapchain(Context& context, GLFWwindow* window) {
    create(context, window);
}

// ---------------------------------------------------------------------------
// Recreate (called on window resize)
// ---------------------------------------------------------------------------

void Swapchain::Recreate(Context& context, GLFWwindow* window) {
    context.Device().waitIdle();
    imageViews_.clear();
    images_.clear();
    swapchain_ = nullptr;
    create(context, window);
}

// ---------------------------------------------------------------------------
// create
// ---------------------------------------------------------------------------

void Swapchain::create(Context& context, GLFWwindow* window) {
    SwapchainSupportDetails support = querySupport(*context.PhysicalDevice(), context.GetSurface());

    vk::SurfaceFormatKHR surfaceFormat = chooseFormat(support.formats);
    vk::PresentModeKHR presentMode     = choosePresentMode(support.presentModes);
    vk::Extent2D extent                = chooseExtent(support.capabilities, window);

    // Request one more image than the minimum to avoid waiting on the driver
    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.surface          = context.GetSurface();
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = vk::ImageUsageFlagBits::eColorAttachment;

    uint32_t graphicsFamily = context.GetGraphicsQueueFamily();
    uint32_t presentFamily  = context.GetPresentQueueFamily();

    if (graphicsFamily != presentFamily) {
        uint32_t queueFamilyIndices[] = { graphicsFamily, presentFamily };
        createInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode      = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices   = nullptr;
    }

    createInfo.preTransform   = support.capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode    = presentMode;
    createInfo.clipped        = VK_TRUE;
    createInfo.oldSwapchain   = nullptr;

    swapchain_ = vk::raii::SwapchainKHR(context.Device(), createInfo);

    // Store format and extent for later use
    format_ = surfaceFormat.format;
    extent_ = extent;

    // Retrieve swapchain images
    images_ = swapchain_.getImages();

    // Create image views
    imageViews_.clear();
    imageViews_.reserve(images_.size());

    for (const auto& image : images_) {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.image    = image;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.format   = format_;

        viewInfo.components.r = vk::ComponentSwizzle::eIdentity;
        viewInfo.components.g = vk::ComponentSwizzle::eIdentity;
        viewInfo.components.b = vk::ComponentSwizzle::eIdentity;
        viewInfo.components.a = vk::ComponentSwizzle::eIdentity;

        viewInfo.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        imageViews_.emplace_back(context.Device(), viewInfo);
    }
}

// ---------------------------------------------------------------------------
// querySupport
// ---------------------------------------------------------------------------

Swapchain::SwapchainSupportDetails Swapchain::querySupport(
    vk::PhysicalDevice device, vk::SurfaceKHR surface) const
{
    SwapchainSupportDetails details;
    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats      = device.getSurfaceFormatsKHR(surface);
    details.presentModes = device.getSurfacePresentModesKHR(surface);
    return details;
}

// ---------------------------------------------------------------------------
// chooseFormat - prefer B8G8R8A8_SRGB + SRGB_NONLINEAR
// ---------------------------------------------------------------------------

vk::SurfaceFormatKHR Swapchain::chooseFormat(
    const std::vector<vk::SurfaceFormatKHR>& formats) const
{
    for (const auto& format : formats) {
        if (format.format == vk::Format::eB8G8R8A8Srgb &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            return format;
        }
    }
    // Fallback to the first available format
    return formats[0];
}

// ---------------------------------------------------------------------------
// choosePresentMode - prefer Mailbox, fallback to FIFO
// ---------------------------------------------------------------------------

vk::PresentModeKHR Swapchain::choosePresentMode(
    const std::vector<vk::PresentModeKHR>& modes) const
{
    for (const auto& mode : modes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            return mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

// ---------------------------------------------------------------------------
// chooseExtent - handle GLFW framebuffer size
// ---------------------------------------------------------------------------

vk::Extent2D Swapchain::chooseExtent(
    const vk::SurfaceCapabilitiesKHR& capabilities,
    GLFWwindow* window) const
{
    // If currentExtent is not the special value, use it directly
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    // Otherwise, query GLFW for the actual framebuffer size
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    vk::Extent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(
        actualExtent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);

    actualExtent.height = std::clamp(
        actualExtent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    return actualExtent;
}
