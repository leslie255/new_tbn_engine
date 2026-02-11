#pragma once

#include "../object_base.hxx"

#include <glm/mat4x4.hpp>

struct CameraBase : public ObjectBase {
    virtual glm::mat4x4 projection_matrix(float width, float height) const;

    virtual glm::mat4x4 view_matrix() const;
};
