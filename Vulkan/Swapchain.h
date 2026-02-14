#pragma once
#include "Core.h"

class Context; // forward declaration

class Swapchain {
public:
    Swapchain(Context& context, GLFWwindow* window);

    // Non-copyable, non-movable (owns raii resources)
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    void Recreate(Context& context, GLFWwindow* window);

    vk::Format GetFormat() const { return format_; }
    vk::Extent2D GetExtent() const { return extent_; }
    uint32_t GetImageCount() const { return static_cast<uint32_t>(images_.size()); }
    const std::vector<vk::raii::ImageView>& GetImageViews() const { return imageViews_; }
    const vk::raii::SwapchainKHR& GetHandle() const { return swapchain_; }

private:
    vk::raii::SwapchainKHR swapchain_ = nullptr;
    std::vector<vk::Image> images_;
    std::vector<vk::raii::ImageView> imageViews_;
    vk::Format format_;
    vk::Extent2D extent_;

    void create(Context& context, GLFWwindow* window);

    struct SwapchainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    SwapchainSupportDetails querySupport(vk::PhysicalDevice device, vk::SurfaceKHR surface) const;
    vk::SurfaceFormatKHR chooseFormat(const std::vector<vk::SurfaceFormatKHR>& formats) const;
    vk::PresentModeKHR choosePresentMode(const std::vector<vk::PresentModeKHR>& modes) const;
    vk::Extent2D chooseExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) const;
};
