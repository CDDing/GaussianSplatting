#include "Buffer.h"
#include "Context.h"

// One-shot copy command for staging → device-local transfer
static void copyBuffer(Context& context, VkBuffer src, VkBuffer dst, vk::DeviceSize size)
{
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags            = vk::CommandPoolCreateFlagBits::eTransient;
    poolInfo.queueFamilyIndex = context.GetGraphicsQueueFamily();

    vk::raii::CommandPool pool(context.Device(), poolInfo);

    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool        = *pool;
    allocInfo.level              = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 1;

    auto cmdBuffers = context.Device().allocateCommandBuffers(allocInfo);
    auto& cmd       = cmdBuffers[0];

    cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    cmd.copyBuffer(src, dst, vk::BufferCopy{0, 0, size});
    cmd.end();

    vk::CommandBuffer rawCmd = *cmd;
    vk::SubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &rawCmd;

    context.GetGraphicsQueue().submit(submitInfo);
    context.GetGraphicsQueue().waitIdle();
}

Buffer Buffer::CreateDeviceLocal(Context& context, vk::BufferUsageFlags usage,
                                 vk::DeviceSize size, const void* data) {
    Buffer buf;
    buf.allocator_ = context.GetAllocator();
    buf.size_ = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = size;
    bufferInfo.usage = static_cast<VkBufferUsageFlags>(usage);

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (data) {
        // Device-local buffer (transfer destination)
        bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        allocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (vmaCreateBuffer(buf.allocator_, &bufferInfo, &allocInfo,
                            &buf.buffer_, &buf.allocation_, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create device-local buffer");
        }

        // Staging buffer (host-visible, coherent)
        VkBuffer stagingBuffer           = VK_NULL_HANDLE;
        VmaAllocation stagingAllocation  = nullptr;

        VkBufferCreateInfo stagingInfo{};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size  = size;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage         = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags         = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                         VMA_ALLOCATION_CREATE_MAPPED_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        VmaAllocationInfo stagingAllocInfoOut{};
        if (vmaCreateBuffer(buf.allocator_, &stagingInfo, &stagingAllocInfo,
                            &stagingBuffer, &stagingAllocation, &stagingAllocInfoOut) != VK_SUCCESS) {
            vmaDestroyBuffer(buf.allocator_, buf.buffer_, buf.allocation_);
            buf.buffer_     = VK_NULL_HANDLE;
            buf.allocation_ = nullptr;
            throw std::runtime_error("Failed to create staging buffer");
        }

        memcpy(stagingAllocInfoOut.pMappedData, data, size);

        try {
            copyBuffer(context, stagingBuffer, buf.buffer_, size);
        } catch (...) {
            vmaDestroyBuffer(buf.allocator_, stagingBuffer, stagingAllocation);
            vmaDestroyBuffer(buf.allocator_, buf.buffer_, buf.allocation_);
            buf.buffer_     = VK_NULL_HANDLE;
            buf.allocation_ = nullptr;
            throw;
        }
        vmaDestroyBuffer(buf.allocator_, stagingBuffer, stagingAllocation);
    } else {
        // Device-local buffer without initial data
        allocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (vmaCreateBuffer(buf.allocator_, &bufferInfo, &allocInfo,
                            &buf.buffer_, &buf.allocation_, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create device-local buffer");
        }
    }

    return buf;
}

Buffer Buffer::CreateHostVisible(Context& context, vk::BufferUsageFlags usage,
                                 vk::DeviceSize size) {
    Buffer buf;
    buf.allocator_ = context.GetAllocator();
    buf.size_ = size;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size  = size;
    bufferInfo.usage = static_cast<VkBufferUsageFlags>(usage);

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage         = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags         = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                              VMA_ALLOCATION_CREATE_MAPPED_BIT;
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    VmaAllocationInfo allocInfoOut{};
    if (vmaCreateBuffer(buf.allocator_, &bufferInfo, &allocInfo,
                        &buf.buffer_, &buf.allocation_, &allocInfoOut) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create host-visible buffer");
    }

    buf.mappedData_ = allocInfoOut.pMappedData;
    return buf;
}

void Buffer::Upload(const void* data, vk::DeviceSize size) {
    if (!mappedData_) {
        throw std::runtime_error("Upload called on non-mapped buffer");
    }
    if (size > size_) {
        throw std::runtime_error("Upload size exceeds buffer size");
    }
    memcpy(mappedData_, data, size);
}

void Buffer::RecordCopy(vk::CommandBuffer cmd, const Buffer& dst) const {
    // HOST_WRITE → TRANSFER_READ: staging 버퍼의 호스트 쓰기가 GPU에 보이도록
    vk::BufferMemoryBarrier srcBarrier{};
    srcBarrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
    srcBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    srcBarrier.buffer = buffer_;
    srcBarrier.offset = 0;
    srcBarrier.size = size_;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eHost,
        vk::PipelineStageFlagBits::eTransfer,
        {}, {}, srcBarrier, {}
    );

    cmd.copyBuffer(buffer_, dst.buffer_, vk::BufferCopy{0, 0, size_});

    // TRANSFER_WRITE → UNIFORM_READ: compute shader에서 UBO 읽기 전 전송 완료 보장
    vk::BufferMemoryBarrier dstBarrier{};
    dstBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
    dstBarrier.dstAccessMask = vk::AccessFlagBits::eUniformRead;
    dstBarrier.buffer = dst.buffer_;
    dstBarrier.offset = 0;
    dstBarrier.size = size_;

    cmd.pipelineBarrier(
        vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eComputeShader,
        {}, {}, dstBarrier, {}
    );
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
    , mappedData_(other.mappedData_)
    , size_(other.size_)
{
    other.allocator_  = nullptr;
    other.buffer_     = VK_NULL_HANDLE;
    other.allocation_ = nullptr;
    other.mappedData_ = nullptr;
    other.size_       = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        if (buffer_ && allocation_ && allocator_) {
            vmaDestroyBuffer(allocator_, buffer_, allocation_);
        }

        allocator_  = other.allocator_;
        buffer_     = other.buffer_;
        allocation_ = other.allocation_;
        mappedData_ = other.mappedData_;
        size_       = other.size_;

        other.allocator_  = nullptr;
        other.buffer_     = VK_NULL_HANDLE;
        other.allocation_ = nullptr;
        other.mappedData_ = nullptr;
        other.size_       = 0;
    }
    return *this;
}
