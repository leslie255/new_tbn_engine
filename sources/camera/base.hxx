#pragma once

#include <glm/mat4x4.hpp>

struct CameraBase {
    virtual glm::mat4x4 projection_matrix(float width, float height) const;

    virtual glm::mat4x4 view_matrix() const;

    virtual ~CameraBase() = default;
};

template <class T>
concept is_camera = requires(T* a) {
    { ((const T*)a)->projection_matrix(std::declval<float>()) } -> std::same_as<glm::mat4x4>;
    { ((const T*)a)->view_matrix() } -> std::same_as<glm::mat4x4>;
};
