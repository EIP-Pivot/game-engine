#include "pivot/graphics/VulkanApplication.hxx"
#include "pivot/graphics/DebugMacros.hxx"
#include "pivot/graphics/culling.hxx"
#include "pivot/graphics/vk_utils.hxx"

#include <Logger.hpp>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <algorithm>

VulkanApplication::VulkanApplication()
    : pivot::graphics::VulkanBase(), assetStorage(*this), drawResolver(*this, assetStorage)
{
    DEBUG_FUNCTION;
    if (bEnableValidationLayers && !VulkanBase::checkValidationLayerSupport(validationLayers)) {
        logger->warn("Vulkan Instance") << "Validation layers requested, but not available!";
        LOGGER_ENDL;
        bEnableValidationLayers = false;
    }
    window.setKeyPressCallback(Window::Key::ESCAPE,
                               [](Window &window, const Window::Key) { window.shouldClose(true); });
}

VulkanApplication::~VulkanApplication()
{
    DEBUG_FUNCTION
    if (device) device.waitIdle();
    if (swapchain) swapchain.destroy();
    assetStorage.destroy();
    swapchainDeletionQueue.flush();
    mainDeletionQueue.flush();
    drawResolver.destroy();
    VulkanBase::destroy();
}

void VulkanApplication::init()
{
    VulkanBase::init({}, deviceExtensions, validationLayers);
    initVulkanRessources();
}

void VulkanApplication::initVulkanRessources()
{
    DEBUG_FUNCTION

    drawResolver.init();
    createSyncStructure();
    createDescriptorSetLayout();
    createTextureDescriptorSetLayout();
    createPipelineCache();
    createPipelineLayout();
    createCommandPool();
    createDescriptorPool();
    createImGuiDescriptorPool();

    auto size = window.getSize();
    swapchain.create(size, physical_device, device, surface);

    createRenderPass();
    createPipeline();
    createDepthResources();
    createColorResources();
    createFramebuffers();

    assetStorage.build();

    createTextureSampler();
    createTextureDescriptorSets();
    createCommandBuffers();
    initDearImGui();

    postInitialization();
}

void VulkanApplication::postInitialization() { DEBUG_FUNCTION }

void VulkanApplication::recreateSwapchain()
{
    DEBUG_FUNCTION

    /// do not recreate the swapchain if the window size is 0
    vk::Extent2D size = window.getSize();
    while (size.width == 0 || size.height == 0) {
        window.pollEvent();
        size = window.getSize();
    }

    logger->info("VulkanSwapchain") << "Recreating swapchain...";
    LOGGER_ENDL;

    device.waitIdle();
    swapchainDeletionQueue.flush();

    swapchain.recreate(size, physical_device, device, surface);
    createRenderPass();
    createPipeline();
    createColorResources();
    createDepthResources();
    createFramebuffers();
    createCommandBuffers();
    initDearImGui();
    logger->info("Swapchain recreation") << "New height = " << swapchain.getSwapchainExtent().height
                                         << ", New width =" << swapchain.getSwapchainExtent().width
                                         << ", numberOfImage = " << swapchain.nbOfImage();
    LOGGER_ENDL;
    postInitialization();
}

