#pragma once
#include "ComputePass.h"
#include "ComputePipeline.h"

class Context;

class SortPass : public ComputePass {
public:
    SortPass(Context& context, const std::string& shaderPath);
    void Record(vk::CommandBuffer cmd) override;

private:
    ComputePipeline pipeline_;
};
