#include "pivot/graphics/VulkanApplication.hxx"
#include "pivot/graphics/DebugMacros.hxx"
#include "pivot/graphics/QueueFamilyIndices.hxx"
#include "pivot/graphics/culling.hxx"
#include "pivot/graphics/vk_utils.hxx"

#include <Logger.hpp>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <algorithm>

VulkanApplication::VulkanApplication(): VulkanLoader(), window("Vulkan", 800, 600)
{
    DEBUG_FUNCTION;
    if (bEnableValidationLayers && !checkValidationLayerSupport()) {
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
    swapchainDeletionQueue.flush();
    mainDeletionQueue.flush();
}

void VulkanApplication::init() { initVulkanRessources(); }

void VulkanApplication::draw(const std::vector<std::reference_wrapper<const RenderObject>> &sceneInformation,
                             const gpuObject::CameraData &gpuCamera
#ifdef CULLING_DEBUG
                             ,
                             const std::optional<std::reference_wrapper<const gpuObject::CameraData>> cullingCamera
#endif
)
try {
    auto indices = QueueFamilyIndices::findQueueFamilies(physical_device, surface);

    auto &frame = frames[currentFrame];
    uint32_t imageIndex;
    vk::Result result;

    VK_TRY(device.waitForFences(frame.inFlightFences, VK_TRUE, UINT64_MAX));
    std::tie(result, imageIndex) =
        device.acquireNextImageKHR(swapchain.getSwapchain(), UINT64_MAX, frame.imageAvailableSemaphore);

    vk_utils::vk_try(result);
    auto &cmd = commandBuffers[imageIndex];

    device.resetFences(frame.inFlightFences);
    cmd.reset();

    vk::Semaphore waitSemaphores[] = {frame.imageAvailableSemaphore};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::Semaphore signalSemaphores[] = {frame.renderFinishedSemaphore};

    vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores,
    };

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

    auto sceneObjectGPUData = buildSceneObjectsGPUData(sceneInformation, cullingGPUCamera);
    buildIndirectBuffers(sceneObjectGPUData.objectDrawBatches, frame);

    copyBuffer(frame.data.uniformBuffer, sceneObjectGPUData.objectGPUData);

    vk::DeviceSize offset = 0;
    vk::CommandBufferBeginInfo beginInfo;
    VK_TRY(cmd.begin(&beginInfo));

    vk::BufferMemoryBarrier barrier{
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .srcQueueFamilyIndex = indices.graphicsFamily.value(),
        .dstQueueFamilyIndex = indices.graphicsFamily.value(),
        .buffer = cullingBuffer.buffer,
        .size = cpuStorage.meshesBoundingBoxes.size() * sizeof(MeshBoundingBox),
    };
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, computeLayout, 0, frame.data.objectDescriptor, nullptr);
    cmd.pushConstants<gpuObject::CameraData>(computeLayout, vk::ShaderStageFlagBits::eCompute, 0, cullingGPUCamera);
    cmd.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);
    cmd.dispatch(sceneObjectGPUData.objectDrawBatches.size() / 64 + 1, 1, 1);
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader,
                        vk::DependencyFlagBits::eByRegion, {}, barrier, {});

    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, frame.data.objectDescriptor, nullptr);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, texturesSet, nullptr);
    cmd.pushConstants<gpuObject::CameraData>(
        pipelineLayout, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, gpuCamera);
    cmd.bindVertexBuffers(0, vertexBuffers.buffer, offset);
    cmd.bindIndexBuffer(indicesBuffers.buffer, 0, vk::IndexType::eUint32);
    cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    {
        // bool *mapped = static_cast<bool *>(allocator.mapMemory(cullingBuffer.memory));
        for (const auto &draw: sceneObjectGPUData.objectDrawBatches) {
            if (true ) {
                cmd.drawIndexedIndirect(frame.indirectBuffer.buffer,
                                        draw.first * sizeof(vk::DrawIndexedIndirectCommand), draw.count,
                                        sizeof(vk::DrawIndexedIndirectCommand));
            }
        }
        // allocator.unmapMemory(cullingBuffer.memory);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    }
    cmd.endRenderPass();
    cmd.end();
    graphicsQueue.submit(submitInfo, frame.inFlightFences);

    vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = &(swapchain.getSwapchain()),
        .pImageIndices = &imageIndex,
    };
    vk_utils::vk_try(presentQueue.presentKHR(presentInfo));
    currentFrame = (currentFrame + 1) % MAX_FRAME_FRAME_IN_FLIGHT;
} catch (const vk::OutOfDateKHRError &se) {
    return recreateSwapchain();
}

