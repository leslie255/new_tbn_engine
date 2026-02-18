#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <filesystem>
#include <glm/ext.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <webgpu/webgpu_cpp.h>

#include "../log.hxx"
#include "base.hxx"

using std::string_view_literals::operator""sv;

template <class T>
wgpu::IndexFormat index_format_of() {}

struct alignas(16) Vertex {
    std::array<float, 3> position;
    float padding_0;
    std::array<float, 3> normal;
    float padding_1;
    std::array<float, 2> uv;
    float padding_2[2];
};

template <>
inline wgpu::IndexFormat index_format_of<uint16_t>() {
    return wgpu::IndexFormat::Uint16;
}

template <>
inline wgpu::IndexFormat index_format_of<uint32_t>() {
    return wgpu::IndexFormat::Uint32;
}

template <class T>
concept IndexType = requires {
    { index_format_of<T>() } -> std::convertible_to<wgpu::IndexFormat>;
};

template <IndexType I>
struct Model {
    std::vector<Vertex> vertices;
    std::vector<I> indices;

    inline bool check_indices_all_in_bounds() const {
        for (uint32_t index : this->indices) {
            if ((size_t)index > vertices.size()) {
                return false;
            }
        }
        return true;
    }

    static inline Model from_glb_file(const std::filesystem::path& file_path) {
        auto status = std::filesystem::status(file_path);
        if (!std::filesystem::status_known(status)) {
            log_error("error occuring loading gltf asset: file doesn't exist");
            abort();
        }
        fastgltf::Parser parser;
        auto data_buffer = fastgltf::GltfDataBuffer::FromPath(file_path);
        if (auto error = data_buffer.error(); error != fastgltf::Error::None) {
            log_error("error occuring loading gltf asset: {}", fastgltf::getErrorMessage(error));
            abort();
        }
        auto asset = parser.loadGltfBinary(data_buffer.get(), file_path);
        if (auto error = asset.error(); error != fastgltf::Error::None) {
            log_error("error occuring loading gltf asset: {}", fastgltf::getErrorMessage(error));
            abort();
        }
        return Model::from_fastgltf_asset(asset.get());
    }

    static inline Model from_gltf_file(const std::filesystem::path& file_path) {
        auto status = std::filesystem::status(file_path);
        if (!std::filesystem::status_known(status)) {
            log_error("error occuring loading gltf asset: file doesn't exist");
            abort();
        }
        fastgltf::Parser parser;
        auto data_buffer = fastgltf::GltfDataBuffer::FromPath(file_path);
        if (auto error = data_buffer.error(); error != fastgltf::Error::None) {
            log_error("error occuring loading gltf asset: {}", fastgltf::getErrorMessage(error));
            abort();
        }
        auto asset = parser.loadGltfJson(data_buffer.get(), file_path);
        if (auto error = asset.error(); error != fastgltf::Error::None) {
            log_error("error occuring loading gltf asset: {}", fastgltf::getErrorMessage(error));
            abort();
        }
        return Model::from_fastgltf_asset(asset.get());
    }

