#pragma once
#include "Core.h"

class Context;
class Swapchain;

class Pipeline {
public:
    Pipeline(Context& context, const Swapchain& swapchain);

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;

    vk::RenderPass GetRenderPass() const { return *renderPass_; }
    vk::Pipeline GetHandle() const { return *pipeline_; }
    vk::PipelineLayout GetLayout() const { return *layout_; }

private:
    vk::raii::RenderPass renderPass_     = nullptr;
    vk::raii::PipelineLayout layout_     = nullptr;
    vk::raii::Pipeline pipeline_         = nullptr;

    void createRenderPass(Context& context, const Swapchain& swapchain);
    void createPipeline(Context& context, const Swapchain& swapchain);

    static std::vector<uint32_t> loadShader(const std::string& path);
};
