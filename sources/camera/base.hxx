#pragma once

#include "../object.hxx"

#include <glm/mat4x4.hpp>

struct CameraBase : public ObjectBase {
    virtual glm::vec3 view_position() const;

    virtual glm::mat4x4 projection_matrix(float width, float height) const;

    virtual glm::mat4x4 view_matrix() const;
};
