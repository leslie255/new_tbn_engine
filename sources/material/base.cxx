#include "base.hxx"

ShaderInfo MaterialBase::create_fragment_shader(const wgpu::Device&) const {
    std::abort();
}

wgpu::BindGroupLayout MaterialBase::create_bind_group_layout(const wgpu::Device&) const {
    std::abort();
}

wgpu::BindGroup MaterialBase::create_bind_group(const wgpu::Device&, wgpu::BindGroupLayout) const {
    std::abort();
}
