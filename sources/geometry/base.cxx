#include "base.hxx"
#include "../log.hxx"

wgpu::PrimitiveState GeometryBase::primitive_state() const {
    return wgpu::PrimitiveState {
        .topology = wgpu::PrimitiveTopology::TriangleList,
        .frontFace = wgpu::FrontFace::CCW,
        .cullMode = wgpu::CullMode::Back,
    };
}

ShaderInfo GeometryBase::create_vertex_shader(const wgpu::Device&) const {
    log_error("unimplemented: {}", __PRETTY_FUNCTION__);
    std::abort();
}

std::vector<wgpu::VertexBufferLayout> GeometryBase::vertex_buffer_layouts() const {
    return std::vector<wgpu::VertexBufferLayout> {};
}

wgpu::BindGroupLayout GeometryBase::create_bind_group_layout(const wgpu::Device& device) const {
    auto descriptor = wgpu::BindGroupLayoutDescriptor {
        .entryCount = 0,
        .entries = nullptr,
    };
    return device.CreateBindGroupLayout(&descriptor);
}

wgpu::BindGroup GeometryBase::create_bind_group(
    const wgpu::Device& device,
    wgpu::BindGroupLayout layout
) const {
    auto descriptor = wgpu::BindGroupDescriptor {
        .layout = layout,
        .entryCount = 0,
        .entries = nullptr,
    };
    return device.CreateBindGroup(&descriptor);
}

void GeometryBase::set_model_view(const wgpu::Queue&, glm::mat4x4, glm::mat4x4) {}

DrawParameters GeometryBase::draw_parameters() const {
    log_error("unimplemented: {}", __PRETTY_FUNCTION__);
    std::abort();
}
