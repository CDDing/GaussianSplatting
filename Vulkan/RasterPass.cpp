#include "RasterPass.h"
#include "Context.h"

RasterPass::RasterPass(Context& context, const std::string& shaderPath)
    : pipeline_(context, shaderPath, {}) {
}

void RasterPass::Record(vk::CommandBuffer /*cmd*/) {
    // TODO: per-tile rasterization (alpha-blended splats → framebuffer)
}
