#include "Renderer.h"
#include "Buffer.h"
#include "Context.h"
#include "Swapchain.h"
#include "Pipeline.h"
#include "ComputePass.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

Renderer::Renderer(Context& context, Swapchain& swapchain,
                   Pipeline& pipeline, CommandManager& commands) {
    createFramebuffers(context, swapchain, pipeline);
    createSyncObjects(context, swapchain.GetImageCount());
}

// ---------------------------------------------------------------------------
// createFramebuffers
// ---------------------------------------------------------------------------

void Renderer::createFramebuffers(Context& context, Swapchain& swapchain,
                                  Pipeline& pipeline) {
    const auto& imageViews = swapchain.GetImageViews();
    framebuffers_.reserve(imageViews.size());

    for (const auto& imageView : imageViews) {
        vk::ImageView attachments[] = { *imageView };

        vk::FramebufferCreateInfo framebufferInfo{};
        framebufferInfo.setRenderPass(pipeline.GetRenderPass());
        framebufferInfo.setAttachments(attachments);
        framebufferInfo.setWidth(swapchain.GetExtent().width);
        framebufferInfo.setHeight(swapchain.GetExtent().height);
        framebufferInfo.setLayers(1);

        framebuffers_.emplace_back(context.Device(), framebufferInfo);
    }
}

// ---------------------------------------------------------------------------
// createSyncObjects
// ---------------------------------------------------------------------------

void Renderer::createSyncObjects(Context& context, uint32_t swapchainImageCount) {
    vk::SemaphoreCreateInfo semaphoreInfo{};
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

    imageAvailable_.reserve(FRAMES_IN_FLIGHT);
    inFlight_.reserve(FRAMES_IN_FLIGHT);

    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        imageAvailable_.push_back(context.Device().createSemaphore(semaphoreInfo));
        inFlight_.push_back(context.Device().createFence(fenceInfo));
    }

    // Per-swapchain-image semaphores to avoid reuse while presentation is pending
    renderFinished_.reserve(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        renderFinished_.push_back(context.Device().createSemaphore(semaphoreInfo));
    }
}

// ---------------------------------------------------------------------------
// recordCommandBuffer
// ---------------------------------------------------------------------------

void Renderer::recordCommandBuffer(vk::CommandBuffer cmd, uint32_t imageIndex,
                                   Swapchain& swapchain, Pipeline& pipeline,
                                   Buffer* uboStaging, Buffer* uboDevice,
                                   ComputePass* projPass, ComputePass* sortPass,
                                   ComputePass* rasterPass) {
    vk::CommandBufferBeginInfo beginInfo{};
    cmd.begin(beginInfo);

    // Staging → Device UBO copy
    if (uboStaging && uboDevice) {
        uboStaging->RecordCopy(cmd, *uboDevice);
    }

    // ─── Compute passes ───
    if (projPass)   projPass->Record(cmd);
    if (sortPass)   sortPass->Record(cmd);
    if (rasterPass) rasterPass->Record(cmd);

    // ─── Render pass ───
    vk::ClearValue clearColor{vk::ClearColorValue{std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}}};

    vk::RenderPassBeginInfo renderPassInfo{};
    renderPassInfo.setRenderPass(pipeline.GetRenderPass());
    renderPassInfo.setFramebuffer(*framebuffers_[imageIndex]);
    renderPassInfo.renderArea.offset = vk::Offset2D{0, 0};
    renderPassInfo.renderArea.extent = swapchain.GetExtent();
    renderPassInfo.setClearValues(clearColor);

    cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    // TODO: fullscreen quad (graphics pipeline + draw call)
    cmd.endRenderPass();
    cmd.end();
}

// ---------------------------------------------------------------------------
// DrawFrame
// ---------------------------------------------------------------------------

void Renderer::WaitForCurrentFrame(Context& context) {
    context.Device().waitForFences(
        *inFlight_[currentFrame_], VK_TRUE, UINT64_MAX);
}

bool Renderer::DrawFrame(Context& context, Swapchain& swapchain,
                         Pipeline& pipeline, CommandManager& commands,
                         Buffer* uboStaging, Buffer* uboDevice,
                         ComputePass* projPass, ComputePass* sortPass,
                         ComputePass* rasterPass) {
    // Fence already waited by WaitForCurrentFrame() before UBO upload

    // Acquire next swapchain image
    auto [result, imageIndex] = swapchain.GetHandle().acquireNextImage(
        UINT64_MAX, *imageAvailable_[currentFrame_]);

    if (result == vk::Result::eErrorOutOfDateKHR) {
        return true; // Need swapchain recreation
    }

    context.Device().resetFences(*inFlight_[currentFrame_]);

    // Record command buffer
    auto& cmdBuffers = commands.GetCommandBuffers();
    cmdBuffers[currentFrame_].reset();
    recordCommandBuffer(*cmdBuffers[currentFrame_], imageIndex,
                        swapchain, pipeline,
                        uboStaging, uboDevice,
                        projPass, sortPass, rasterPass);

    // Submit
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    vk::SubmitInfo submitInfo{};
    submitInfo.setWaitSemaphores(*imageAvailable_[currentFrame_]);
    submitInfo.setWaitDstStageMask(waitStage);
    submitInfo.setCommandBuffers(*cmdBuffers[currentFrame_]);
    submitInfo.setSignalSemaphores(*renderFinished_[imageIndex]);

    context.GetGraphicsQueue().submit(submitInfo, *inFlight_[currentFrame_]);

    // Present
    vk::SwapchainKHR swapchains[] = { *swapchain.GetHandle() };

    vk::PresentInfoKHR presentInfo{};
    presentInfo.setWaitSemaphores(*renderFinished_[imageIndex]);
    presentInfo.setSwapchainCount(1);
    presentInfo.setPSwapchains(swapchains);
    presentInfo.setImageIndices(imageIndex);

    try {
        auto presentResult = context.GetPresentQueue().presentKHR(presentInfo);
        if (presentResult == vk::Result::eSuboptimalKHR) {
            currentFrame_ = (currentFrame_ + 1) % FRAMES_IN_FLIGHT;
            return true;
        }
    } catch (const vk::OutOfDateKHRError&) {
        currentFrame_ = (currentFrame_ + 1) % FRAMES_IN_FLIGHT;
        return true;
    }

    currentFrame_ = (currentFrame_ + 1) % FRAMES_IN_FLIGHT;
    return false;
}

// ---------------------------------------------------------------------------
// RecreateFramebuffers
// ---------------------------------------------------------------------------

void Renderer::RecreateFramebuffers(Context& context, Swapchain& swapchain,
                                    Pipeline& pipeline) {
    framebuffers_.clear();
    createFramebuffers(context, swapchain, pipeline);

    // Recreate per-image semaphores (image count may change)
    vk::SemaphoreCreateInfo semaphoreInfo{};
    renderFinished_.clear();
    renderFinished_.reserve(swapchain.GetImageCount());
    for (uint32_t i = 0; i < swapchain.GetImageCount(); i++) {
        renderFinished_.push_back(context.Device().createSemaphore(semaphoreInfo));
    }
}
