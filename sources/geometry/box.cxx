#include "box.hxx"

#include <glm/ext/matrix_transform.hpp>

using namespace std::literals;

static std::string_view SHADER_CODE = R"(

@group(0) @binding(0) var<uniform> projection: mat4x4<f32>;

@group(1) @binding(0) var<uniform> model: mat4x4<f32>;
@group(1) @binding(1) var<uniform> model_view: mat4x4<f32>;
@group(1) @binding(2) var<uniform> normal_transform: mat4x4<f32>;

struct VertexOut {
    @builtin(position) position_clip: vec4<f32>,
    @location(0) position_world: vec3<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) normal: vec3<f32>,
};

@vertex fn main(@builtin(vertex_index) i: u32) -> VertexOut {
    const positions = array(
        // South
        vec3<f32>(0., 0., 1.),
        vec3<f32>(1., 0., 1.),
        vec3<f32>(1., 1., 1.),
        vec3<f32>(1., 1., 1.),
        vec3<f32>(0., 1., 1.),
        vec3<f32>(0., 0., 1.),
        // North
        vec3<f32>(0., 0., 0.),
        vec3<f32>(0., 1., 0.),
        vec3<f32>(1., 1., 0.),
        vec3<f32>(1., 1., 0.),
        vec3<f32>(1., 0., 0.),
        vec3<f32>(0., 0., 0.),
        // East
        vec3<f32>(1., 0., 0.),
        vec3<f32>(1., 1., 0.),
        vec3<f32>(1., 1., 1.),
        vec3<f32>(1., 1., 1.),
        vec3<f32>(1., 0., 1.),
        vec3<f32>(1., 0., 0.),
        // West
        vec3<f32>(0., 1., 0.),
        vec3<f32>(0., 0., 0.),
        vec3<f32>(0., 0., 1.),
        vec3<f32>(0., 0., 1.),
        vec3<f32>(0., 1., 1.),
        vec3<f32>(0., 1., 0.),
        // Up
        vec3<f32>(1., 1., 0.),
        vec3<f32>(0., 1., 0.),
        vec3<f32>(0., 1., 1.),
        vec3<f32>(0., 1., 1.),
        vec3<f32>(1., 1., 1.),
        vec3<f32>(1., 1., 0.),
        // Down
        vec3<f32>(0., 0., 0.),
        vec3<f32>(1., 0., 0.),
        vec3<f32>(1., 0., 1.),
        vec3<f32>(1., 0., 1.),
        vec3<f32>(0., 0., 1.),
        vec3<f32>(0., 0., 0.),
    );
    const uvs = array(
        // South
        vec2<f32>(0, 1.),
        vec2<f32>(1, 1.),
        vec2<f32>(1, 0.),
        vec2<f32>(1, 0.),
        vec2<f32>(0, 0.),
        vec2<f32>(0, 1.),
        // North
        vec2<f32>(1, 1.),
        vec2<f32>(1, 0.),
        vec2<f32>(0, 0.),
        vec2<f32>(0, 0.),
        vec2<f32>(0, 1.),
        vec2<f32>(1, 1.),
        // East
        vec2<f32>(1, 1.),
        vec2<f32>(1, 0.),
        vec2<f32>(0, 0.),
        vec2<f32>(0, 0.),
        vec2<f32>(0, 1.),
        vec2<f32>(1, 1.),
        // West
        vec2<f32>(0, 0.),
        vec2<f32>(0, 1.),
        vec2<f32>(1, 1.),
        vec2<f32>(1, 1.),
        vec2<f32>(1, 0.),
        vec2<f32>(0, 0.),
        // Up
        vec2<f32>(0, 1.),
        vec2<f32>(1, 1.),
        vec2<f32>(1, 0.),
        vec2<f32>(1, 0.),
        vec2<f32>(0, 0.),
        vec2<f32>(0, 1.),
        // Down
        vec2<f32>(0, 1.),
        vec2<f32>(1, 1.),
        vec2<f32>(1, 0.),
        vec2<f32>(1, 0.),
        vec2<f32>(0, 0.),
        vec2<f32>(0, 1.),
    );
    const normals = array(
        // South
        vec3<f32>(0., 0., 1.),
        vec3<f32>(0., 0., 1.),
        vec3<f32>(0., 0., 1.),
        vec3<f32>(0., 0., 1.),
        vec3<f32>(0., 0., 1.),
        vec3<f32>(0., 0., 1.),
        // North
        vec3<f32>(0., 0., -1.),
        vec3<f32>(0., 0., -1.),
        vec3<f32>(0., 0., -1.),
        vec3<f32>(0., 0., -1.),
        vec3<f32>(0., 0., -1.),
        vec3<f32>(0., 0., -1.),
        // East
        vec3<f32>(1., 0., 0.),
        vec3<f32>(1., 0., 0.),
        vec3<f32>(1., 0., 0.),
        vec3<f32>(1., 0., 0.),
        vec3<f32>(1., 0., 0.),
        vec3<f32>(1., 0., 0.),
        // West
        vec3<f32>(-1., 0., 0.),
        vec3<f32>(-1., 0., 0.),
        vec3<f32>(-1., 0., 0.),
        vec3<f32>(-1., 0., 0.),
        vec3<f32>(-1., 0., 0.),
        vec3<f32>(-1., 0., 0.),
        // Up
        vec3<f32>(0., 1., 0.),
        vec3<f32>(0., 1., 0.),
        vec3<f32>(0., 1., 0.),
        vec3<f32>(0., 1., 0.),
        vec3<f32>(0., 1., 0.),
        vec3<f32>(0., 1., 0.),
        // Down
        vec3<f32>(0., -1., 0.),
        vec3<f32>(0., -1., 0.),
        vec3<f32>(0., -1., 0.),
        vec3<f32>(0., -1., 0.),
        vec3<f32>(0., -1., 0.),
        vec3<f32>(0., -1., 0.),
    );

    var output: VertexOut;
    output.position_clip = projection * model_view * vec4(positions[i], 1.0);
    output.position_world = (model * vec4(positions[i], 1.0)).xyz;
    output.uv = uvs[i];
    output.normal = (normal_transform * vec4(normals[i], 1.0)).xyz;

    return output;
}

)";

