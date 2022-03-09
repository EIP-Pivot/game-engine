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

AssetStorage::AssetStorage(VulkanBase &base): base_ref(base) { cpuStorage.materialStaging.add("white", {}); }

AssetStorage::~AssetStorage() {}

void AssetStorage::build()
{
    DEBUG_FUNCTION

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
    vk_debug::setObjectName(base_ref->get().device, boundingboxbuffer.buffer, "BoundingBox Buffer");

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

    cpuStorage = {};
}

void AssetStorage::destroy()
{
    DEBUG_FUNCTION
    if (vertexBuffer && indicesBuffer) {
        base_ref->get().allocator.destroyBuffer(vertexBuffer);
        base_ref->get().allocator.destroyBuffer(indicesBuffer);
    }
    if (boundingboxbuffer) base_ref->get().allocator.destroyBuffer(boundingboxbuffer);
    if (materialBuffer) base_ref->get().allocator.destroyBuffer(materialBuffer);

    for (auto &image: textureStorage.getStorage()) {
        base_ref->get().device.destroyImageView(image.imageView);
        base_ref->get().allocator.destroyImage(image);
    }
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
    base_ref->get().allocator.copyBuffer(stagingVertex, cpuStorage.vertexStagingBuffer);
    vertexBuffer = stagingVertex.cloneBuffer(
        base_ref->get(), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
        vma::MemoryUsage::eGpuOnly);
    base_ref->get().allocator.destroyBuffer(stagingVertex);

    auto indexSize = cpuStorage.indexStagingBuffer.size() * sizeof(uint32_t);
    auto stagingIndex = base_ref->get().allocator.createBuffer(indexSize, vk::BufferUsageFlagBits::eTransferSrc,
                                                               vma::MemoryUsage::eCpuToGpu);
    base_ref->get().allocator.copyBuffer(stagingIndex, cpuStorage.indexStagingBuffer);
    indicesBuffer = stagingIndex.cloneBuffer(
        base_ref->get(), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
        vma::MemoryUsage::eGpuOnly);
    base_ref->get().allocator.destroyBuffer(stagingIndex);
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
        base_ref->get().allocator.copyBuffer(stagingBuffer, img.image);

        vk::ImageCreateInfo imageInfo{
            .imageType = vk::ImageType::e2D,
            .format = vk::Format::eR8G8B8A8Srgb,
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
        image.createImageView(base_ref->get());
        image.transitionLayout(base_ref->get(), vk::ImageLayout::eTransferDstOptimal);
        base_ref->get().copyBufferToImage(stagingBuffer, image);
        image.generateMipmaps(base_ref->get(), image.mipLevels);

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

    for (auto &[name, idx]: cpuStorage.materialStaging) {
        const auto &mat = cpuStorage.materialStaging.getStorage()[idx];
        materialStorage.add(
            name,
            {
                .metallic = mat.metallic,
                .roughness = mat.roughness,
                .baseColor = mat.baseColor,
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

    base_ref->get().allocator.copyBuffer(materialStaging, materialStorage.getStorage());
    materialBuffer = materialStaging.cloneBuffer(
        base_ref->get(), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vma::MemoryUsage::eGpuOnly);
    base_ref->get().allocator.destroyBuffer(materialStaging);
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

    base_ref->get().allocator.copyBuffer(boundingboxStaging, meshBoundingBoxStorage.getStorage());
    boundingboxbuffer = boundingboxStaging.cloneBuffer(
        base_ref->get(), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
        vma::MemoryUsage::eGpuOnly);
    base_ref->get().allocator.destroyBuffer(boundingboxStaging);
}

}    // namespace pivot::graphics
