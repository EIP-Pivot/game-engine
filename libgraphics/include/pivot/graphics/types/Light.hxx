#pragma once

#include "pivot/graphics/types/Transform.hxx"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vector>

namespace pivot::graphics
{

namespace gpu_object
{
    /// Represent a omnidirectional light
    /// @cond
    struct PointLight {
        alignas(16) glm::vec4 position;
        alignas(16) glm::vec4 color = {1.0f, 1.0f, 1.0f, 0.0f};
        alignas(4) float intensity = 1.0f;
        alignas(4) float minRadius = 1.0f;
        alignas(4) float radius = 10.0f;
        alignas(4) float falloff = 1.0f;
    };
    static_assert(sizeof(PointLight) % 4 == 0);
    static_assert(sizeof(PointLight) == (sizeof(float) * 4) + (sizeof(float) * 4) + (sizeof(float) * 4));
    /// @endcond

    /// Represent a Directional Light
    /// @cond
    struct DirectionalLight {
        alignas(16) glm::vec4 orientation;
        alignas(16) glm::vec4 color = {1.0f, 1.0f, 1.0f, 0.0f};
        alignas(4) float intensity = 1.0f;
    };
    /// @endcond
    static_assert(sizeof(DirectionalLight) % 4 == 0);
    static_assert(sizeof(DirectionalLight) == (sizeof(float) * 4) + (sizeof(float) * 4) + (sizeof(float)) + 12);

    /// Represent a Spot Light
    /// @cond
    struct SpotLight {
        alignas(16) glm::vec4 position;
        alignas(16) glm::vec4 direction;
        alignas(16) glm::vec4 color = {1.0f, 1.0f, 1.0f, 0.0f};
        alignas(4) float cutOff = glm::cos(glm::radians(12.5f));
        alignas(4) float outerCutOff = glm::cos(glm::radians(17.5f));
        alignas(4) float intensity = 1.0f;
    };
    /// @endcond
    /// TODO: Check ensure the cutOff > outerCutOff
    static_assert(sizeof(SpotLight) % 4 == 0);
    static_assert(sizeof(SpotLight) == ((sizeof(float) * 4) * 3) + (sizeof(float) * 3) + 4);

}    // namespace gpu_object

/// The CPU side point light representation
/// @cond
struct PointLight {
    Transform position;
    glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float minRadius = 1.0f;
    float radius = 10.0f;
    float falloff = 1.0f;
};
/// @endcond

/// The CPU side Directional light representation
/// @cond
struct DirectionalLight {
    Transform position;
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius = 0.5f;
};
/// @endcond

/// The CPU side spot light representation
/// @cond
struct SpotLight {
    Transform position;
    glm::vec3 color = {1.0f, 1.0f, 1.0f};
    float cutOff = 0.3f;
};
/// @endcond

}    // namespace pivot::graphics