VulkanApplication::SceneObjectsGPUData
VulkanApplication::buildSceneObjectsGPUData(const std::vector<std::reference_wrapper<const RenderObject>> &objects,
                                            const gpuObject::CameraData &camera)
{
    if (objects.empty()) return {};
    if (objects.size() >= MAX_OBJECT) throw TooManyObjectInSceneError();

    std::vector<DrawBatch> packedDraws;
    std::vector<gpuObject::UniformBufferObject> objectGPUData;
    unsigned drawCount = 0;

    for (const auto &object: objects) {
        packedDraws.push_back({
            .meshId = object.get().meshID,
            .first = drawCount++,
            .count = 1,
        });
        objectGPUData.push_back(
            gpuObject::UniformBufferObject(object.get().objectInformation, loadedTextures, materials));
    }
    return {packedDraws, objectGPUData};
}

void VulkanApplication::buildIndirectBuffers(const std::vector<DrawBatch> &packedDraws, Frame &frame)
{
    void *sceneData = nullptr;
    vmaMapMemory(allocator, frame.indirectBuffer.memory, &sceneData);
    auto *buffer = (VkDrawIndexedIndirectCommand *)sceneData;

    for (uint32_t i = 0; i < packedDraws.size(); i++) {
        const auto &mesh = loadedMeshes.at(packedDraws[i].meshId);

        buffer[i].firstIndex = mesh.indicesOffset;
        buffer[i].indexCount = mesh.indicesSize;
        buffer[i].vertexOffset = mesh.verticiesOffset;
        buffer[i].instanceCount = 1;
        buffer[i].firstInstance = i;
    }
    vmaUnmapMemory(allocator, frame.indirectBuffer.memory);
}

void VulkanApplication::initVulkanRessources()
{
    DEBUG_FUNCTION
    createInstance();
    createDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createAllocator();
    createSyncStructure();
    createCommandPool();

    createCullingBuffers();
    createIndirectBuffer();
    createUniformBuffers();

    this->pushModelsToGPU();
    this->pushTexturesToGPU();

    createDescriptorSetLayout();
    createTextureDescriptorSetLayout();
    createDescriptorPool();

    createComputePipelineLayout();
    createComputePipeline();

    swapchain.init(window, physical_device, device, surface);

    createRenderPass();
    createPipelineCache();
    createPipelineLayout();
    createPipeline();
    createDepthResources();
    createColorResources();
    createFramebuffers();
    createDescriptorSets();

    createTextureSampler();
    createTextureDescriptorSets();
    createCommandBuffers();
    initDearImgui();

    materials["white"] = {
        .ambientColor = {1.0f, 1.0f, 1.0f, 1.0f},
        .diffuse = {1.0f, 1.0f, 1.0f, 1.0f},
        .specular = {1.0f, 1.0f, 1.0f, 1.0f},
    };
    postInitialization();
}

void VulkanApplication::postInitialization()
{
    DEBUG_FUNCTION

    std::vector<gpuObject::Material> materialStor;
    std::transform(materials.begin(), materials.end(), std::back_inserter(materialStor),
                   [](const auto &i) { return i.second; });

    copyBuffer(materialBuffer, materialStor);
}

void VulkanApplication::recreateSwapchain()
{
    DEBUG_FUNCTION

    /// do not recreate the swapchain if the window size is 0
    vk::Extent2D size;
    do {
        size = window.getSize();
        window.pollEvent();
    } while (size.width == 0 || size.height == 0);

    logger->info("Swapchain") << "Recreaing swapchain...";
    LOGGER_ENDL;

    device.waitIdle();
    swapchainDeletionQueue.flush();

    swapchain.recreate(window, physical_device, device, surface);
    createRenderPass();
    createPipeline();
    createColorResources();
    createDepthResources();
    createFramebuffers();
    createCommandBuffers();
    initDearImgui();
    logger->info("Swapchain") << "Swapchain recreation complete... { height=" << swapchain.getSwapchainExtent().height
                              << ", width =" << swapchain.getSwapchainExtent().width
                              << ", numberOfImage = " << swapchain.nbOfImage() << " }";
    LOGGER_ENDL;
    postInitialization();
}
