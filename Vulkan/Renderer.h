#pragma once
#include "Core.h"
#include "CommandManager.h"

class Context;
class Swapchain;
class Pipeline;

class Renderer {
public:
    Renderer(Context& context, Swapchain& swapchain,
             Pipeline& pipeline, CommandManager& commands);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Returns true if swapchain needs recreation
    bool DrawFrame(Context& context, Swapchain& swapchain,
                   Pipeline& pipeline, CommandManager& commands,
                   vk::Buffer vertexBuffer, uint32_t vertexCount);

    void RecreateFramebuffers(Context& context, Swapchain& swapchain,
                              Pipeline& pipeline);

private:
    static constexpr uint32_t FRAMES_IN_FLIGHT = CommandManager::FRAMES_IN_FLIGHT;

    std::vector<vk::raii::Framebuffer> framebuffers_;

    std::vector<vk::raii::Semaphore> imageAvailable_;
    std::vector<vk::raii::Semaphore> renderFinished_;
    std::vector<vk::raii::Fence> inFlight_;
    uint32_t currentFrame_ = 0;

    void createFramebuffers(Context& context, Swapchain& swapchain, Pipeline& pipeline);
    void createSyncObjects(Context& context, uint32_t swapchainImageCount);
    void recordCommandBuffer(vk::CommandBuffer cmd, uint32_t imageIndex,
                             Swapchain& swapchain, Pipeline& pipeline,
                             vk::Buffer vertexBuffer, uint32_t vertexCount);
};
