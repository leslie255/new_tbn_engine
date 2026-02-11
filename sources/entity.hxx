#pragma once

#include <glm/ext.hpp>
#include <memory>

#include "geometry/base.hxx"
#include "material/base.hxx"

class Entity {
    std::shared_ptr<GeometryBase> geometry = nullptr;
    std::shared_ptr<MaterialBase> material = nullptr;

    wgpu::RenderPipeline pipeline = nullptr;
    wgpu::BindGroup geometry_bind_group = nullptr;
    wgpu::BindGroup material_bind_group = nullptr;

    glm::mat4x4 model_matrix = glm::identity<glm::mat4x4>();

  public:
    Entity() = default;

    Entity(nullptr_t);

    bool operator==(nullptr_t);

    Entity(
        const wgpu::Device& device,
        const wgpu::Queue& queue,
        wgpu::TextureFormat surface_color_format,
        wgpu::TextureFormat surface_depth_stencil_format,
        wgpu::BindGroupLayout camera_bind_group_layout,
        std::shared_ptr<GeometryBase> geometry,
        std::shared_ptr<MaterialBase> material
    );

    void set_model(glm::mat4x4 model_matrix);

    void prepare_for_drawing(const wgpu::Queue& queue, glm::mat4x4 view_matrix);

    void draw_commands(wgpu::RenderPassEncoder& render_pass);
};
