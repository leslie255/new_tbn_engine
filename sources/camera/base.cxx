#include "base.hxx"

glm::mat4x4 CameraBase::projection_matrix(float, float) const {
    abort();
}

glm::mat4x4 CameraBase::view_matrix() const {
    abort();
}
