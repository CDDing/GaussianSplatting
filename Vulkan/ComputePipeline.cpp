#include "ComputePipeline.h"
#include "Context.h"

ComputePipeline::ComputePipeline(Context& context, const std::string& shaderPath,
                                 const std::vector<vk::DescriptorSetLayoutBinding>& bindings,
                                 uint32_t pushConstantSize) {
    // Descriptor set layout
    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.setBindings(bindings);
    descriptorSetLayout_ = context.Device().createDescriptorSetLayout(layoutInfo);

    // Pipeline layout (with optional push constants)
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setSetLayouts(*descriptorSetLayout_);

    vk::PushConstantRange pushRange{};
    if (pushConstantSize > 0) {
        pushRange.stageFlags = vk::ShaderStageFlagBits::eCompute;
        pushRange.offset     = 0;
        pushRange.size       = pushConstantSize;
        pipelineLayoutInfo.setPushConstantRanges(pushRange);
    }
    layout_ = context.Device().createPipelineLayout(pipelineLayoutInfo);

    // Shader module
    auto code = loadShader(shaderPath);
    vk::ShaderModuleCreateInfo moduleInfo{};
    moduleInfo.setCode(code);
    vk::raii::ShaderModule shaderModule = context.Device().createShaderModule(moduleInfo);

    // Compute pipeline
    vk::PipelineShaderStageCreateInfo stageInfo{};
    stageInfo.setStage(vk::ShaderStageFlagBits::eCompute);
    stageInfo.setModule(*shaderModule);
    stageInfo.setPName("main");

    vk::ComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.setStage(stageInfo);
    pipelineInfo.setLayout(*layout_);

    pipeline_ = context.Device().createComputePipeline(nullptr, pipelineInfo);
}

std::vector<uint32_t> ComputePipeline::loadShader(const std::string& path) {
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
