#pragma once

#include "pivot/graphics/VulkanException.hxx"
#include "pivot/graphics/types/AllocatedBuffer.hxx"
#include "pivot/graphics/types/AllocatedImage.hxx"
#include "pivot/graphics/types/common.hxx"

#include <vulkan/vulkan.hpp>

namespace pivot::graphics::abstract
{

/// @class AImmediateCommand
/// Used to easly add immediateCommand functionality to an herited class
class AImmediateCommand
{
public:
    /// Default ctor
    AImmediateCommand();
    /// Default dtor
    ~AImmediateCommand();

    /// Initialize the immediate context, using a device an a queue
    void init(vk::Device &, const vk::PhysicalDevice &gpu, const uint32_t queueFamilyIndex);
    /// Destroy the immediate context and releasing the reference to the device
    void destroy();

    /// Perform an command on the GPU immediately and wait for it to complete
    void immediateCommand(std::function<void(vk::CommandBuffer &)> cmd);

    template <typename T>
    /// Copy the source buffer into the destination
    void copyBuffer(AllocatedBuffer<T> &src, AllocatedBuffer<T> &dst)
    {
        if (src.getBytesSize() > dst.getAllocatedSize()) throw VulkanException("The destination buffer is too small");

        immediateCommand([&](vk::CommandBuffer &cmd) {
            vk::BufferCopy copyRegion{
                .srcOffset = 0,
                .dstOffset = 0,
                .size = src.getBytesSize(),
            };
            cmd.copyBuffer(src.buffer, dst.buffer, copyRegion);
        });
        dst.size = src.getSize();
    }

    /// Copy buffer to image
    void copyBufferToImage(const AllocatedBuffer<std::byte> &srdBuffer, AllocatedImage &dstImage);
    /// Generate mipmaps for the image
    void generateMipmaps(AllocatedImage &image, uint32_t mipLevel);

    /// Transition image layout to given format
    void transitionLayout(AllocatedImage &image, vk::ImageLayout layout);

private:
    void createImmediateContext(const uint32_t queueFamilyIndex);

private:
    OptionalRef<const vk::PhysicalDevice> physical_device_ref;
    OptionalRef<vk::Device> device_ref;

    vk::Fence immediateFence = VK_NULL_HANDLE;
    vk::CommandPool immediateCommandPool = VK_NULL_HANDLE;
    vk::Queue immediateQueue = VK_NULL_HANDLE;
};

}    // namespace pivot::graphics::abstract
