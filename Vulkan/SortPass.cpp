#include "SortPass.h"
#include "Context.h"

SortPass::SortPass(Context& context, const std::string& shaderPath)
    : pipeline_(context, shaderPath, {}) {
}

void SortPass::Record(vk::CommandBuffer /*cmd*/) {
    // TODO: radix sort by (tile_id, depth)
}
