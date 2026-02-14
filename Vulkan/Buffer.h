#pragma once
#include "Core.h"

class Context;

class Buffer {
public:
    // Creates a host-visible, mapped buffer and copies data into it
    Buffer(Context& context, vk::BufferUsageFlags usage,
           vk::DeviceSize size, const void* data = nullptr);
    ~Buffer();

    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    vk::Buffer GetHandle() const { return buffer_; }

private:
    VmaAllocator allocator_ = nullptr;
    VkBuffer buffer_        = VK_NULL_HANDLE;
    VmaAllocation allocation_ = nullptr;
};
