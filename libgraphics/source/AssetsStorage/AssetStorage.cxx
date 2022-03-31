#include "pivot/graphics/AssetStorage.hxx"

#include "pivot/graphics/DebugMacros.hxx"
#include "pivot/graphics/vk_debug.hxx"

#include <Logger.hpp>

namespace pivot::graphics
{

const std::unordered_map<std::string, AssetStorage::AssetHandler> AssetStorage::supportedTexture = {
    {".png", &AssetStorage::loadPngTexture},
    {".ktx", &AssetStorage::loadKtxImage},
};

const std::unordered_map<std::string, AssetStorage::AssetHandler> AssetStorage::supportedObject = {
    {".obj", &AssetStorage::loadObjModel},
    {".gltf", &AssetStorage::loadGltfModel},
};

const std::unordered_map<std::string, AssetStorage::AssetHandler> AssetStorage::supportedFonts = {
    {".ttf", &AssetStorage::loadTTFFont},
};

AssetStorage::AssetStorage(VulkanBase &base): base_ref(base) { cpuStorage.materialStaging.add("white", {}); }

AssetStorage::~AssetStorage() {}

void AssetStorage::build()
{
    DEBUG_FUNCTION
    createTextureSampler();

    for (const auto &[name, model]: modelStorage) {
        meshBoundingBoxStorage.add(
            name, gpu_object::MeshBoundingBox(std::span(
                      cpuStorage.vertexStagingBuffer.begin() + model.mesh.vertexOffset, model.mesh.vertexSize)));
    }
    assert(modelStorage.size() == meshBoundingBoxStorage.size());

    logger.info("Asset Storage") << prefabStorage.size() << " prefab loaded";
    logger.info("Asset Storage") << "Pushing " << modelStorage.size() << " models onto the GPU";
    pushModelsOnGPU();
    vk_debug::setObjectName(base_ref->get().device, vertexBuffer.buffer, "Vertex Buffer");
    vk_debug::setObjectName(base_ref->get().device, indicesBuffer.buffer, "Indices Buffer");

    logger.info("Asset Storage") << "Pushing " << meshBoundingBoxStorage.size() << " bounding boxes onto the GPU";
    pushBoundingBoxesOnGPU();
    vk_debug::setObjectName(base_ref->get().device, boundingboxBuffer.buffer, "BoundingBox Buffer");

    logger.info("Asset Storage") << "Pushing " << cpuStorage.textureStaging.size() << " textures onto the GPU";
    pushTexturesOnGPU();
    for (const auto &[name, idex]: textureStorage) {
        const auto &im = textureStorage.get(idex);
        vk_debug::setObjectName(base_ref->get().device, im.image, "Texture " + name);
        vk_debug::setObjectName(base_ref->get().device, im.imageView, "Texture " + name + " ImageView");
    }

    logger.info("Asset Storage") << "Pushing " << cpuStorage.materialStaging.size() << " materials onto the GPU";
    pushMaterialOnGPU();
    vk_debug::setObjectName(base_ref->get().device, materialBuffer.buffer, "Material Buffer");

    logger.info("Asset Storage") << "Pushing " << cpuStorage.characterStaging.size() << " characters onto the GPU";
    pushCharactersOnGPU();
    vk_debug::setObjectName(base_ref->get().device, characterBuffer.buffer, "Character Buffer");

    cpuStorage = {};
    createDescriptorSetLayout();
    createDescriptorPool();
    createDescriptorSet();
}

void AssetStorage::destroy()
{
    DEBUG_FUNCTION
    if (vertexBuffer && indicesBuffer) {
        base_ref->get().allocator.destroyBuffer(vertexBuffer);
        base_ref->get().allocator.destroyBuffer(indicesBuffer);
    }
    if (boundingboxBuffer) base_ref->get().allocator.destroyBuffer(boundingboxBuffer);
    if (materialBuffer) base_ref->get().allocator.destroyBuffer(materialBuffer);

    for (auto &image: textureStorage.getStorage()) {
        base_ref->get().device.destroyImageView(image.imageView);
        base_ref->get().allocator.destroyImage(image);
    }
    if (characterBuffer) base_ref->get().allocator.destroyBuffer(characterBuffer);
    vulkanDeletionQueue.flush();
}

bool AssetStorage::bindForGraphics(vk::CommandBuffer &cmd, const vk::PipelineLayout &pipelineLayout,
                                   std::uint32_t descriptorSetNb, bool bindVertex, bool bindIndex)
{
    if (!vertexBuffer || !indicesBuffer || !descriptorSet) return false;

    vk::DeviceSize offset = 0;
    if (bindVertex) cmd.bindVertexBuffers(0, vertexBuffer.buffer, offset);
    if (bindIndex) cmd.bindIndexBuffer(indicesBuffer.buffer, 0, vk::IndexType::eUint32);
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, descriptorSetNb, descriptorSet, nullptr);
    return true;
}

