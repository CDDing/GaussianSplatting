#include "Pipeline.h"
#include "Context.h"
#include "Swapchain.h"
#include "Vertex.h"

Pipeline::Pipeline(Context& context, const Swapchain& swapchain) {
    createRenderPass(context, swapchain);
    createPipeline(context, swapchain);
}

std::vector<uint32_t> Pipeline::loadShader(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));
    return buffer;
}

void Pipeline::createRenderPass(Context& context, const Swapchain& swapchain) {
    // Color attachment
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.setFormat(swapchain.GetFormat());
    colorAttachment.setSamples(vk::SampleCountFlagBits::e1);
    colorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colorAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colorAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    colorAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    // Color attachment reference
    vk::AttachmentReference colorAttachmentRef{};
    colorAttachmentRef.setAttachment(0);
    colorAttachmentRef.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    // Subpass
    vk::SubpassDescription subpass{};
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    subpass.setColorAttachments(colorAttachmentRef);

    // Subpass dependency
    vk::SubpassDependency dependency{};
    dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
    dependency.setDstSubpass(0);
    dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    dependency.setSrcAccessMask({});
    dependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

    // Create render pass
    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.setAttachments(colorAttachment);
    renderPassInfo.setSubpasses(subpass);
    renderPassInfo.setDependencies(dependency);

    renderPass_ = context.Device().createRenderPass(renderPassInfo);
}

void Pipeline::createPipeline(Context& context, const Swapchain& swapchain) {
    // Pipeline layout (no push constants, no descriptor set layouts)
    vk::PipelineLayoutCreateInfo layoutInfo{};
    layout_ = context.Device().createPipelineLayout(layoutInfo);

    // Graphics pipeline 생성 생략: compute-only 렌더링으로 전환 중.
    // Render pass는 swapchain clear용으로 유지, pipeline은 추후 fullscreen quad 추가 시 생성.
}
