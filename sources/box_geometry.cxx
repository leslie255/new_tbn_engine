#include "box_geometry.hxx"

#include <glm/ext/matrix_transform.hpp>

static std::string_view SHADER_CODE = R"(

@group(0) @binding(0) var<uniform> projection: mat4x4<f32>;

@group(1) @binding(0) var<uniform> model_view: mat4x4<f32>;
@group(1) @binding(1) var<uniform> normal_transform: mat4x4<f32>;

@vertex fn main(@builtin(vertex_index) i: u32) -> @builtin(position) vec4<f32> {
    const pos = array(
        vec2(0.0, 1.0),
        vec2(-1.0, -1.0),
        vec2(1.0, -1.0),
    );
    return projection * model_view * vec4(pos[i], 0.0, 1.0);
}

)";

BoxGeometry::BoxGeometry(const wgpu::Device& device, const wgpu::Queue& queue) {
    auto buffer_descriptor = wgpu::BufferDescriptor {
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = sizeof(float[4][4]),
        .mappedAtCreation = false,
    };
    // They're both 4x4 matrices so can share the same descriptor.
    this->model_view = device.CreateBuffer(&buffer_descriptor);
    this->normal_transform = device.CreateBuffer(&buffer_descriptor);

    auto identity4x4 = glm::identity<glm::mat4x4>();
    queue.WriteBuffer(this->model_view, 0, &identity4x4, sizeof(identity4x4));
    queue.WriteBuffer(this->normal_transform, 0, &identity4x4, sizeof(identity4x4));
}

wgpu::VertexState BoxGeometry::create_vertex_state(const wgpu::Device& device) const {
    auto wgsl = wgpu::ShaderSourceWGSL({
        .nextInChain = nullptr,
        .code = wgpu::StringView(SHADER_CODE),
    });
    auto shader_module_descriptor = wgpu::ShaderModuleDescriptor {.nextInChain = &wgsl};
    auto shader_module = device.CreateShaderModule(&shader_module_descriptor);
    return wgpu::VertexState {
        .module = shader_module,
        .entryPoint = wgpu::StringView("main"),
    };
}

wgpu::BindGroupLayout BoxGeometry::create_bind_group_layout(const wgpu::Device& device) const {
    auto layout_entries = std::array {
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
    };
    auto bind_group_layout_descriptor = wgpu::BindGroupLayoutDescriptor {
        .entryCount = layout_entries.size(),
        .entries = layout_entries.data(),
    };
    return device.CreateBindGroupLayout(&bind_group_layout_descriptor);
}

wgpu::BindGroup BoxGeometry::create_bind_group(
    const wgpu::Device& device,
    wgpu::BindGroupLayout layout) const {
    auto bind_group_0_entries = std::array {
        wgpu::BindGroupEntry {
            .binding = 0,
            .buffer = this->model_view,
            .offset = 0,
            .size = sizeof(float[4][4]),
        },
        wgpu::BindGroupEntry {
            .binding = 1,
            .buffer = this->normal_transform,
            .offset = 0,
            .size = sizeof(float[4][4]),
        },
    };
    auto bind_group_0_descriptor = wgpu::BindGroupDescriptor {
        .layout = layout,
        .entryCount = bind_group_0_entries.size(),
        .entries = bind_group_0_entries.data(),
    };
    return device.CreateBindGroup(&bind_group_0_descriptor);
}

void BoxGeometry::set_model_view(const wgpu::Queue& queue, glm::mat4x4 model, glm::mat4x4 view) {
    auto model_view = view * model;
    queue.WriteBuffer(this->model_view, 0, &model_view, sizeof(model_view));

    auto normal_transform = glm::transpose(glm::inverse(model));
    queue.WriteBuffer(this->normal_transform, 0, &normal_transform, sizeof(normal_transform));
}

DrawParameters BoxGeometry::draw_parameters() const {
    return DrawParametersIndexless {
        .vertex_buffer = nullptr,
        .vertex_count = 3,
    };
}
