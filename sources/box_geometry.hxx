#pragma once

#include "geometry_base.hxx"

#include <glm/common.hpp>
#include <glm/matrix.hpp>

class BoxGeometry : public GeometryBase {
    wgpu::Buffer model_view;
    wgpu::Buffer normal_transform;

  public:
    BoxGeometry() = default;
    BoxGeometry(const wgpu::Device& device, const wgpu::Queue& queue);

    wgpu::VertexState create_vertex_state(const wgpu::Device& device) const override;

    wgpu::BindGroupLayout create_bind_group_layout(const wgpu::Device& device) const override;

    wgpu::BindGroup create_bind_group(const wgpu::Device& device, wgpu::BindGroupLayout layout)
        const override;

    void set_model_view(const wgpu::Queue& queue, glm::mat4x4 model, glm::mat4x4 view) override;

    virtual DrawParameters draw_parameters() const override;
};
