#pragma once

#include <webgpu/webgpu_cpp.h>

#include "camera/base.hxx"
#include "entity.hxx"
#include "canvas.hxx"

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

    Scene(wgpu::Device device, wgpu::Queue queue, CanvasFormat surface_format);

    void set_camera(std::shared_ptr<CameraBase> camera);

    EntityId create_entity(
        std::shared_ptr<GeometryBase> geometry,
        std::shared_ptr<MaterialBase> material
    );
    Entity& get_entity(EntityId id);
    void delete_entity(EntityId id);

    /// Must be a surface of the same texture format that the scene is created for.
    void draw(const Canvas& surface);
};
