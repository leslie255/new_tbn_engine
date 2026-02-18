#include <glm/vec2.hpp>

#include "texture_blitter.hxx"

using namespace std::literals;

const std::string_view BLIT_SHADER_CODE = R"(

@group(0) @binding(0) var input_texture: texture_2d<f32>;
@group(0) @binding(1) var<uniform> extend: vec2<u32>;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
}

@vertex fn vs_main(@builtin(vertex_index) i: u32) -> VertexOutput {
    const positions = array(
        vec2<f32>(-1.0, -1.0),
        vec2<f32>( 1.0, -1.0),
        vec2<f32>( 1.0,  1.0),
        vec2<f32>( 1.0,  1.0),
        vec2<f32>(-1.0,  1.0),
        vec2<f32>(-1.0, -1.0),
    );
    const uvs = array(
        vec2<f32>(0.0, 1.0),
        vec2<f32>(1.0, 1.0),
        vec2<f32>(1.0, 0.0),
        vec2<f32>(1.0, 0.0),
        vec2<f32>(0.0, 0.0),
        vec2<f32>(0.0, 1.0),
    );

    var output: VertexOutput;
    output.position = vec4<f32>(positions[i], 0.0, 1.0);
    output.uv = uvs[i];
    return output;
}

@fragment fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
    let coordinate = vec2<u32>(
        u32(input.uv.x * f32(extend.x)),
        u32(input.uv.y * f32(extend.y)),
    );
    return textureLoad(input_texture, coordinate, 0);
}

)";

TextureBlitter::TextureBlitter(
    wgpu::Device device,
    wgpu::Queue queue,
    const TextureBlitter::CreateInfo& info
)
    : device(std::move(device))
    , queue(std::move(queue)) {
    auto bind_group_layout_entries = std::array {
        wgpu::BindGroupLayoutEntry {
            .binding = 0,
            .visibility = wgpu::ShaderStage::Fragment,
            .texture =
                wgpu::TextureBindingLayout {
                    .sampleType = wgpu::TextureSampleType::Float,
                    .viewDimension = wgpu::TextureViewDimension::e2D,
                    .multisampled = false,
                },
        },
        wgpu::BindGroupLayoutEntry {
            .binding = 1,
            .visibility = wgpu::ShaderStage::Fragment,
            .buffer =
                wgpu::BufferBindingLayout {
                    .type = wgpu::BufferBindingType::Uniform,
                    .minBindingSize = sizeof(glm::uvec2),
                },
        }
    };
    auto bind_group_layout_descriptor = wgpu::BindGroupLayoutDescriptor {
        .entryCount = bind_group_layout_entries.size(),
        .entries = bind_group_layout_entries.data(),
    };
    this->bind_group_layout = this->device.CreateBindGroupLayout(&bind_group_layout_descriptor);

    auto pipeline_layout_descriptor = wgpu::PipelineLayoutDescriptor {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &this->bind_group_layout
    };
    auto pipeline_layout = this->device.CreatePipelineLayout(&pipeline_layout_descriptor);

    auto shader_source = wgpu::ShaderSourceWGSL({
        .nextInChain = nullptr,
        .code = wgpu::StringView(BLIT_SHADER_CODE),
    });
    auto shader_module_descriptor = wgpu::ShaderModuleDescriptor {
        .nextInChain = &shader_source,
        .label = "TextureBlitter"sv,
    };
    auto shader_module = this->device.CreateShaderModule(&shader_module_descriptor);

    auto vertexState = wgpu::VertexState {
        .module = shader_module,
        .entryPoint = "vs_main",
        .bufferCount = 0,
    };
    auto colorTarget = wgpu::ColorTargetState {
        .format = info.dst_format,
        .writeMask = wgpu::ColorWriteMask::All,
    };
    auto fragmentState = wgpu::FragmentState {
        .module = shader_module,
        .entryPoint = "fs_main",
        .targetCount = 1,
        .targets = &colorTarget,
    };
    auto primitive = wgpu::PrimitiveState {
        .topology = wgpu::PrimitiveTopology::TriangleList,
        .frontFace = wgpu::FrontFace::CCW,
        .cullMode = wgpu::CullMode::None,
    };
    auto multisample = wgpu::MultisampleState {.count = 1};
    auto pipelineDesc = wgpu::RenderPipelineDescriptor {
        .layout = pipeline_layout,
        .vertex = vertexState,
        .primitive = primitive,
        .multisample = multisample,
        .fragment = &fragmentState,
    };
    this->pipeline = this->device.CreateRenderPipeline(&pipelineDesc);

    auto buffer_descriptor = wgpu::BufferDescriptor {
        .label = "Texture Blitter Extend Uniform",
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = sizeof(glm::uvec2),
    };
    this->extend_uniform = this->device.CreateBuffer(&buffer_descriptor);

    this->resize(info.width, info.height);
}

void TextureBlitter::resize(uint32_t width, uint32_t height) {
    auto extend = glm::uvec2(width, height);
    this->queue.WriteBuffer(this->extend_uniform, 0, (const uint8_t*)&extend, sizeof(extend));
}

void TextureBlitter::blit(
    wgpu::CommandEncoder& encoder,
    wgpu::TextureView src_texture,
    wgpu::TextureView dst_texture
) const {
    auto bgEntries = std::array {
        wgpu::BindGroupEntry {
            .binding = 0,
            .textureView = src_texture,
        },
        wgpu::BindGroupEntry {
            .binding = 1,
            .buffer = this->extend_uniform,
            .offset = 0,
            .size = sizeof(uint32_t) * 2
        },
    };

    auto bind_group_descriptor = wgpu::BindGroupDescriptor {
        .layout = this->bind_group_layout,
        .entryCount = bgEntries.size(),
        .entries = bgEntries.data()
    };

    auto bind_group = device.CreateBindGroup(&bind_group_descriptor);

    auto color_attachment = wgpu::RenderPassColorAttachment {
        .view = dst_texture,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = wgpu::Color {0.0, 0.0, 0.0, 0.0},
    };
    auto render_pass_descriptor = wgpu::RenderPassDescriptor {
        .label = "Texture Blitter"sv,
        .colorAttachmentCount = 1,
        .colorAttachments = &color_attachment,
    };
    auto render_pass = encoder.BeginRenderPass(&render_pass_descriptor);

    render_pass.SetBindGroup(0, bind_group);
    render_pass.SetPipeline(this->pipeline);
    render_pass.Draw(6);
    render_pass.End();
}
