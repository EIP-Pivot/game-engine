#pragma once

#include "pivot/graphics/interface/IRenderer.hxx"

namespace pivot::graphics
{

/// Draw the scene
class GraphicsRenderer : public IGraphicsRenderer
{
public:
    /// @copydoc IRenderer::IRenderer
    GraphicsRenderer(PipelineStorage &storage, AssetStorage &assets);
    ~GraphicsRenderer();

    std::string getName() const noexcept override { return "Graphics"; }
    bool onInit(const vk::Extent2D &size, VulkanBase &base_ref, vk::DescriptorSetLayout &resolverLayout,
                vk::RenderPass &pass) override;
    void onStop(VulkanBase &base_ref) override;
    bool onRecreate(const vk::Extent2D &size, VulkanBase &base_ref, vk::DescriptorSetLayout &resolverLayout,
                    vk::RenderPass &pass) override;
    bool onDraw(const CameraData &cameraData, DrawCallResolver &resolver, vk::CommandBuffer &cmd) override;

private:
    void createPipelineLayout(vk::Device &device, vk::DescriptorSetLayout &resolverLayout);
    void createPipeline(VulkanBase &base_ref, vk::RenderPass &pass, const vk::Extent2D &size);

private:
    bool bIsMultiDraw = false;
    vk::PipelineLayout pipelineLayout = VK_NULL_HANDLE;
};
}    // namespace pivot::graphics
