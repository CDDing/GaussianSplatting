#pragma once
#include "Core.h"

class Context {
public:
    Context(GLFWwindow* window);
    ~Context();

    // Non-copyable, non-movable
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    vk::raii::Device& Device() { return device_; }
    vk::raii::PhysicalDevice& PhysicalDevice() { return physical_; }
    vk::raii::Instance& Instance() { return instance_; }
    vk::Queue GetGraphicsQueue() const { return graphicsQueue_; }
    vk::Queue GetPresentQueue() const { return presentQueue_; }
    uint32_t GetGraphicsQueueFamily() const { return graphicsQueueFamily_; }
    uint32_t GetPresentQueueFamily() const { return presentQueueFamily_; }
    VmaAllocator GetAllocator() const { return allocator_; }
    vk::SurfaceKHR GetSurface() const { return *surface_; }

private:
    // Declaration order = reverse destruction order
    vk::raii::Context context_;
    vk::raii::Instance instance_                     = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger_ = nullptr;
    vk::raii::SurfaceKHR surface_                    = nullptr;
    vk::raii::PhysicalDevice physical_               = nullptr;
    vk::raii::Device device_                         = nullptr;
    VmaAllocator allocator_                          = nullptr;

    vk::Queue graphicsQueue_;
    vk::Queue presentQueue_;
    uint32_t graphicsQueueFamily_ = 0;
    uint32_t presentQueueFamily_  = 0;

    void createInstance();
    void setupDebugMessenger();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createAllocator();

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device) const;
    bool isDeviceSuitable(vk::PhysicalDevice device) const;
};
