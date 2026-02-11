#include "base.hxx"

#include <glm/ext.hpp>

glm::mat4x4 CameraBase::projection_matrix(float, float) const {
    return glm::identity<glm::mat4x4>();
}

glm::mat4x4 CameraBase::view_matrix() const {
    return glm::identity<glm::mat4x4>();
}
