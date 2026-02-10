#pragma once

#include <webgpu/webgpu_cpp.h>

#include "../shader_info.hxx"

struct MaterialBase {
    virtual ShaderInfo create_fragment_shader(const wgpu::Device& device)
        const;

    virtual wgpu::BindGroupLayout create_bind_group_layout(const wgpu::Device& device) const;

    virtual wgpu::BindGroup create_bind_group(
        const wgpu::Device& device,
        wgpu::BindGroupLayout layout) const;
};
