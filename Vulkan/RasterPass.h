#pragma once
#include "ComputePass.h"
#include "ComputePipeline.h"

class Context;

class RasterPass : public ComputePass {
public:
    RasterPass(Context& context, const std::string& shaderPath);
    void Record(vk::CommandBuffer cmd) override;

private:
    ComputePipeline pipeline_;
};
