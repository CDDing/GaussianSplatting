#pragma once
#include "Core.h"
#include "ComputePass.h"
#include "ComputePipeline.h"

class Context;

class ProjectionPass : public ComputePass {
public:
    struct Buffers {
        vk::Buffer positions;     // SSBO binding 1
        vk::Buffer sh;            // SSBO binding 2
        vk::Buffer opacity;       // SSBO binding 3
        vk::Buffer scale;         // SSBO binding 4
        vk::Buffer rotation;      // SSBO binding 5
        vk::Buffer projected2D;   // SSBO binding 6 (output)
        vk::Buffer visibility;    // SSBO binding 7 (output)
        vk::Buffer tileCount;     // SSBO binding 8 (output)
    };

    struct BufferSizes {
        vk::DeviceSize positions;
        vk::DeviceSize sh;
        vk::DeviceSize opacity;
        vk::DeviceSize scale;
        vk::DeviceSize rotation;
        vk::DeviceSize projected2D;
        vk::DeviceSize visibility;
        vk::DeviceSize tileCount;
    };

    struct PushConstants {
        uint32_t gaussianCount;
        uint32_t tileWidth;
        uint32_t tileHeight;
    };

    ProjectionPass(Context& context, const std::string& shaderPath,
                   uint32_t framesInFlight);

    void UpdateDescriptors(Context& context, uint32_t frameIndex,
                           vk::Buffer cameraUbo, vk::DeviceSize uboSize,
                           const Buffers& buffers, const BufferSizes& sizes);

    void SetFrameIndex(uint32_t frameIndex) { currentFrame_ = frameIndex; }
    void SetPushConstants(const PushConstants& pc) { pushConstants_ = pc; }
    void Record(vk::CommandBuffer cmd) override;

private:
    ComputePipeline pipeline_;
    vk::raii::DescriptorPool descriptorPool_          = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets_;
    uint32_t currentFrame_ = 0;
    PushConstants pushConstants_{};
};
