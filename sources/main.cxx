#include <GLFW/glfw3.h>
#include <dawn/webgpu_cpp_print.h>
#include <fmt/ostream.h>
#include <glm/ext.hpp>
#include <glm/gtc/color_space.hpp>
#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>

#include "camera/perspective.hxx"
#include "entity.hxx"
#include "geometry/box.hxx"
#include "log.hxx"
#include "material/color.hxx"
#include "scene.hxx"
#include "surface.hxx"

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

struct Application {
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;

    WindowSurface window_surface;

    std::shared_ptr<PerspectiveCamera> camera;

    Scene scene;

    std::shared_ptr<BoxGeometry> geometry;
    std::shared_ptr<ColorMaterial> material;

    EntityId entity;

    GLFWwindow* window;

    void run() {
        this->initialize_wgpu();
        this->initialize_window_and_surface();
        this->initialize_scene();

#if defined(__EMSCRIPTEN__)
        emscripten_set_main_loop_arg(emscripten_main_loop, this, 0, true);
#else
        while (!glfwWindowShouldClose(this->window)) {
            glfwPollEvents();
            this->draw_frame();
            this->window_surface.present();
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
            [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message
            ) {
                if (status != wgpu::RequestAdapterStatus::Success) {
                    log_error("error requesting adapter: {}", fmt::streamed(message));
                    abort();
                }
                this->adapter = std::move(adapter);
            }
        );
        this->instance.WaitAny(adapter_future, UINT64_MAX);

        // Device.
        wgpu::DeviceDescriptor device_descriptor {};
        device_descriptor.SetUncapturedErrorCallback(
            [](const wgpu::Device&, wgpu::ErrorType error_type, wgpu::StringView message) {
                log_error(
                    "webgpu (dawn) error type: {}, message: {}",
                    fmt::streamed(error_type),
                    fmt::streamed(message)
                );
                __builtin_trap();
            }
        );
        device_descriptor.SetDeviceLostCallback(
            wgpu::CallbackMode::AllowSpontaneous,
            [](const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView message) {
                if (reason == wgpu::DeviceLostReason::Destroyed) {
                    log_verbose("webgpu (dawn) device destroyed peacefully");
                } else {
                    log_warn(
                        "webgpu (dawn) device lost, reason: {}, message: {}",
                        fmt::streamed(reason),
                        fmt::streamed(message)
                    );
                }
            }
        );
        auto device_future = adapter.RequestDevice(
            &device_descriptor,
            wgpu::CallbackMode::WaitAnyOnly,
            [&](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
                if (status != wgpu::RequestDeviceStatus::Success) {
                    log_error("webgpu (dawn) request device error: {}", fmt::streamed(message));
                    __builtin_trap();
                }
                this->device = std::move(device);
            }
        );
        instance.WaitAny(device_future, UINT64_MAX);

        // Queue.
        this->queue = this->device.GetQueue();
    }

    void initialize_window_and_surface() {
        if (!glfwInit()) {
            log_error("GLFW initialization error");
            abort();
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        this->window = glfwCreateWindow(
            INIT_WINDOW_WIDTH,
            INIT_WINDOW_HEIGHT,
            "WebGPU Test",
            nullptr,
            nullptr
        );
        this->window_surface = WindowSurface(
            this->instance,
            this->adapter,
            this->device,
            this->window,
            WindowSurface::CreateInfo {
                .create_depth_stencil_texture = true,
                .depth_stencil_format = wgpu::TextureFormat::Depth16Unorm,
            }
        );
        glfwSetWindowSizeCallback(this->window, Application::window_resize_callback);

        glfwSetWindowUserPointer(this->window, this);
    }

    void initialize_scene() {
        this->camera = std::make_shared<PerspectiveCamera>();
        this->camera->position = glm::vec3(0, 0, 100);
        this->camera->direction = glm::normalize(glm::vec3(0, 0., -1));

        this->scene = Scene(this->device, this->queue, this->window_surface.get_format());
        this->scene.set_camera(this->camera);

        // Geometry.
        this->geometry = std::make_shared<BoxGeometry>(this->device, this->queue);

        // Material.
        this->material = std::make_shared<ColorMaterial>(
            this->device,
            this->queue,
            glm::convertSRGBToLinear(glm::vec3(0, 0.5, 0.5))
        );
        this->material->set_light_position(this->queue, glm::vec3(400, 400, -400));

        this->scene.create_entity(this->geometry, this->material);
    }

    void draw_frame() {
        auto surface = this->window_surface.get_current_surface();

        this->material->set_view_position(this->queue, this->camera->position);

        double period = 4.0;
        auto rotation = (float)fmod(unix_seconds() / period, glm::two_pi<double>());
        auto cube_size = glm::vec3(60, 60, 60);
        auto model = glm::identity<glm::mat4x4>();
        model = glm::rotate(model, rotation, glm::vec3(0, 1, 0)),
        model = glm::rotate(model, rotation - glm::pi<float>(), glm::vec3(1, 0, 0)),
        model = glm::translate(model, -0.5f * cube_size);
        model = glm::scale(model, cube_size);
        this->scene.get_entity(this->entity).set_model(model);

        this->scene.draw(surface);
    }

    static void window_resize_callback(GLFWwindow* window, int32_t width, int32_t height) {
        auto this_ = (Application*)glfwGetWindowUserPointer(window);
        this_->window_surface.reconfigure_for_size((uint32_t)width, (uint32_t)height);
    }
};

int main() {
    log_level_scope(LogLevel::Verbose, [&] {
        auto application = Application {};
        application.run();
    });
}