    static inline Model from_fastgltf_asset(const fastgltf::Asset& asset) {
        assert(asset.meshes.size() >= 1);
        const auto& model = asset.meshes[0];
        assert(model.primitives.size() == 1);
        const auto& primitive = model.primitives[0];
        assert(primitive.attributes.size() >= 3);
        std::optional<size_t> i_attributes_position = std::nullopt;
        std::optional<size_t> i_attributes_normal = std::nullopt;
        std::optional<size_t> i_attributes_texcoord_0 = std::nullopt;
        for (size_t i = 0; i < primitive.attributes.size(); ++i) {
            const auto& attributes = primitive.attributes[i];
            if (attributes.name == "POSITION"sv) {
                i_attributes_position = i;
            } else if (attributes.name == "NORMAL"sv) {
                i_attributes_normal = i;
            } else if (attributes.name == "TEXCOORD_0"sv) {
                i_attributes_texcoord_0 = i;
            }
        }
        assert(i_attributes_position.has_value());
        assert(i_attributes_normal.has_value());
        assert(i_attributes_texcoord_0.has_value());
        assert(primitive.indicesAccessor.has_value());
        const auto& indices_accessor = asset.accessors[primitive.indicesAccessor.value()];
        const auto& position_accessor =
            asset.accessors[primitive.attributes[i_attributes_position.value()].accessorIndex];
        const auto& normal_accessor =
            asset.accessors[primitive.attributes[i_attributes_normal.value()].accessorIndex];
        const auto& uv_accessor =
            asset.accessors[primitive.attributes[i_attributes_texcoord_0.value()].accessorIndex];
        assert(
            position_accessor.count == normal_accessor.count &&
            normal_accessor.count == uv_accessor.count
        );

        assert(indices_accessor.bufferViewIndex.has_value());
        auto indices = std::vector<I>(indices_accessor.count);

        size_t i_index = 0;
        ;
        fastgltf::iterateAccessor<uint32_t>(asset, indices_accessor, [&](uint32_t index) {
            indices[i_index] = (I)index;
            ++i_index;
        });

        auto vertices = std::vector<Vertex>(position_accessor.count);

        size_t i_position = 0;
        fastgltf::iterateAccessor<fastgltf::math::fvec3>(
            asset,
            position_accessor,
            [&](fastgltf::math::fvec3 position) {
                vertices[i_position].position[0] = position[0];
                vertices[i_position].position[1] = position[1];
                vertices[i_position].position[2] = position[2];
                ++i_position;
            }
        );

        size_t i_normal = 0;
        fastgltf::iterateAccessor<fastgltf::math::fvec3>(
            asset,
            normal_accessor,
            [&](fastgltf::math::fvec3 normal) {
                vertices[i_normal].normal[0] = normal[0];
                vertices[i_normal].normal[1] = normal[1];
                vertices[i_normal].normal[2] = normal[2];
                ++i_normal;
            }
        );

        size_t i_uv = 0;
        fastgltf::iterateAccessor<fastgltf::math::fvec2>(
            asset,
            uv_accessor,
            [&](fastgltf::math::fvec2 uv) {
                vertices[i_uv].uv[0] = uv[0];
                vertices[i_uv].uv[1] = uv[1];
                vertices[i_uv].uv[2] = uv[2];
                ++i_uv;
            }
        );

        return Model<I> {
            .vertices = vertices,
            .indices = indices,
        };
    }
};

constexpr std::string_view SHADER_CODE = R"(

@group(0) @binding(0) var<uniform> projection: mat4x4<f32>;

struct GeometryUniforms {
    model: mat4x4<f32>,
    model_view: mat4x4<f32>,
    normal_transform: mat4x4<f32>,
};

@group(1) @binding(0) var<uniform> geometry: GeometryUniforms;

struct VertexIn {
    @location(0) position: vec3<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) normal: vec3<f32>,
};

struct VertexOut {
    @builtin(position) position_clip: vec4<f32>,
    @location(0) position_world: vec3<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) normal: vec3<f32>,
};

@vertex fn main(input: VertexIn) -> VertexOut {
    var output: VertexOut;
    output.position_clip = projection * geometry.model_view * vec4(input.position, 1.0);
    output.position_world = (geometry.model * vec4(input.position, 1.0)).xyz;
    output.uv = input.uv;
    output.normal = (geometry.normal_transform * vec4(input.normal, 1.0)).xyz;

    return output;
}

)";

static auto VERTEX_ATTRIBUTES = std::array {
    wgpu::VertexAttribute {
        .format = wgpu::VertexFormat::Float32x3,
        .offset = offsetof(Vertex, position),
        .shaderLocation = 0,
    },
    wgpu::VertexAttribute {
        .format = wgpu::VertexFormat::Float32x2,
        .offset = offsetof(Vertex, uv),
        .shaderLocation = 1,
    },
    wgpu::VertexAttribute {
        .format = wgpu::VertexFormat::Float32x3,
        .offset = offsetof(Vertex, normal),
        .shaderLocation = 2,
    },
};

class ModelGeometry : public GeometryBase {
    wgpu::Buffer vertex_buffer;
    wgpu::Buffer index_buffer;
    wgpu::Buffer uniform_buffer;
    wgpu::IndexFormat index_format;
    uint32_t index_count;

    struct Uniforms {
        glm::mat4x4 model = glm::identity<glm::mat4x4>();
        glm::mat4x4 model_view = glm::identity<glm::mat4x4>();
        glm::mat4x4 normal_transform = glm::identity<glm::mat4x4>();
    };

  public:
    ModelGeometry() = default;

