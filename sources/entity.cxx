#include "entity.hxx"

Entity::Entity(nullptr_t) {}

bool Entity::operator==(nullptr_t) {
    return this->pipeline == nullptr;
}

Entity::Entity(
    const wgpu::Device& device,
    const wgpu::Queue& queue,
    wgpu::TextureFormat surface_color_format,
    wgpu::TextureFormat surface_depth_stencil_format,
    wgpu::BindGroupLayout camera_bind_group_layout,
    std::shared_ptr<GeometryBase> geometry,
    std::shared_ptr<MaterialBase> material
)
    : geometry(geometry)
    , material(material) {
    (void)queue;
    // Bind group layouts.
    auto geometry_bind_group_layout = geometry->create_bind_group_layout(device);
    auto material_bind_group_layout = material->create_bind_group_layout(device);
    auto bind_group_layouts = std::array {
        camera_bind_group_layout,
        geometry_bind_group_layout,
        material_bind_group_layout,
    };

    // Pipeline Layout.
    auto pipeline_layout_descriptor = wgpu::PipelineLayoutDescriptor {
        .bindGroupLayoutCount = bind_group_layouts.size(),
        .bindGroupLayouts = bind_group_layouts.data(),
    };
    auto pipeline_layout = device.CreatePipelineLayout(&pipeline_layout_descriptor);

    // Pipeline.
    auto vertex_shader = geometry->create_vertex_shader(device);
    auto vertex_buffer_layouts = geometry->vertex_buffer_layouts();
    auto vertex_state = wgpu::VertexState {
        .module = vertex_shader.shader_module,
        .entryPoint = wgpu::StringView(vertex_shader.entry_point),
        .constantCount = vertex_shader.constants.size(),
        .constants = vertex_shader.constants.data(),
        .bufferCount = vertex_buffer_layouts.size(),
        .buffers = vertex_buffer_layouts.data(),
    };
    auto color_target_state = wgpu::ColorTargetState {
        .format = surface_color_format,
        .blend = nullptr,
        .writeMask = wgpu::ColorWriteMask::All,
    };
    auto depth_stencil_state = wgpu::DepthStencilState {
        .format = surface_depth_stencil_format,
        .depthWriteEnabled = true,
        .depthCompare = wgpu::CompareFunction::Less,
    };
    auto fragment_shader = material->create_fragment_shader(device);
    auto fragment_state = wgpu::FragmentState {
        .module = fragment_shader.shader_module,
        .entryPoint = wgpu::StringView(fragment_shader.entry_point),
        .constantCount = fragment_shader.constants.size(),
        .constants = fragment_shader.constants.data(),
        .targetCount = 1,
        .targets = &color_target_state,
    };
    auto pipeline_descriptor = wgpu::RenderPipelineDescriptor {
        .layout = pipeline_layout,
        .vertex = vertex_state,
        .primitive = geometry->primitive_state(),
        .depthStencil = &depth_stencil_state,
        .fragment = &fragment_state,
    };

    this->pipeline = device.CreateRenderPipeline(&pipeline_descriptor);
    this->geometry_bind_group = geometry->create_bind_group(device, geometry_bind_group_layout);
    this->material_bind_group = material->create_bind_group(device, material_bind_group_layout);
}

void Entity::set_model(glm::mat4x4 model_matrix) {
    this->model_matrix = model_matrix;
}

void Entity::prepare_for_drawing(const wgpu::Queue& queue, glm::vec3 view_position, glm::mat4x4 view_matrix) {
    this->material->update_view_position(queue, view_position);
    this->geometry->set_model_view(queue, this->model_matrix, view_matrix);
}

void Entity::draw_commands(wgpu::RenderPassEncoder& render_pass) {
    render_pass.SetPipeline(this->pipeline);
    render_pass.SetBindGroup(1, this->geometry_bind_group);
    render_pass.SetBindGroup(2, this->material_bind_group);
    auto draw_parameters = geometry->draw_parameters();
    if (const auto* parameters = std::get_if<DrawParametersIndexless>(&draw_parameters)) {
        if (parameters->vertex_buffer != nullptr) {
            render_pass.SetVertexBuffer(0, parameters->vertex_buffer);
        }
        render_pass.Draw(
            parameters->vertex_count,
            parameters->instance_count,
            parameters->first_vertex,
            parameters->first_instance
        );
    } else if (const auto* parameters = std::get_if<DrawParametersIndexed>(&draw_parameters)) {
        assert(parameters->index_buffer != nullptr);
        render_pass.SetIndexBuffer(parameters->index_buffer, parameters->index_format);
        if (parameters->vertex_buffer != nullptr) {
            render_pass.SetVertexBuffer(0, parameters->vertex_buffer);
        }
        render_pass.DrawIndexed(
            parameters->index_count,
            parameters->instance_count,
            parameters->first_index,
            parameters->base_vertex,
            parameters->first_instance
        );
    }
}
