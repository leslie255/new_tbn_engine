#pragma once

#include <glm/mat4x4.hpp>

struct CameraBase {
    virtual glm::mat4x4 projection_matrix(float width, float height) const;

    virtual glm::mat4x4 view_matrix() const;

    virtual ~CameraBase() = default;
};
