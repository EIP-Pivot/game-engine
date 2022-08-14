#pragma once

#include <iostream>
#include <stdexcept>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include "pivot/graphics/types/vk_types.hxx"
#include <pivot/utility/entity.hxx>

namespace pivot::graphics
{
/// @struct Transform
///
/// @brief Hold the model matrix
class Transform
{
public:
    /// Get the model matrix
    glm::mat4 getModelMatrix() const noexcept { return recomposeMatrix(*this); }

    /// Default equality operator
    bool operator==(const Transform &) const = default;

    /// Compute transform with root
    Transform with_root(const Transform &root)
    {
        return Transform{
            .position = this->position + root.position,
            .rotation = this->rotation + root.rotation,
            .scale = this->scale,
            .root = EntityRef::empty(),
        };
    }

private:
    static glm::mat4 recomposeMatrix(const Transform &tran)
    {
        return glm::translate(glm::mat4(1.0f), tran.position) * glm::toMat4(glm::quat(tran.rotation)) *
               glm::scale(glm::mat4(1.0f), tran.scale);
    }

public:
    /// Translation or position component
    glm::vec3 position = glm::vec3(0.0f);

    /// Rotation component
    glm::vec3 rotation = glm::vec3(0.0f);

    /// Scale component
    glm::vec3 scale = glm::vec3(1.0f);

    /// Root of the transform
    pivot::EntityRef root = EntityRef::empty();
};

inline std::ostream &operator<<(std::ostream &os, const glm::vec3 &vec)
{
    return os << "(" << vec.x << "," << vec.y << "," << vec.z << ")";
}
inline std::ostream &operator<<(std::ostream &os, const Transform &transform)
{
    return os << "Position: " << transform.position << std::endl
              << "Rotation: " << transform.rotation << std::endl
              << "Scale: " << transform.scale << std::endl
              << "Root: " << transform.root;
}
}    // namespace pivot::graphics