bool AssetStorage::bindForCompute(vk::CommandBuffer &cmd, const vk::PipelineLayout &pipelineLayout,
                                  std::uint32_t descriptorSetNb)
{
    if (!descriptorSet) return false;
    cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, descriptorSetNb, descriptorSet, nullptr);
    return true;
}

bool AssetStorage::loadModel(const std::filesystem::path &path)
{
    auto iter = supportedObject.find(path.extension().string());
    if (iter == supportedObject.end()) {
        logger.err("LOAD MODEL") << "Not supported model extension: " << path.extension();
        return false;
    }
    logger.info("Asset Storage") << "Loading model at : " << path;
    return std::apply(iter->second, std::make_tuple(this, path));
}

bool AssetStorage::loadTexture(const std::filesystem::path &path)
{
    DEBUG_FUNCTION
    const auto iter = supportedTexture.find(path.extension().string());
    if (iter == supportedTexture.end()) {
        logger.err("Load Texture") << "Not supported texture extension: " << path.extension();
        return false;
    }
    logger.info("Asset Storage") << "Loading texture at : " << path;
    return std::apply(iter->second, std::make_tuple(this, path));
}

bool AssetStorage::loadFont(const std::filesystem::path &path)
{
    auto iter = supportedFonts.find(path.extension().string());
    if (iter == supportedFonts.end()) {
        logger.err("LOAD MODEL") << "Not supported font extension: " << path.extension();
        return false;
    }
    logger.info("Asset Storage") << "Loading font at : " << path;
    return std::apply(iter->second, std::make_tuple(this, path));
}

template <typename T>
static void copy_with_staging_buffer(VulkanBase &base_ref, AllocatedBuffer &staging, AllocatedBuffer &dst,
                                     const std::vector<T> &data)
{
    base_ref.allocator.copyBuffer(staging, std::span(data));
    base_ref.copyBuffer(staging, dst);
    base_ref.allocator.destroyBuffer(staging);
}

void AssetStorage::pushModelsOnGPU()
{
    DEBUG_FUNCTION

    auto vertexSize = cpuStorage.vertexStagingBuffer.size() * sizeof(Vertex);

    if (vertexSize == 0) {
        logger.warn("Asset Storage") << "No model to push";
        return;
    }
    auto stagingVertex = base_ref->get().allocator.createBuffer(vertexSize, vk::BufferUsageFlagBits::eTransferSrc,
                                                                vma::MemoryUsage::eCpuToGpu);
    vertexBuffer = base_ref->get().allocator.createBuffer(
        vertexSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vma::MemoryUsage::eGpuOnly);

    copy_with_staging_buffer(base_ref->get(), stagingVertex, vertexBuffer, cpuStorage.vertexStagingBuffer);

    auto indexSize = cpuStorage.indexStagingBuffer.size() * sizeof(uint32_t);
    auto stagingIndex = base_ref->get().allocator.createBuffer(indexSize, vk::BufferUsageFlagBits::eTransferSrc,
                                                               vma::MemoryUsage::eCpuToGpu);
    indicesBuffer = base_ref->get().allocator.createBuffer(
        indexSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
        vma::MemoryUsage::eGpuOnly);
    copy_with_staging_buffer(base_ref->get(), stagingIndex, indicesBuffer, cpuStorage.indexStagingBuffer);
}

