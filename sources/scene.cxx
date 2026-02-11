#include "scene.hxx"

using namespace std::literals;

static inline wgpu::BindGroupLayout create_camera_bind_group_layout(const wgpu::Device& device) {
    // Bind group layout.
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
    };
    auto layout_descriptor = wgpu::BindGroupLayoutDescriptor {
        .label = "Camera"sv,
        .entryCount = layout_entries.size(),
        .entries = layout_entries.data(),
    };
    return device.CreateBindGroupLayout(&layout_descriptor);
}

static inline wgpu::BindGroup create_camera_bind_group(
    const wgpu::Device& device,
    wgpu::BindGroupLayout layout,
    wgpu::Buffer projection_uniform
) {
    auto entries = std::array {
        wgpu::BindGroupEntry {
            .binding = 0,
            .buffer = projection_uniform,
            .offset = 0,
            .size = sizeof(float[4][4]),
        },
    };
    auto bind_group_descriptor = wgpu::BindGroupDescriptor {
        .label = "Camera"sv,
        .layout = layout,
        .entryCount = entries.size(),
        .entries = entries.data(),
    };
    return device.CreateBindGroup(&bind_group_descriptor);
}

Scene::Scene(
    wgpu::Device device,
    wgpu::Queue queue,
    wgpu::TextureFormat surface_color_format,
    wgpu::TextureFormat surface_depth_stencil_format
)
    : device(std::move(device))
    , queue(std::move(queue))
    , surface_color_format(surface_color_format)
    , surface_depth_stencil_format(surface_depth_stencil_format) {
    // Projection uniform buffer.
    auto projection_uniform_buffer_descriptor = wgpu::BufferDescriptor {
        .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
        .size = sizeof(float[4][4]),
        .mappedAtCreation = false,
    };
    this->projection_uniform = this->device.CreateBuffer(&projection_uniform_buffer_descriptor);

    auto identity4x4 = glm::identity<glm::mat4x4>();
    this->queue.WriteBuffer(this->projection_uniform, 0, &identity4x4, sizeof(identity4x4));

    this->camera_bind_group_layout = create_camera_bind_group_layout(this->device);
    this->camera_bind_group =
        create_camera_bind_group(this->device, this->camera_bind_group_layout, projection_uniform);

    this->camera = nullptr;
    this->entities = std::vector<Entity> {};
}

void Scene::set_camera(std::shared_ptr<CameraBase> camera) {
    this->camera = camera;
}

EntityId Scene::create_entity(
    std::shared_ptr<GeometryBase> geometry,
    std::shared_ptr<MaterialBase> material
) {
    auto entity = Entity(
        this->device,
        this->queue,
        this->surface_color_format,
        this->surface_depth_stencil_format,
        this->camera_bind_group_layout,
        std::move(geometry),
        std::move(material)
    );
    this->entities.push_back(std::move(entity));
    return EntityId(this->entities.size() - 1);
}

Entity& Scene::get_entity(EntityId id) {
    assert(id.index + 1 < this.entities.size());
    auto& entity = this->entities[id.index];
    assert(entity != nullptr);
    return entity;
}

void Scene::delete_entity(EntityId id) {
    assert(id.index + 1 < this.entities.size());
    this->entities[id.index] = nullptr;
}

void Scene::draw(const DrawInfo& info) {
    assert(info.color_texture != nullptr);
    assert(info.depth_stencil_texture != nullptr);
    assert(info.frame_width != 0);
    assert(info.frame_height != 0);

    auto clear_color = glm::convertSRGBToLinear(info.clear_color.value_or(glm::vec3(0, 0, 0)));

    auto encoder = this->device.CreateCommandEncoder();
    auto color_attachment = wgpu::RenderPassColorAttachment {
        .view = info.color_texture,
        .loadOp = info.clear_color.has_value() ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = wgpu::Color {clear_color.r, clear_color.g, clear_color.b, 1.0},
    };
    auto depth_stencil_attachment = wgpu::RenderPassDepthStencilAttachment {
        .view = info.depth_stencil_texture,
        .depthLoadOp = wgpu::LoadOp::Clear,
        .depthStoreOp = wgpu::StoreOp::Store,
        .depthClearValue = 1.0,
        .depthReadOnly = false,
    };
    auto render_pass_descriptor = wgpu::RenderPassDescriptor {
        .colorAttachmentCount = 1,
        .colorAttachments = &color_attachment,
        .depthStencilAttachment = &depth_stencil_attachment,
    };
    auto render_pass = encoder.BeginRenderPass(&render_pass_descriptor);

    glm::mat4x4 view_matrix;
    glm::mat4x4 projection_matrix;
    if (this->camera != nullptr) {
        view_matrix = this->camera->view_matrix();
        projection_matrix =
            this->camera->projection_matrix((float)info.frame_width, (float)info.frame_height);
    } else {
        view_matrix = glm::identity<glm::mat4x4>();
        projection_matrix = glm::identity<glm::mat4x4>();
    }

    this->queue
        .WriteBuffer(this->projection_uniform, 0, &projection_matrix, sizeof(projection_matrix));

    render_pass.SetBindGroup(0, this->camera_bind_group);

    for (auto& entity : this->entities) {
        if (entity != nullptr) {
            entity.prepare_for_drawing(this->queue, view_matrix);
            entity.draw_commands(render_pass);
        }
    }

    render_pass.End();

    auto command_buffer = encoder.Finish();
    this->device.GetQueue().Submit(1, &command_buffer);
}
