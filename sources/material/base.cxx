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

void MaterialBase::update_view_position(const wgpu::Queue&, glm::vec3) {}

void MaterialBase::update_light_position(const wgpu::Queue&, glm::vec3) {}
