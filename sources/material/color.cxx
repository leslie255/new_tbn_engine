#include "color.hxx"

using namespace std::literals;

ColorMaterial::ColorMaterial(
    const wgpu::Device& device,
    const wgpu::Queue& queue,
    glm::vec3 fill_color) {
    auto buffer_descriptor = wgpu::BufferDescriptor {
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = sizeof(float[4][4]),
        .mappedAtCreation = false,
    };
    // They're all 4x4 matrices so can share the same descriptor.
    this->color = device.CreateBuffer(&buffer_descriptor);
    this->view_position = device.CreateBuffer(&buffer_descriptor);
    this->light_position = device.CreateBuffer(&buffer_descriptor);

    queue.WriteBuffer(this->color, 0, &fill_color, sizeof(fill_color));

    // Zero-initialise positions for sanity sake.
    auto vec3_zero = glm::vec3(0, 0, 0);
    queue.WriteBuffer(this->view_position, 0, &vec3_zero, sizeof(glm::vec3));
    queue.WriteBuffer(this->light_position, 0, &vec3_zero, sizeof(glm::vec3));
}

void ColorMaterial::set_color(const wgpu::Queue& queue, glm::vec3 value) {
    queue.WriteBuffer(this->color, 0, &value, sizeof(value));
}

void ColorMaterial::set_view_position(const wgpu::Queue& queue, glm::vec3 value) {
    queue.WriteBuffer(this->view_position, 0, &value, sizeof(value));
}

void ColorMaterial::set_light_position(const wgpu::Queue& queue, glm::vec3 value) {
    queue.WriteBuffer(this->light_position, 0, &value, sizeof(value));
}

static std::string_view SHADER_CODE = R"(

struct VertexOut {
    @builtin(position) position_clip: vec4<f32>,
    @location(0) position_world: vec3<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) normal: vec3<f32>,
};

@group(2) @binding(0) var<uniform> fill_color: vec3<f32>;
@group(2) @binding(1) var<uniform> view_position: vec3<f32>;
@group(2) @binding(2) var<uniform> light_position: vec3<f32>;

@fragment fn fs_main(input: VertexOut) -> @location(0) vec4<f32> {
    let normal = normalize(input.normal);
    let light_direction = normalize(light_position - input.position_world);
    let view_direction = normalize(view_position - input.position_world);

    // FIXME: parameterize parameters instead of hard coding them.
    let ambient_strength = 0.2;
    let diffuse_strength = 0.8;
    let specular_strength = 0.2;
    let specular_intensity = 64.0;
    let light_color = vec3<f32>(1.0, 1.0, 1.0);

    let ambient_term = ambient_strength * fill_color;

    let diffuse_term = diffuse_strength * (0.5 * dot(normal, light_direction) + 0.5) * fill_color;

    let reflection_direction = reflect(-light_direction, normal);
    var specular_factor = dot(view_direction, reflection_direction);
    specular_factor = max(specular_factor, 0.0);
    specular_factor = pow(specular_factor, specular_intensity);
    let specular_term = specular_strength * specular_factor * light_color;

    let color = ambient_term + diffuse_term + specular_term;
    return vec4<f32>(color, 1.0);
}

)";

ShaderInfo ColorMaterial::create_fragment_shader(const wgpu::Device& device) const {
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

wgpu::BindGroupLayout ColorMaterial::create_bind_group_layout(const wgpu::Device& device) const {
    auto entries = std::array {
        wgpu::BindGroupLayoutEntry {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Fragment,
            .buffer =
                wgpu::BufferBindingLayout {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(glm::vec3),
                },
        },
        wgpu::BindGroupLayoutEntry {
            .binding = 1,
            .visibility = wgpu::ShaderStage::Fragment,
            .buffer =
                wgpu::BufferBindingLayout {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(glm::vec3),
                },
        },
        wgpu::BindGroupLayoutEntry {
            .binding = 2,
            .visibility = wgpu::ShaderStage::Fragment,
            .buffer =
                wgpu::BufferBindingLayout {
                    .type = wgpu::BufferBindingType::Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = sizeof(glm::vec3),
                },
        },
    };
    auto descriptor = wgpu::BindGroupLayoutDescriptor {
        .label = "Color Material"sv,
        .entryCount = entries.size(),
        .entries = entries.data(),
    };
    return device.CreateBindGroupLayout(&descriptor);
}

wgpu::BindGroup ColorMaterial::create_bind_group(
    const wgpu::Device& device,
    wgpu::BindGroupLayout layout) const {
    auto entries = std::array {
        wgpu::BindGroupEntry {
            .binding = 0,
            .buffer = this->color,
            .offset = 0,
            .size = sizeof(glm::vec3),
        },
        wgpu::BindGroupEntry {
            .binding = 1,
            .buffer = this->view_position,
            .offset = 0,
            .size = sizeof(glm::vec3),
        },
        wgpu::BindGroupEntry {
            .binding = 2,
            .buffer = this->light_position,
            .offset = 0,
            .size = sizeof(glm::vec3),
        },
    };
    auto descriptor = wgpu::BindGroupDescriptor {
        .label = "Color Material"sv,
        .layout = layout,
        .entryCount = entries.size(),
        .entries = entries.data(),
    };
    return device.CreateBindGroup(&descriptor);
}
