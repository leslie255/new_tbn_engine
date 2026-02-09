#pragma once

#include "base.hxx"

struct OrthographicCamera : CameraBase {
    enum class CoordinateSystem {
        CenterUp,
        CenterDown,
        TopLeftDown,
        BottomLeftUp,
    };

    float z_near = -1;
    float z_far = 1;
    CoordinateSystem coordinate_system = CoordinateSystem::CenterUp;

    glm::mat4x4 projection_matrix(float width, float height) const override;

    glm::mat4x4 view_matrix() const override;
};
