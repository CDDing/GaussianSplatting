#include "ProjectionPass.h"
#include "Context.h"

ProjectionPass::ProjectionPass(Context& context, const std::string& shaderPath,
                               uint32_t framesInFlight)
    : pipeline_(context, shaderPath,
                // 9 bindings: 1 UBO + 8 SSBOs
                std::vector<vk::DescriptorSetLayoutBinding>{
                    {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute},
                    {1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
                    {2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
                    {3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
                    {4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
                    {5, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
                    {6, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
                    {7, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
                    {8, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute},
                },
                sizeof(PushConstants))
{
    // Descriptor pool: 1 UBO + 8 SSBOs per set × framesInFlight sets
    std::array<vk::DescriptorPoolSize, 2> poolSizes = {
        vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, framesInFlight},
        vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, framesInFlight * 8}
    };

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    poolInfo.setMaxSets(framesInFlight);
    poolInfo.setPoolSizes(poolSizes);
    descriptorPool_ = context.Device().createDescriptorPool(poolInfo);

    // Allocate descriptor sets
    std::vector<vk::DescriptorSetLayout> layouts(framesInFlight,
                                                  pipeline_.GetDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(*descriptorPool_);
    allocInfo.setSetLayouts(layouts);
    descriptorSets_ = context.Device().allocateDescriptorSets(allocInfo);
}

void ProjectionPass::UpdateDescriptors(Context& context, uint32_t frameIndex,
                                       vk::Buffer cameraUbo, vk::DeviceSize uboSize,
                                       const Buffers& buffers,
                                       const BufferSizes& sizes) {
    std::array<vk::DescriptorBufferInfo, 9> bufferInfos = {{
        {cameraUbo,          0, uboSize},
        {buffers.positions,  0, sizes.positions},
        {buffers.sh,         0, sizes.sh},
        {buffers.opacity,    0, sizes.opacity},
        {buffers.scale,      0, sizes.scale},
        {buffers.rotation,   0, sizes.rotation},
        {buffers.projected2D,0, sizes.projected2D},
        {buffers.visibility, 0, sizes.visibility},
        {buffers.tileCount,  0, sizes.tileCount},
    }};

    std::array<vk::WriteDescriptorSet, 9> writes{};
    for (uint32_t i = 0; i < 9; i++) {
        writes[i].setDstSet(*descriptorSets_[frameIndex]);
        writes[i].setDstBinding(i);
        writes[i].setDescriptorType(i == 0 ? vk::DescriptorType::eUniformBuffer
                                           : vk::DescriptorType::eStorageBuffer);
        writes[i].setBufferInfo(bufferInfos[i]);
    }

    context.Device().updateDescriptorSets(writes, {});
}

void ProjectionPass::Record(vk::CommandBuffer cmd) {
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline_.GetHandle());
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                           pipeline_.GetLayout(), 0,
                           *descriptorSets_[currentFrame_], {});
    cmd.pushConstants(pipeline_.GetLayout(),
                      vk::ShaderStageFlagBits::eCompute,
                      0, sizeof(PushConstants), &pushConstants_);

    uint32_t groupCount = (pushConstants_.gaussianCount + 255) / 256;
    cmd.dispatch(groupCount, 1, 1);

    // Compute → Compute 배리어 (후속 sort pass 대비)
    vk::MemoryBarrier barrier{};
    barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eComputeShader,
        vk::PipelineStageFlagBits::eComputeShader,
        {}, barrier, {}, {}
    );
}
