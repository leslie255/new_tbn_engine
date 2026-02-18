#pragma once

#include <glm/vec3.hpp>
#include <webgpu/webgpu_cpp.h>

#include "../object.hxx"
#include "../shader_info.hxx"

/// alignas(16) to be compatible with WGSL struct of the same topology.
struct alignas(16) PhongParameters {
    float ambient_strength = 0.2;
    float diffuse_strength = 0.8;
    float specular_strength = 0.2;
    float specular_intensity = 32.0;
    glm::vec3 light_color = glm::vec3(1.0, 1.0, 1.0);
};

struct MaterialBase : public ObjectBase {
    virtual ShaderInfo create_fragment_shader(const wgpu::Device& device) const;

    virtual wgpu::BindGroupLayout create_bind_group_layout(const wgpu::Device& device) const;

    virtual wgpu::BindGroup create_bind_group(
        const wgpu::Device& device,
        wgpu::BindGroupLayout layout
    ) const;

    virtual void update_view_position(const wgpu::Queue& queue, glm::vec3 view_position);

    virtual void update_light_position(const wgpu::Queue& queue, glm::vec3 light_position);
};