    template <IndexType I>
    ModelGeometry(const wgpu::Device& device, const wgpu::Queue& queue, const Model<I>& model)
        : index_format(index_format_of<I>())
        , index_count(model.indices.size()) {
        auto vertex_buffer_descriptor = wgpu::BufferDescriptor {
            .label = "ModelGeometry::vertex_buffer"sv,
            .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex,
            .size = (uint64_t)model.vertices.size() * sizeof(model.vertices[0]),
        };
        this->vertex_buffer = device.CreateBuffer(&vertex_buffer_descriptor);
        queue.WriteBuffer(
            this->vertex_buffer,
            0,
            model.vertices.data(),
            model.vertices.size() * sizeof(model.vertices[0])
        );

        auto index_buffer_descriptor = wgpu::BufferDescriptor {
            .label = "ModelGeometry::index_buffer"sv,
            .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index,
            .size = (uint64_t)model.indices.size() * sizeof(model.indices[0]),
        };
        this->index_buffer = device.CreateBuffer(&index_buffer_descriptor);
        queue.WriteBuffer(
            this->index_buffer,
            0,
            model.indices.data(),
            model.indices.size() * sizeof(model.indices[0])
        );

        auto uniform_buffer_descriptor = wgpu::BufferDescriptor {
            .label = "ModelGeometry::uniform_buffer"sv,
            .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform,
            .size = sizeof(Uniforms),
        };
        this->uniform_buffer = device.CreateBuffer(&uniform_buffer_descriptor);
        auto uniforms = Uniforms {};
        queue.WriteBuffer(this->uniform_buffer, 0, &uniforms, sizeof(uniforms));
    }

    ShaderInfo create_vertex_shader(const wgpu::Device& device) const override {
        auto wgsl = wgpu::ShaderSourceWGSL({
            .nextInChain = nullptr,
            .code = wgpu::StringView(SHADER_CODE),
        });
        auto shader_module_descriptor = wgpu::ShaderModuleDescriptor {
            .nextInChain = &wgsl,
            .label = "ModelGeometry"sv,
        };
        auto shader_module = device.CreateShaderModule(&shader_module_descriptor);
        return ShaderInfo {
            .shader_module = shader_module,
            .constants = {},
        };
    }

    std::vector<wgpu::VertexBufferLayout> vertex_buffer_layouts() const override {
        return std::vector {
            wgpu::VertexBufferLayout {
                .stepMode = wgpu::VertexStepMode::Vertex,
                .arrayStride = sizeof(Vertex),
                .attributeCount = VERTEX_ATTRIBUTES.size(),
                .attributes = VERTEX_ATTRIBUTES.data(),
            },
        };
    }

    wgpu::BindGroupLayout create_bind_group_layout(const wgpu::Device& device) const override {
        auto entries = std::array {
            wgpu::BindGroupLayoutEntry {
                .binding = 0,
                .visibility = wgpu::ShaderStage::Vertex,
                .buffer =
                    wgpu::BufferBindingLayout {
                        .type = wgpu::BufferBindingType::Uniform,
                        .hasDynamicOffset = false,
                        .minBindingSize = sizeof(Uniforms),
                    },
            },
        };
        auto descriptor = wgpu::BindGroupLayoutDescriptor {
            .label = "ModelGeometry"sv,
            .entryCount = entries.size(),
            .entries = entries.data(),
        };
        return device.CreateBindGroupLayout(&descriptor);
    }

    wgpu::BindGroup create_bind_group(const wgpu::Device& device, wgpu::BindGroupLayout layout)
        const override {
        auto entries = std::array {
            wgpu::BindGroupEntry {
                .binding = 0,
                .buffer = this->uniform_buffer,
                .offset = 0,
                .size = sizeof(Uniforms),
            },
        };
        auto descriptor = wgpu::BindGroupDescriptor {
            .label = "ModelGeometry"sv,
            .layout = layout,
            .entryCount = entries.size(),
            .entries = entries.data(),
        };
        return device.CreateBindGroup(&descriptor);
    }

    void set_model_view(const wgpu::Queue& queue, glm::mat4x4 model, glm::mat4x4 view) override {
        auto uniforms = Uniforms {
            .model = model,
            .model_view = view * model,
            .normal_transform = glm::transpose(glm::inverse(model)),
        };
        queue.WriteBuffer(this->uniform_buffer, 0, &uniforms, sizeof(uniforms));
    }

    DrawParameters draw_parameters() const override {
        return DrawParametersIndexed {
            .index_buffer = this->index_buffer,
            .index_format = this->index_format,
            .vertex_buffer = this->vertex_buffer,
            .index_count = this->index_count,
            .instance_count = 1,
            .first_index = 0,
            .base_vertex = 0,
            .first_instance = 0,
        };
    }
};
