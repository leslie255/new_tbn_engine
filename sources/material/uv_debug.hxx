#pragma once

#include "base.hxx"

class UvDebugMaterial : public MaterialBase {
  public:
    ShaderInfo create_fragment_shader(const wgpu::Device& device) const override;

    wgpu::BindGroupLayout create_bind_group_layout(const wgpu::Device& device) const override;

    wgpu::BindGroup create_bind_group(const wgpu::Device& device, wgpu::BindGroupLayout layout)
        const override;
};
