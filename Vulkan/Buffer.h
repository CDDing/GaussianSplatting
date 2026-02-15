#pragma once
#include "Core.h"

class Context;

class Buffer {
public:
    // GPU 전용 (DEVICE_LOCAL). data != nullptr면 내부 staging으로 초기 업로드.
    static Buffer CreateDeviceLocal(Context& context, vk::BufferUsageFlags usage,
                                    vk::DeviceSize size, const void* data = nullptr);

    // HOST_VISIBLE (매핑됨). 매 프레임 Upload용 스테이징 버퍼.
    static Buffer CreateHostVisible(Context& context, vk::BufferUsageFlags usage,
                                    vk::DeviceSize size);

    ~Buffer();
    Buffer(Buffer&&) noexcept;
    Buffer& operator=(Buffer&&) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    vk::Buffer GetHandle() const { return buffer_; }
    vk::DeviceSize GetSize() const { return size_; }

    // HOST_VISIBLE 버퍼에 데이터 쓰기 (memcpy)
    void Upload(const void* data, vk::DeviceSize size);

    // this → dst로 vkCmdCopyBuffer 기록
    void RecordCopy(vk::CommandBuffer cmd, const Buffer& dst) const;

private:
    Buffer() = default;

    VmaAllocator allocator_   = nullptr;
    VkBuffer buffer_          = VK_NULL_HANDLE;
    VmaAllocation allocation_ = nullptr;
    void* mappedData_         = nullptr;
    vk::DeviceSize size_      = 0;
};
