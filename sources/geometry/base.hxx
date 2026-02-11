#pragma once

#include <fmt/base.h>
#include <glm/matrix.hpp>
#include <webgpu/webgpu_cpp.h>

#include "../shader_info.hxx"

struct DrawParametersIndexed {
    wgpu::Buffer index_buffer;
    wgpu::IndexFormat index_format;
    /// `nullptr` for vertexless drawing.
    wgpu::Buffer vertex_buffer;
    uint32_t index_count;
    uint32_t instance_count;
    uint32_t first_index = 0;
    int32_t base_vertex = 0;
    uint32_t first_instance = 0;
};

struct DrawParametersIndexless {
    /// `nullptr` for vertexless drawing.
    wgpu::Buffer vertex_buffer;
    uint32_t vertex_count;
    uint32_t instance_count = 1;
    uint32_t first_vertex = 0;
    uint32_t first_instance = 0;
};

using DrawParameters = std::variant<DrawParametersIndexed, DrawParametersIndexless>;

struct GeometryBase {
    virtual ShaderInfo create_vertex_shader(const wgpu::Device& device) const;

    virtual wgpu::BindGroupLayout create_bind_group_layout(const wgpu::Device& device) const;

    virtual wgpu::PrimitiveState primitive_state() const;

    virtual wgpu::BindGroup create_bind_group(
        const wgpu::Device& device,
        wgpu::BindGroupLayout layout
    ) const;

    virtual void set_model_view(const wgpu::Queue& queue, glm::mat4x4 model, glm::mat4x4 view);

    virtual DrawParameters draw_parameters() const;

    virtual ~GeometryBase() = default;
};
