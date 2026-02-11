#include "orthographic.hxx"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

glm::mat4x4 OrthographicCamera::projection_matrix(float width, float height) const {
    switch (this->coordinate_system) {
    case CoordinateSystem::CenterUp:
        return glm::orthoRH_NO<float>(
            -0.5 * width,  // left
            0.5 * width,   // right
            -0.5 * height, // bottom
            0.5 * height,  // top
            this->z_near,  // zNear
            this->z_far
        ); // zFar
    case CoordinateSystem::CenterDown:
        return glm::orthoRH_NO<float>(
            -0.5 * width,  // left
            0.5 * width,   // right
            0.5 * height,  // bottom
            -0.5 * height, // top
            this->z_near,  // zNear
            this->z_far
        ); // zFar
    case CoordinateSystem::TopLeftDown:
        return glm::orthoRH_NO<float>(
            0.0,          // left
            width,        // right
            height,       // bottom
            0.0,          // top
            this->z_near, // zNear
            this->z_far
        ); // zFar
    case CoordinateSystem::BottomLeftUp:
        return glm::orthoRH_NO<float>(
            0.0,          // left
            width,        // right
            0.0,          // bottom
            height,       // top
            this->z_near, // zNear
            this->z_far
        ); // zFar
    }
}

glm::mat4x4 OrthographicCamera::view_matrix() const {
    return glm::identity<glm::mat4x4>();
}