void AssetStorage::pushTexturesOnGPU()
{
    DEBUG_FUNCTION
    if (cpuStorage.textureStaging.size() == 0) {
        logger.warn("Asset Storage") << "No textures to push";
        return;
    }
    for (const auto &[name, idx]: cpuStorage.textureStaging) {
        const auto &img = cpuStorage.textureStaging.get(idx);
        AllocatedBuffer stagingBuffer = base_ref->get().allocator.createBuffer(
            img.image.size(), vk::BufferUsageFlagBits::eTransferSrc, vma::MemoryUsage::eCpuToGpu);
        base_ref->get().allocator.copyBuffer(stagingBuffer, std::span(img.image));

        vk::ImageCreateInfo imageInfo{
            .imageType = vk::ImageType::e2D,
            .format = img.format,
            .extent = img.size,
            .mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(img.size.width, img.size.height))) + 1),
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
                     vk::ImageUsageFlagBits::eSampled,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
        };
        vma::AllocationCreateInfo allocInfo;
        allocInfo.usage = vma::MemoryUsage::eGpuOnly;
        auto image = base_ref->get().allocator.createImage(imageInfo, allocInfo);
        image.createImageView(base_ref->get().device);
        base_ref->get().transitionLayout(image, vk::ImageLayout::eTransferDstOptimal);
        base_ref->get().copyBufferToImage(stagingBuffer, image);
        base_ref->get().generateMipmaps(image, image.mipLevels);

        base_ref->get().allocator.destroyBuffer(stagingBuffer);
        textureStorage.add(name, std::move(image));
    }
}

void AssetStorage::pushMaterialOnGPU()
{
    DEBUG_FUNCTION
    auto size = sizeof(gpu_object::Material) * cpuStorage.materialStaging.size();
    if (size == 0) {
        logger.warn("Asset Storage") << "No material to push";
        return;
    }
    auto materialStaging = base_ref->get().allocator.createBuffer(
        size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
        vma::MemoryUsage::eCpuToGpu);
    materialBuffer = base_ref->get().allocator.createBuffer(
        size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vma::MemoryUsage::eGpuOnly);

    for (auto &[name, idx]: cpuStorage.materialStaging) {
        const auto &mat = cpuStorage.materialStaging.getStorage()[idx];
        materialStorage.add(
            name,
            {
                .baseColor = mat.baseColor,
                .metallic = mat.metallic,
                .roughness = mat.roughness,
                .baseColorTexture =
                    (mat.baseColorTexture.empty()) ? (-1) : (textureStorage.getIndex(mat.baseColorTexture)),
                .metallicRoughnessTexture = (mat.metallicRoughnessTexture.empty())
                                                ? (-1)
                                                : (textureStorage.getIndex(mat.metallicRoughnessTexture)),
                .normalTexture = (mat.normalTexture.empty()) ? (-1) : (textureStorage.getIndex(mat.normalTexture)),
                .occlusionTexture =
                    (mat.occlusionTexture.empty()) ? (-1) : (textureStorage.getIndex(mat.occlusionTexture)),
                .emissiveTexture =
                    (mat.emissiveTexture.empty()) ? (-1) : (textureStorage.getIndex(mat.emissiveTexture)),
            });
    }
    copy_with_staging_buffer(base_ref->get(), materialStaging, materialBuffer, materialStorage.getStorage());
}

void AssetStorage::pushBoundingBoxesOnGPU()
{
    DEBUG_FUNCTION
    auto size = sizeof(gpu_object::MeshBoundingBox) * meshBoundingBoxStorage.size();
    if (size == 0) {
        logger.warn("Asset Storage") << "No bounding box to push";
        return;
    }
    auto boundingboxStaging = base_ref->get().allocator.createBuffer(
        size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
        vma::MemoryUsage::eCpuToGpu);
    boundingboxBuffer = base_ref->get().allocator.createBuffer(
        size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vma::MemoryUsage::eGpuOnly);
    copy_with_staging_buffer(base_ref->get(), boundingboxStaging, boundingboxBuffer,
                             meshBoundingBoxStorage.getStorage());
}

void AssetStorage::pushCharactersOnGPU()
{
    DEBUG_FUNCTION
    auto size = sizeof(gpu_object::Character) * cpuStorage.characterStaging.size();
    if (size == 0) {
        logger.warn("Asset Storage") << "No Characters to push";
        return;
    }
    auto characterStaging = base_ref->get().allocator.createBuffer(
        size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc,
        vma::MemoryUsage::eCpuToGpu);
    characterBuffer = base_ref->get().allocator.createBuffer(
        size, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vma::MemoryUsage::eGpuOnly);
    for (auto &[name, idx]: cpuStorage.characterStaging) {
        const auto &character = cpuStorage.characterStaging.getStorage()[idx];
        charStorage.add(
            name,
            {
                .textureId = (character.textureId.empty()) ? (-1) : (textureStorage.getIndex(character.textureId)),
                .bearing = character.bearing,
                .advance = character.advance,
            });
    }
    copy_with_staging_buffer(base_ref->get(), characterStaging, characterBuffer, charStorage.getStorage());
}

}    // namespace pivot::graphics