BoxGeometry::BoxGeometry(const wgpu::Device& device, const wgpu::Queue& queue) {
    auto buffer_descriptor = wgpu::BufferDescriptor {
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = sizeof(float[4][4]),
        .mappedAtCreation = false,
    };
    // They're all 4x4 matrices so can share the same descriptor.
    this->model = device.CreateBuffer(&buffer_descriptor);
    this->model_view = device.CreateBuffer(&buffer_descriptor);
    this->normal_transform = device.CreateBuffer(&buffer_descriptor);

    // Initialize with identity matrices for sanity sake.
    auto identity4x4 = glm::identity<glm::mat4x4>();
    queue.WriteBuffer(this->model, 0, &identity4x4, sizeof(identity4x4));
    queue.WriteBuffer(this->model_view, 0, &identity4x4, sizeof(identity4x4));
    queue.WriteBuffer(this->normal_transform, 0, &identity4x4, sizeof(identity4x4));
}

ShaderInfo BoxGeometry::create_vertex_shader(const wgpu::Device& device) const {
    auto wgsl = wgpu::ShaderSourceWGSL({
        .nextInChain = nullptr,
        .code = wgpu::StringView(SHADER_CODE),
    });
    auto shader_module_descriptor = wgpu::ShaderModuleDescriptor {
        .nextInChain = &wgsl,
        .label = "Box"sv,
    };
    auto shader_module = device.CreateShaderModule(&shader_module_descriptor);
    return ShaderInfo {
        .shader_module = shader_module,
        .constants = {},
    };
}

wgpu::BindGroupLayout BoxGeometry::create_bind_group_layout(const wgpu::Device& device) const {
    auto entries = std::array {
        wgpu::BindGroupLayoutEntry {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                wgpu::BufferBindingLayout {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(float[4][4]),
                },
        },
        wgpu::BindGroupLayoutEntry {
            .binding = 1,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                wgpu::BufferBindingLayout {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(float[4][4]),
                },
        },
        wgpu::BindGroupLayoutEntry {
            .binding = 2,
            .visibility = wgpu::ShaderStage::Vertex,
            .buffer =
                wgpu::BufferBindingLayout {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(float[4][4]),
                },
        },
    };
    auto descriptor = wgpu::BindGroupLayoutDescriptor {
        .label = "Box"sv,
        .entryCount = entries.size(),
        .entries = entries.data(),
    };
    return device.CreateBindGroupLayout(&descriptor);
}

wgpu::BindGroup BoxGeometry::create_bind_group(
    const wgpu::Device& device,
    wgpu::BindGroupLayout layout
) const {
    auto entries = std::array {
        wgpu::BindGroupEntry {
            .binding = 0,
            .buffer = this->model,
            .offset = 0,
            .size = sizeof(float[4][4]),
        },
        wgpu::BindGroupEntry {
            .binding = 1,
            .buffer = this->model_view,
            .offset = 0,
            .size = sizeof(float[4][4]),
        },
        wgpu::BindGroupEntry {
            .binding = 2,
            .buffer = this->normal_transform,
            .offset = 0,
            .size = sizeof(float[4][4]),
        },
    };
    auto descriptor = wgpu::BindGroupDescriptor {
        .label = "Box"sv,
        .layout = layout,
        .entryCount = entries.size(),
        .entries = entries.data(),
    };
    return device.CreateBindGroup(&descriptor);
}

void BoxGeometry::set_model_view(const wgpu::Queue& queue, glm::mat4x4 model, glm::mat4x4 view) {
    queue.WriteBuffer(this->model, 0, &model, sizeof(model));

    auto model_view = view * model;
    queue.WriteBuffer(this->model_view, 0, &model_view, sizeof(model_view));

    auto normal_transform = glm::transpose(glm::inverse(model));
    queue.WriteBuffer(this->normal_transform, 0, &normal_transform, sizeof(normal_transform));
}

DrawParameters BoxGeometry::draw_parameters() const {
    return DrawParametersIndexless {
        .vertex_buffer = nullptr,
        .vertex_count = 36,
    };
}
