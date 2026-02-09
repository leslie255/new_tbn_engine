#include "geometry_base.hxx"

wgpu::VertexState GeometryBase::create_vertex_state(const wgpu::Device&) const {
    __builtin_trap();
}

wgpu::BindGroupLayout GeometryBase::create_bind_group_layout(const wgpu::Device&) const {
    __builtin_trap();
}

wgpu::BindGroup GeometryBase::create_bind_group(const wgpu::Device&, wgpu::BindGroupLayout) const {
    __builtin_trap();
}

void GeometryBase::set_model_view(const wgpu::Queue&, glm::mat4x4, glm::mat4x4) {
    __builtin_trap();
}

DrawParameters GeometryBase::draw_parameters() const {
    __builtin_trap();
}
