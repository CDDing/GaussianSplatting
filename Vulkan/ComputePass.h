#pragma once
#include "Core.h"

class ComputePass {
public:
    virtual ~ComputePass() = default;
    virtual void Record(vk::CommandBuffer cmd) = 0;
};
