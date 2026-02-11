#include "perspective.hxx"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

glm::mat4x4 PerspectiveCamera::projection_matrix(float width, float height) const {
    if (this->z_far.has_value()) {
        return glm::perspectiveRH_NO<float>(
            this->fov_y,    // fovy
            width / height, // aspect
            this->z_near,   // zNear
            this->z_far.value()
        ); // zFar
    } else {
        return glm::infinitePerspectiveRH_NO(
            this->fov_y,    // fovy
            width / height, // aspect
            this->z_near
        ); // zNear
    }
}

glm::mat4x4 PerspectiveCamera::view_matrix() const {
    return glm::lookAtRH(
        this->position,                   // eye
        this->position + this->direction, // center
        this->up
    ); // up
}
