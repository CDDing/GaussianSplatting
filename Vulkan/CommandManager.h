#pragma once
#include "Core.h"

class Context;

class CommandManager {
public:
    static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

    CommandManager(Context& context);

    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;

    const std::vector<vk::raii::CommandBuffer>& GetCommandBuffers() const { return commandBuffers_; }

    void ImmediateSubmit(Context& context,
                         std::function<void(vk::CommandBuffer)>&& fn);

private:
    vk::raii::CommandPool pool_ = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers_;

    // For ImmediateSubmit
    vk::raii::CommandPool immediatePool_      = nullptr;
    vk::raii::CommandBuffer immediateBuffer_  = nullptr;
    vk::raii::Fence immediateFence_           = nullptr;
};
