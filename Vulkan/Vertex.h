#pragma once
#include "Core.h"

struct Vertex {
    glm::vec2 position;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        return {{
            {0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, position)},
            {1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)}
        }};
    }
};
