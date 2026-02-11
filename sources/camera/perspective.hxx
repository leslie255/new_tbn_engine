#pragma once

#include "../utils.hxx"
#include "base.hxx"

struct PerspectiveCamera : public CameraBase {
    glm::vec3 position = glm::vec3(0, 0, 0);
    glm::vec3 direction = glm::vec3(0, 0, 1);
    glm::vec3 up = glm::vec3(0, 1, 0);
    /// In radians.
    float fov_y = degrees_to_radians<float>(90);
    float z_near = 0.1;
    std::optional<float> z_far = std::nullopt;

    glm::mat4x4 projection_matrix(float width, float height) const override;
    glm::mat4x4 view_matrix() const override;
};
