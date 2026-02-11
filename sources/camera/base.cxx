#include "base.hxx"

#include <glm/ext.hpp>

glm::vec3 CameraBase::view_position() const {
    return glm::vec3(0, 0, 0);
}

glm::mat4x4 CameraBase::projection_matrix(float, float) const {
    return glm::identity<glm::mat4x4>();
}

glm::mat4x4 CameraBase::view_matrix() const {
    return glm::identity<glm::mat4x4>();
}
