#pragma once

#include <concepts>
#include <fmt/base.h>
#include <glm/matrix.hpp>
#include <webgpu/webgpu_cpp.h>

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
    virtual wgpu::VertexState create_vertex_state(const wgpu::Device& device) const;

    virtual wgpu::BindGroupLayout create_bind_group_layout(const wgpu::Device& device) const;

    virtual wgpu::BindGroup create_bind_group(
        const wgpu::Device& device,
        wgpu::BindGroupLayout layout) const;

    virtual void set_model_view(const wgpu::Queue& queue, glm::mat4x4 model, glm::mat4x4 view);

    virtual DrawParameters draw_parameters() const;
};

template <class T>
concept is_geometry = requires(T* a) {
    {
        ((const T*)a)->create_vertex_state(std::declval<const wgpu::Device&>())
    } -> std::same_as<wgpu::VertexState>;
    {
        ((const T*)a)->create_bind_group_layout(std::declval<const wgpu::Device&>())
    } -> std::same_as<wgpu::BindGroupLayout>;
    {
        ((const T*)a)->create_bind_group(
            std::declval<const wgpu::Device&>(),
            std::declval<wgpu::BindGroupLayout>())
    } -> std::same_as<wgpu::BindGroup>;
    {
        a->set_model_view(
            std::declval<const wgpu::Queue&>(),
            std::declval<glm::mat4x4>(),
            std::declval<glm::mat4x4>())
    } -> std::same_as<void>;
    { ((const T*)a)->draw_parameters() } -> std::same_as<DrawParameters>;
};
