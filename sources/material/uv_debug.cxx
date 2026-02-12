#include "uv_debug.hxx"

using namespace std::literals;

static std::string_view SHADER_CODE = R"(

struct VertexOut {
    @builtin(position) position_clip: vec4<f32>,
    @location(0) position_world: vec3<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) normal: vec3<f32>,
};

@fragment fn main(input: VertexOut) -> @location(0) vec4<f32> {
    let uv_deriv = fwidth(input.uv);
    let uv_width_x = uv_deriv.x * 4.0;
    let uv_width_y = uv_deriv.y * 4.0;
    let dist_to_edge_x = min(input.uv.x, 1.0 - input.uv.x);
    let dist_to_edge_y = min(input.uv.y, 1.0 - input.uv.y);
    return select(
        vec4(input.uv.x, input.uv.y, 0.28, 1.0),
        vec4(1.0, 1.0, 1.0, 1.0),
        dist_to_edge_x < uv_width_x || dist_to_edge_y < uv_width_y);
}

)";

ShaderInfo UvDebugMaterial::create_fragment_shader(const wgpu::Device& device) const {
    auto shader_source = wgpu::ShaderSourceWGSL({
        .nextInChain = nullptr,
        .code = wgpu::StringView(SHADER_CODE),
    });
    auto shader_module_descriptor = wgpu::ShaderModuleDescriptor {
        .nextInChain = &shader_source,
    };
    auto shader_module = device.CreateShaderModule(&shader_module_descriptor);

    return ShaderInfo {
        .shader_module = shader_module,
        .constants = {},
    };
}

wgpu::BindGroupLayout UvDebugMaterial::create_bind_group_layout(const wgpu::Device& device) const {
    auto descriptor = wgpu::BindGroupLayoutDescriptor {
        .label = "UV Debug Material"sv,
        .entryCount = 0,
        .entries = nullptr,
    };
    return device.CreateBindGroupLayout(&descriptor);
}

wgpu::BindGroup UvDebugMaterial::create_bind_group(
    const wgpu::Device& device,
    wgpu::BindGroupLayout layout
) const {
    auto descriptor = wgpu::BindGroupDescriptor {
        .label = "UV Debug Material"sv,
        .layout = layout,
        .entryCount = 0,
        .entries = nullptr,
    };
    return device.CreateBindGroup(&descriptor);
}
