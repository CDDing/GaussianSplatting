#pragma once
#include "Core.h"

class Context;

class ComputePipeline {
public:
    ComputePipeline(Context& context, const std::string& shaderPath,
                    const std::vector<vk::DescriptorSetLayoutBinding>& bindings,
                    uint32_t pushConstantSize = 0);

    vk::Pipeline GetHandle() const { return *pipeline_; }
    vk::PipelineLayout GetLayout() const { return *layout_; }
    vk::DescriptorSetLayout GetDescriptorSetLayout() const { return *descriptorSetLayout_; }

private:
    vk::raii::DescriptorSetLayout descriptorSetLayout_ = nullptr;
    vk::raii::PipelineLayout layout_                   = nullptr;
    vk::raii::Pipeline pipeline_                       = nullptr;

    static std::vector<uint32_t> loadShader(const std::string& path);
};
