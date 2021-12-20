#include "pivot/graphics/VulkanBase.hxx"
#include "pivot/graphics/DebugMacros.hxx"

namespace pivot::graphics
{
VulkanBase::VulkanBase(const std::string &windowName)
    : VulkanLoader(), abstract::AImmediateCommand(), window(windowName, 800, 600)
{
}

VulkanBase::~VulkanBase() {}

void VulkanBase::init(const std::vector<const char *> &instanceExtensions,
                      const std::vector<const char *> &deviceExtensions,
                      const std::vector<const char *> &validationLayers)
{
    DEBUG_FUNCTION
    createInstance(instanceExtensions, validationLayers);
    createDebugMessenger();
    createSurface();
    selectPhysicalDevice(deviceExtensions);
    createLogicalDevice(deviceExtensions);
    createAllocator();
    abstract::AImmediateCommand::init(device, queueIndices.transferFamily.value());
}

void VulkanBase::destroy()
{
    DEBUG_FUNCTION;
    abstract::AImmediateCommand::destroy();
    baseDeletionQueue.flush();
}

}    // namespace pivot::graphics