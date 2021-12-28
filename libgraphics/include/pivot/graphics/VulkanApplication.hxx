#pragma once

#include "pivot/graphics/AssetStorage.hxx"
#include "pivot/graphics/DeletionQueue.hxx"
#include "pivot/graphics/QueueFamilyIndices.hxx"
#include "pivot/graphics/VulkanBase.hxx"
#include "pivot/graphics/VulkanSwapchain.hxx"
#include "pivot/graphics/Window.hxx"
#include "pivot/graphics/types/Frame.hxx"
#include "pivot/graphics/types/Material.hxx"
#include "pivot/graphics/types/Mesh.hxx"
#include "pivot/graphics/types/RenderObject.hxx"
#include "pivot/graphics/types/common.hxx"
#include "pivot/graphics/types/vk_types.hxx"
#include "pivot/graphics/vk_utils.hxx"

#include <cstring>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

#ifndef MAX_OBJECT
#define MAX_OBJECT 5000
#endif

#ifndef MAX_TEXTURES
#define MAX_TEXTURES 1000
#endif

#ifndef MAX_COMMANDS
#define MAX_COMMANDS MAX_OBJECT
#endif

#ifndef MAX_MATERIALS
#define MAX_MATERIALS 100
#endif

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

/// @class VulkanApplication
/// @brief Main class of the graphics engine
///
/// To use it, you need to first load the textures and the 3D models using the appropriate methods
///
/// Then you will call the init() method, to initialise the Vulkan ressources
///
/// You can now call the draw() method when you are ready to render a new frame
class VulkanApplication : public pivot::graphics::VulkanBase
{
private:
    struct DrawBatch {
        std::string meshId;
        uint32_t first;
        uint32_t count;
    };

    struct SceneObjectsGPUData {
        std::vector<DrawBatch> objectDrawBatches;
        std::vector<gpuObject::UniformBufferObject> objectGPUData;
    };

public:
    /// Default constructor
    VulkanApplication();
    /// Default destructor
    ~VulkanApplication();

    /// @brief Initialise the Vulkan ressources
    ///
    /// @throw VulkanException if something went awry
    void init();

    /// @brief draw the next frame
    ///
    /// @arg scene The 3D scene to draw
    /// @arg camera The information about the camera
    ///
    /// You must have already loaded your models and texture !
    void draw(const std::vector<std::reference_wrapper<const RenderObject>> &sceneInformation,
              const gpuObject::CameraData &camera
#ifdef CULLING_DEBUG
              ,
              const std::optional<std::reference_wrapper<const gpuObject::CameraData>> cullingCamera = std::nullopt
#endif
    );

    /// @brief get Swapchain aspect ratio
    float getAspectRatio() const noexcept { return swapchain.getAspectRatio(); }

private:
    SceneObjectsGPUData buildSceneObjectsGPUData(const std::vector<std::reference_wrapper<const RenderObject>> &objects,
                                                 const gpuObject::CameraData &camera);
    void buildIndirectBuffers(const std::vector<DrawBatch> &scene, Frame &frame);

    void postInitialization();
    void recreateSwapchain();
    void initVulkanRessources();

    void createUniformBuffers();
    void createSyncStructure();

    void createDescriptorSetLayout();
    void createTextureDescriptorSetLayout();

    void createDescriptorPool();
    void createDescriptorSets();
    void createTextureDescriptorSets();
    void createCommandPool();
    void createCommandBuffers();

    void createPipelineCache();
    void createPipelineLayout();
    void createPipeline();
    void createDepthResources();
    void createColorResources();
    void createRenderPass();
    void createIndirectBuffer();
    void createTextureSampler();
    void createFramebuffers();

    void createImGuiDescriptorPool();
    void initDearImGui();

public:
    /// The application asssets storage
    pivot::graphics::AssetStorage assetStorage;

private:
    /// @cond
    uint32_t mipLevels = 0;

    struct ImGuiContext {
        vk::CommandPool cmdPool = VK_NULL_HANDLE;
        std::vector<vk::CommandBuffer> cmdBuffer;
        vk::DescriptorPool pool = VK_NULL_HANDLE;
    } imguiContext;

    DeletionQueue mainDeletionQueue;
    DeletionQueue swapchainDeletionQueue;
    VulkanSwapchain swapchain;

    uint8_t currentFrame = 0;
    Frame frames[MAX_FRAME_FRAME_IN_FLIGHT];

    vk::RenderPass renderPass = VK_NULL_HANDLE;
    vk::DescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    vk::DescriptorSetLayout texturesSetLayout = VK_NULL_HANDLE;
    vk::DescriptorPool descriptorPool = VK_NULL_HANDLE;
    vk::DescriptorSet texturesSet = VK_NULL_HANDLE;

    vk::Sampler textureSampler = VK_NULL_HANDLE;

    vk::CommandPool commandPool = VK_NULL_HANDLE;

    std::vector<vk::CommandBuffer> commandBuffersPrimary;
    std::vector<vk::CommandBuffer> commandBuffersSecondary;

    AllocatedImage depthResources = {};
    AllocatedImage colorImage = {};

    vk::PipelineCache pipelineCache = VK_NULL_HANDLE;
    vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE;
    vk::Pipeline graphicsPipeline = VK_NULL_HANDLE;

    std::vector<vk::Framebuffer> swapChainFramebuffers;
    /// @endcond
};
