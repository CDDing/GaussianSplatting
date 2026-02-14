#include "Buffer.h"
#include "Context.h"

Buffer::Buffer(Context& context, vk::BufferUsageFlags usage,
               vk::DeviceSize size, const void* data)
    : allocator_(context.GetAllocator())
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = static_cast<VkBufferUsageFlags>(usage);

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                      VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocInfoOut{};
    if (vmaCreateBuffer(allocator_, &bufferInfo, &allocInfo,
                        &buffer_, &allocation_, &allocInfoOut) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA buffer");
    }

    if (data && allocInfoOut.pMappedData) {
        memcpy(allocInfoOut.pMappedData, data, size);
    }
}

Buffer::~Buffer() {
    if (buffer_ && allocation_ && allocator_) {
        vmaDestroyBuffer(allocator_, buffer_, allocation_);
    }
}

Buffer::Buffer(Buffer&& other) noexcept
    : allocator_(other.allocator_)
    , buffer_(other.buffer_)
    , allocation_(other.allocation_)
{
    other.allocator_  = nullptr;
    other.buffer_     = VK_NULL_HANDLE;
    other.allocation_ = nullptr;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        // Destroy current resources
        if (buffer_ && allocation_ && allocator_) {
            vmaDestroyBuffer(allocator_, buffer_, allocation_);
        }

        // Transfer ownership
        allocator_  = other.allocator_;
        buffer_     = other.buffer_;
        allocation_ = other.allocation_;

        // Nullify source
        other.allocator_  = nullptr;
        other.buffer_     = VK_NULL_HANDLE;
        other.allocation_ = nullptr;
    }
    return *this;
}
