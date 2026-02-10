#pragma once

#include "base.hxx"

#include <glm/vec3.hpp>
#include <webgpu/webgpu_cpp.h>

class ColorMaterial : public MaterialBase {
    wgpu::Buffer color;          // uniform, binding 0, vec3<f32>
    wgpu::Buffer view_position;  // uniform, binding 1, vec3<f32>
    wgpu::Buffer light_position; // uniform, binding 2, vec3<f32>

  public:
    ColorMaterial() = default;

    ColorMaterial(
        const wgpu::Device& device,
        const wgpu::Queue& queue,
        glm::vec3 fill_color = glm::vec3(1, 1, 1));

    void set_color(const wgpu::Queue& queue, glm::vec3 value);

    void set_view_position(const wgpu::Queue& queue, glm::vec3 value);

    void set_light_position(const wgpu::Queue& queue, glm::vec3 value);

    ShaderInfo create_fragment_shader(const wgpu::Device& device) const override;

    wgpu::BindGroupLayout create_bind_group_layout(const wgpu::Device& device) const override;

    wgpu::BindGroup create_bind_group(const wgpu::Device& device, wgpu::BindGroupLayout layout)
        const override;
};
