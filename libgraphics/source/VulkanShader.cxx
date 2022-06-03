#include "pivot/graphics/VulkanShader.hxx"

#include "pivot/graphics/vk_utils.hxx"

#include <Logger.hpp>
#include <fstream>

namespace pivot::graphics
{

VulkanShader::VulkanShader(const std::filesystem::path &path): name(path.stem().string()), shaderPath(path)
{
    reload();
}

VulkanShader::VulkanShader(const std::filesystem::path &path, const std::string &code)
    : name(path.filename().string()), shaderPath(path), source_code(code)
{
}

VulkanShader::VulkanShader(const std::filesystem::path &path, const std::vector<std::uint32_t> &byte_code)
    : name(path.filename().string()), shaderPath(path), byte_code(byte_code)
{
}

VulkanShader::~VulkanShader() {}

VulkanShader::Kind VulkanShader::getKind() const
{
    const auto stage = shaderPath.extension();
    if (stage == ".vert") return Kind::Vertex;
    if (stage == ".frag") return Kind::Fragment;
    if (stage == ".comp") return Kind::Compute;
    throw std::runtime_error("Unsupported shader type");
}

VulkanShader &VulkanShader::reload()
{
    if (shaderPath.extension() == ".spv")
        byte_code = vk_utils::readBinaryFile<std::uint32_t>(shaderPath);
    else
        source_code = vk_utils::readFile(shaderPath);
    return *this;
}

std::string VulkanShader::pre_process(shaderc::CompileOptions options)
{
    static shaderc::Compiler compiler;

    const auto preProcessResult = compiler.PreprocessGlsl(source_code, getShadercKind(), getName().c_str(), options);
    if (preProcessResult.GetCompilationStatus() != shaderc_compilation_status_success) {
        logger.err("Vulkan Shader/Pre-Process") << "Failed to pre-process " << shaderPath << ": ";
        logger.err("Vulkan Shader/Pre-Process") << preProcessResult.GetErrorMessage();
        throw VulkanShaderError("Failed to pre-process");
    }
    return std::string(preProcessResult.begin(), preProcessResult.end());
}

void VulkanShader::compile(VulkanShader::VulkanVersion version, VulkanShader::OptimizationLevel level)
{
    if (isCompiled() && source_code.empty()) {
        logger.warn("Vulkan Shader/compile") << "Can't compile a shader that was imported as bytecode!";
        return;
    }
    static shaderc::Compiler compiler;

    const auto options = getCompileOptions(version, level);
    const auto preprocess = pre_process(options);
    const auto filename = shaderPath.filename().string();
    const auto result = compiler.CompileGlslToSpv(preprocess, getShadercKind(), getName().c_str(), options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        logger.err("Vulkan Shader/Compile") << "While compiling shader file: " << shaderPath << ": ";
        logger.err("Vulkan Shader/Compile") << result.GetErrorMessage();
        throw VulkanShaderError("Failed to compile");
    }
    byte_code = std::vector<std::uint32_t>(result.cbegin(), result.cend());
}

shaderc::CompileOptions VulkanShader::getCompileOptions(VulkanVersion version, OptimizationLevel level)
{
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan,
                                 static_cast<decltype(shaderc_env_version_vulkan_1_0)>(version));
    switch (level) {
        case OptimizationLevel::None: options.SetGenerateDebugInfo(); break;
        case OptimizationLevel::Size: options.SetOptimizationLevel(shaderc_optimization_level_size); break;
        case OptimizationLevel::Performance:
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
            break;
    }
    for (const auto &[key, value]: macros) options.AddMacroDefinition(key, value);
    return options;
}

}    // namespace pivot::graphics