void VulkanApplication::draw(const std::vector<std::reference_wrapper<const RenderObject>> &sceneInformation,
                             const gpuObject::CameraData &gpuCamera
#ifdef CULLING_DEBUG
                             ,
                             const std::optional<std::reference_wrapper<const gpuObject::CameraData>> cullingCamera
#endif
)
try {
    auto &frame = frames[currentFrame];
    VK_TRY(device.waitForFences(frame.inFlightFences, VK_TRUE, UINT64_MAX));

    uint32_t imageIndex = swapchain.getNextImageIndex(UINT64_MAX, frame.imageAvailableSemaphore);
    allocator.setCurrentFrameIndex(imageIndex);

    auto &cmd = commandBuffersPrimary[imageIndex];
    auto &drawCmd = commandBuffersSecondary[imageIndex];
    auto &imguiCmd = imguiContext.cmdBuffer[imageIndex];

    device.resetFences(frame.inFlightFences);
    cmd.reset();
    drawCmd.reset();
    imguiCmd.reset();

    vk::Semaphore waitSemaphores[] = {frame.imageAvailableSemaphore};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::Semaphore signalSemaphores[] = {frame.renderFinishedSemaphore};

    const std::array<float, 4> vClearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    std::array<vk::ClearValue, 2> clearValues{};
    clearValues.at(0).color = vk::ClearColorValue{vClearColor};
    clearValues.at(1).depthStencil = vk::ClearDepthStencilValue{1.0f, 0};

    vk::RenderPassBeginInfo renderPassInfo{
        .renderPass = renderPass,
        .framebuffer = swapChainFramebuffers[imageIndex],
        .renderArea =
            {
                .offset = {0, 0},
                .extent = swapchain.getSwapchainExtent(),
            },
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };
#ifdef CULLING_DEBUG
    auto cullingGPUCamera = cullingCamera.value_or(std::ref(gpuCamera)).get();
#else
    auto cullingGPUCamera = gpuCamera;
#endif
    drawResolver.prepareForDraw(sceneInformation, cullingGPUCamera, currentFrame);

    vk::CommandBufferInheritanceInfo inheritanceInfo{
        .renderPass = renderPass,
        .subpass = 0,
        .framebuffer = swapChainFramebuffers.at(imageIndex),
    };

    // ImGui draw
    {
        vk::CommandBufferBeginInfo imguiBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue,
            .pInheritanceInfo = &inheritanceInfo,
        };
        VK_TRY(imguiCmd.begin(&imguiBeginInfo));
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imguiCmd);
        imguiCmd.end();
    }

    // Normal draw
    {
        vk::DeviceSize offset = 0;
        vk::CommandBufferBeginInfo drawBeginInfo{
            .flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue,
            .pInheritanceInfo = &inheritanceInfo,
        };
        VK_TRY(drawCmd.begin(&drawBeginInfo));
        drawCmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
        drawCmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
                                   drawResolver.getFrameData(currentFrame).objectDescriptor, nullptr);
        drawCmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, texturesSet, nullptr);
        drawCmd.pushConstants<gpuObject::CameraData>(
            pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, gpuCamera);
        drawCmd.bindVertexBuffers(0, assetStorage.getVertexBuffer().buffer, offset);
        drawCmd.bindIndexBuffer(assetStorage.getIndexBuffer().buffer, 0, vk::IndexType::eUint32);

        if (deviceFeature.multiDrawIndirect == VK_TRUE) {
            drawCmd.drawIndexedIndirect(drawResolver.getFrameData(currentFrame).indirectBuffer.buffer, 0,
                                        drawResolver.getFrameData(currentFrame).currentBufferSize,
                                        sizeof(vk::DrawIndexedIndirectCommand));
        } else {
            for (const auto &draw: drawResolver.getFrameData(currentFrame).packedDraws) {
                drawCmd.drawIndexedIndirect(drawResolver.getFrameData(currentFrame).indirectBuffer.buffer,
                                            draw.first * sizeof(vk::DrawIndexedIndirectCommand), draw.count,
                                            sizeof(vk::DrawIndexedIndirectCommand));
            }
        }
        drawCmd.end();
    }

    std::array<vk::CommandBuffer, 2> secondaryBuffer{drawCmd, imguiCmd};
    vk::CommandBufferBeginInfo beginInfo;
    VK_TRY(cmd.begin(&beginInfo));
    cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eSecondaryCommandBuffers);
    cmd.executeCommands(secondaryBuffer);
    cmd.endRenderPass();
    cmd.end();

    const std::array<vk::CommandBuffer, 1> submitCmd{cmd};
    vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores,
    };
    submitInfo.setCommandBuffers(submitCmd);
    graphicsQueue.submit(submitInfo, frame.inFlightFences);

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = &(swapchain.getSwapchain()),
        .pImageIndices = &imageIndex,
    };
    pivot::graphics::vk_utils::vk_try(presentQueue.presentKHR(presentInfo));
    currentFrame = (currentFrame + 1) % MaxFrameInFlight;
} catch (const vk::OutOfDateKHRError &) {
    return recreateSwapchain();
}