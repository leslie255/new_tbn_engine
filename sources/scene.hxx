#pragma once

#include <webgpu/webgpu_cpp.h>

#include "camera/base.hxx"
#include "entity.hxx"

struct EntityId {
    size_t index;
};

class Scene {
    wgpu::Device device = nullptr;
    wgpu::Queue queue = nullptr;

    wgpu::TextureFormat surface_color_format = wgpu::TextureFormat::Undefined;
    wgpu::TextureFormat surface_depth_stencil_format = wgpu::TextureFormat::Undefined;

    wgpu::BindGroupLayout camera_bind_group_layout = nullptr;
    wgpu::BindGroup camera_bind_group = nullptr;
    wgpu::Buffer projection_uniform = nullptr;

    /// Nullable.
    /// When null, use identity as projection and view.
    std::shared_ptr<CameraBase> camera = nullptr;

    /// Each entity is nullable for deletion.
    std::vector<Entity> entities = {};

  public:
    Scene() = default;

    Scene(
        wgpu::Device device,
        wgpu::Queue queue,
        wgpu::TextureFormat surface_color_format,
        wgpu::TextureFormat surface_depth_stencil_format
    );

    void set_camera(std::shared_ptr<CameraBase> camera);

    EntityId create_entity(
        std::shared_ptr<GeometryBase> geometry,
        std::shared_ptr<MaterialBase> material
    );
    Entity& get_entity(EntityId id);
    void delete_entity(EntityId id);

    struct DrawInfo {
        /// Must be non-null and match this scene's `surface_color_format`.
        wgpu::TextureView color_texture;

        /// Must be non-null and match this scene's `surface_depth_stencil_format`.
        wgpu::TextureView depth_stencil_texture;

        /// Must be non-zero.
        uint32_t frame_width;

        /// Must be non-zero.
        uint32_t frame_height;

        std::optional<glm::vec3> clear_color = std::nullopt;
    };

    void draw(const DrawInfo& options);
};
