#include "CommandManager.h"
#include "Context.h"

CommandManager::CommandManager(Context& context) {
    // Main command pool with reset flag
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    poolInfo.setQueueFamilyIndex(context.GetGraphicsQueueFamily());
    pool_ = context.Device().createCommandPool(poolInfo);

    // Allocate FRAMES_IN_FLIGHT command buffers
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandPool(*pool_);
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    allocInfo.setCommandBufferCount(FRAMES_IN_FLIGHT);
    commandBuffers_ = context.Device().allocateCommandBuffers(allocInfo);

    // Immediate submit resources
    vk::CommandPoolCreateInfo immPoolInfo{};
    immPoolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    immPoolInfo.setQueueFamilyIndex(context.GetGraphicsQueueFamily());
    immediatePool_ = context.Device().createCommandPool(immPoolInfo);

    vk::CommandBufferAllocateInfo immAllocInfo{};
    immAllocInfo.setCommandPool(*immediatePool_);
    immAllocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    immAllocInfo.setCommandBufferCount(1);
    immediateBuffer_ = std::move(context.Device().allocateCommandBuffers(immAllocInfo).front());

    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
    immediateFence_ = context.Device().createFence(fenceInfo);
}

void CommandManager::ImmediateSubmit(Context& context,
                                     std::function<void(vk::CommandBuffer)>&& fn) {
    auto result = context.Device().waitForFences(*immediateFence_, VK_TRUE, UINT64_MAX);
    context.Device().resetFences(*immediateFence_);
    immediateBuffer_.reset({});

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    immediateBuffer_.begin(beginInfo);

    fn(*immediateBuffer_);

    immediateBuffer_.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(*immediateBuffer_);
    context.GetGraphicsQueue().submit(submitInfo, *immediateFence_);

    result = context.Device().waitForFences(*immediateFence_, VK_TRUE, UINT64_MAX);
}
