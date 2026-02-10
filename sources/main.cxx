#include <GLFW/glfw3.h>
#include <dawn/webgpu_cpp_print.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/color_space.hpp>
#include <span>
#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>

#include "camera/perspective.hxx"
#include "geometry/box.hxx"
#include "material/color.hxx"

using namespace std::literals;

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

constexpr uint32_t INIT_WINDOW_WIDTH = 720;
constexpr uint32_t INIT_WINDOW_HEIGHT = 480;

static inline double unix_seconds() {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

struct CameraBindGroup {
    wgpu::BindGroup wgpu_bind_group;
    wgpu::BindGroupLayout layout;
    wgpu::Buffer projection;

    CameraBindGroup() = default;

    CameraBindGroup(const wgpu::Device& device, const wgpu::Queue& queue) {
        // Projection uniform buffer.
        auto buffer_descriptor = wgpu::BufferDescriptor {
            .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
            .size = sizeof(float[4][4]),
            .mappedAtCreation = false,
        };
        this->projection = device.CreateBuffer(&buffer_descriptor);

        auto identity4x4 = glm::identity<glm::mat4x4>();
        queue.WriteBuffer(this->projection, 0, &identity4x4, sizeof(identity4x4));

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
        this->layout = device.CreateBindGroupLayout(&layout_descriptor);

        auto entries = std::array {
            wgpu::BindGroupEntry {
                .binding = 0,
                .buffer = this->projection,
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
        this->wgpu_bind_group = device.CreateBindGroup(&bind_group_descriptor);
    }

    void set_projection(const wgpu::Queue& queue, glm::mat4x4 value) {
        queue.WriteBuffer(this->projection, 0, &value, sizeof(value));
    }
};

struct ObjectPipeline {
    wgpu::RenderPipeline wgpu_pipeline;
    wgpu::BindGroup geometry_bind_group;
    wgpu::BindGroup material_bind_group;

    ObjectPipeline() = default;

    template <class Geometry, class Material>
        requires is_geometry<Geometry> && is_material<Material>
    ObjectPipeline(
        const wgpu::Device& device,
        wgpu::TextureFormat surface_format,
        const CameraBindGroup& camera_bind_group,
        const Geometry& geometry,
        const Material& material) {
        // Bind group layouts.
        auto camera_bind_group_layout = camera_bind_group.layout;
        auto geometry_bind_group_layout = geometry.create_bind_group_layout(device);
        auto material_bind_group_layout = material.create_bind_group_layout(device);
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
        auto vertex_state = geometry.create_vertex_state(device);
        auto color_target_state = wgpu::ColorTargetState {
            .format = surface_format,
            .blend = nullptr,
            .writeMask = wgpu::ColorWriteMask::All,
        };
        auto depth_stencil_state = wgpu::DepthStencilState {
            .format = wgpu::TextureFormat::Depth16Unorm,
            .depthWriteEnabled = true,
            .depthCompare = wgpu::CompareFunction::Less,
        };
        auto fragment_shader = material.create_fragment_shader(device);
        auto fragment_state = wgpu::FragmentState {
            .module = fragment_shader.shader_module,
            .constantCount = fragment_shader.constants.size(),
            .constants = fragment_shader.constants.data(),
            .targetCount = 1,
            .targets = &color_target_state,
        };
        auto pipeline_descriptor = wgpu::RenderPipelineDescriptor {
            .layout = pipeline_layout,
            .vertex = vertex_state,
            .depthStencil = &depth_stencil_state,
            .fragment = &fragment_state,
        };

        this->wgpu_pipeline = device.CreateRenderPipeline(&pipeline_descriptor);
        this->geometry_bind_group = geometry.create_bind_group(device, geometry_bind_group_layout);
        this->material_bind_group = material.create_bind_group(device, material_bind_group_layout);
    }

    template <class Geometry>
        requires is_geometry<Geometry>
    void draw_commands(wgpu::RenderPassEncoder& render_pass, const Geometry& geometry) {
        render_pass.SetPipeline(this->wgpu_pipeline);
        render_pass.SetBindGroup(1, this->geometry_bind_group);
        render_pass.SetBindGroup(2, this->material_bind_group);
        auto draw_parameters = geometry.draw_parameters();
        if (const auto* parameters = std::get_if<DrawParametersIndexless>(&draw_parameters)) {
            if (parameters->vertex_buffer != nullptr) {
                render_pass.SetVertexBuffer(0, parameters->vertex_buffer);
            }
            render_pass.Draw(
                parameters->vertex_count,
                parameters->instance_count,
                parameters->first_vertex,
                parameters->first_instance);
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
                parameters->first_instance);
        }
    }
};

struct Application {
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;

    wgpu::Surface surface;
    wgpu::TextureFormat surface_format;
    wgpu::Texture depth_texture;
    wgpu::TextureView depth_texture_view;

    PerspectiveCamera camera;
    CameraBindGroup camera_bind_group;

    BoxGeometry geometry;
    ColorMaterial material;
    ObjectPipeline pipeline;

    GLFWwindow* window;

    void run() {
        this->initialize_wgpu();
        this->initialize_window();
        this->configure_surface();
        this->create_render_pipeline();

        this->camera.position = glm::vec3(0, 0, 100);
        this->camera.direction = glm::normalize(glm::vec3(0, 0., -1));

#if defined(__EMSCRIPTEN__)
        emscripten_set_main_loop_arg(emscripten_main_loop, this, 0, true);
#else
        while (!glfwWindowShouldClose(this->window)) {
            glfwPollEvents();
            this->draw_frame();
            this->surface.Present();
            this->instance.ProcessEvents();
        }
#endif
    }

    static void emscripten_main_loop(void* arg) {
        auto this_ = (Application*)arg;
        this_->draw_frame();
    }

    void initialize_wgpu() {
        // Instance.
        auto required_features = std::array {
            wgpu::InstanceFeatureName::TimedWaitAny,
        };
        auto instance_descriptor = wgpu::InstanceDescriptor {
            .requiredFeatureCount = 1,
            .requiredFeatures = required_features.data(),
        };
        this->instance = wgpu::CreateInstance(&instance_descriptor);

        // Adapter.
        auto adapter_future = instance.RequestAdapter(
            nullptr,
            wgpu::CallbackMode::WaitAnyOnly,
            [&](wgpu::RequestAdapterStatus status,
                wgpu::Adapter adapter,
                wgpu::StringView message) {
                if (status != wgpu::RequestAdapterStatus::Success) {
                    fmt::println("[ERROR] error requesting adapter: {}", fmt::streamed(message));
                    abort();
                }
                this->adapter = std::move(adapter);
            });
        this->instance.WaitAny(adapter_future, UINT64_MAX);

        // Device.
        wgpu::DeviceDescriptor device_descriptor {};
        device_descriptor.SetUncapturedErrorCallback(
            [](const wgpu::Device&, wgpu::ErrorType error_type, wgpu::StringView message) {
                fmt::println(
                    "[Error] webgpu (dawn) error type: {}, message: {}",
                    fmt::streamed(error_type),
                    fmt::streamed(message));
                __builtin_trap();
            });
        device_descriptor.SetDeviceLostCallback(
            wgpu::CallbackMode::AllowSpontaneous,
            [](const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView message) {
                if (reason == wgpu::DeviceLostReason::Destroyed) {
                    fmt::println("[VERBOSE] webgpu (dawn) device destroyed peacefully");
                } else {
                    fmt::println(
                        "[WARN] webgpu (dawn) device lost, reason: {}, message: {}",
                        fmt::streamed(reason),
                        fmt::streamed(message));
                }
            });
        auto device_future = adapter.RequestDevice(
            &device_descriptor,
            wgpu::CallbackMode::WaitAnyOnly,
            [&](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
                if (status != wgpu::RequestDeviceStatus::Success) {
                    fmt::println(
                        "[ERROR] webgpu (dawn) request device error: {}",
                        fmt::streamed(message));
                    __builtin_trap();
                }
                this->device = std::move(device);
            });
        instance.WaitAny(device_future, UINT64_MAX);

        // Queue.
        this->queue = this->device.GetQueue();
    }

    void initialize_window() {
        if (!glfwInit()) {
            fmt::println("[ERROR] GLFW initialization error");
            abort();
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        this->window = glfwCreateWindow(
            INIT_WINDOW_WIDTH,
            INIT_WINDOW_HEIGHT,
            "WebGPU Test",
            nullptr,
            nullptr);
        this->surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);
        glfwSetWindowSizeCallback(this->window, Application::window_resize_callback);

        glfwSetWindowUserPointer(this->window, this);
    }

    void configure_surface() {
        wgpu::SurfaceCapabilities capabilities;
        this->surface.GetCapabilities(this->adapter, &capabilities);
        auto supported_formats = std::span(capabilities.formats, capabilities.formatCount);
        for (wgpu::TextureFormat format :
             std::span(capabilities.formats, capabilities.formatCount)) {
            fmt::println("[VERBOSE] detected supported surface format: {}", fmt::streamed(format));
            if (format == wgpu::TextureFormat::RGBA8UnormSrgb ||
                format == wgpu::TextureFormat::BGRA8UnormSrgb) {
                this->surface_format = format;
            }
        }
        if (this->surface_format == wgpu::TextureFormat::Undefined) {
            if (supported_formats.size() == 0) {
                fmt::println(
                    "[WARN] cannot find a any supported surface format, using RGBA8UnormSrgb");
                this->surface_format = wgpu::TextureFormat::RGBA8UnormSrgb;
            } else {
                fmt::println(
                    "[WARN] cannot find a any suitable surface format, using the first availible "
                    "one instead");
                this->surface_format = capabilities.formats[0];
            }
        }
        fmt::println("[INFO] chosen surface format {}", fmt::streamed(this->surface_format));
        this->reconfigure_surface_for_resize(INIT_WINDOW_WIDTH, INIT_WINDOW_HEIGHT);
    }

    void reconfigure_surface_for_resize(uint32_t width, uint32_t height) {
        auto surface_configuration = wgpu::SurfaceConfiguration {
            .device = this->device,
            .format = this->surface_format,
            .width = width,
            .height = height,
        };
        surface.Configure(&surface_configuration);

        // Re-create depth texture.
        auto depth_texture_descriptor = wgpu::TextureDescriptor {
            .label = "Depth Texture"sv,
            .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
            .dimension = wgpu::TextureDimension::e2D,
            .size =
                wgpu::Extent3D {
                    .width = width,
                    .height = height,
                    .depthOrArrayLayers = 1,
                },
            .format = wgpu::TextureFormat::Depth16Unorm,
        };
        this->depth_texture = this->device.CreateTexture(&depth_texture_descriptor);
        this->depth_texture_view = this->depth_texture.CreateView();
    }

    void create_render_pipeline() {
        // Camera bind group.
        this->camera_bind_group = CameraBindGroup(this->device, this->queue);

        // Geometry.
        this->geometry = BoxGeometry(this->device, this->queue);

        // Material.
        this->material = ColorMaterial(
            this->device,
            this->queue,
            glm::convertSRGBToLinear(glm::vec3(0, 0.5, 0.5)));
        this->material.set_light_position(this->queue, glm::vec3(400, 400, -400));

        this->pipeline = ObjectPipeline(
            this->device,
            this->surface_format,
            this->camera_bind_group,
            this->geometry,
            this->material);
    }

    void draw_frame() {
        wgpu::SurfaceTexture surface_texture;
        surface.GetCurrentTexture(&surface_texture);
        auto surface_texture_view = surface_texture.texture.CreateView();

        int32_t width;
        int32_t height;
        glfwGetWindowSize(this->window, &width, &height);
        auto projection = this->camera.projection_matrix((float)width, (float)height);
        this->camera_bind_group.set_projection(this->queue, projection);

        auto view = this->camera.view_matrix();

        this->material.set_view_position(this->queue, this->camera.position);

        auto period = 2.0;
        auto rotation = (float)fmod(unix_seconds() / period, glm::two_pi<double>());
        auto cube_size = glm::vec3(60, 60, 60);
        auto model = glm::identity<glm::mat4x4>();
        model = glm::rotate(model, rotation, glm::vec3(0, 1, 0)),
        model = glm::rotate(model, rotation - glm::pi<float>(), glm::vec3(1, 0, 0)),
        model = glm::translate(model, -0.5f * cube_size);
        model = glm::scale(model, cube_size);
        this->geometry.set_model_view(this->queue, model, view);

        auto encoder = this->device.CreateCommandEncoder();
        auto color_attachment = wgpu::RenderPassColorAttachment {
            .view = surface_texture_view,
            .loadOp = wgpu::LoadOp::Clear,
            .storeOp = wgpu::StoreOp::Store,
            .clearValue = wgpu::Color {0.0, 0.0, 0.0, 0.0},
        };
        auto depth_stencil_attachment = wgpu::RenderPassDepthStencilAttachment {
            .view = this->depth_texture_view,
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

        render_pass.SetBindGroup(0, this->camera_bind_group.wgpu_bind_group);
        this->pipeline.draw_commands(render_pass, this->geometry);
        render_pass.End();

        auto command_buffer = encoder.Finish();
        this->device.GetQueue().Submit(1, &command_buffer);
    }

    static void window_resize_callback(GLFWwindow* window, int32_t width, int32_t height) {
        auto this_ = (Application*)glfwGetWindowUserPointer(window);
        this_->reconfigure_surface_for_resize((uint32_t)width, (uint32_t)height);
    }
};

int main() {
    auto application = Application {};
    application.run();
}
