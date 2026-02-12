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
#include "material/uv_debug.hxx"
#include "scene.hxx"
#include "swapchain.hxx"

using namespace std::literals;

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
EM_JS(int32_t, web_init_width, (), { return initWidth; });
EM_JS(int32_t, web_init_height, (), { return initHeight; });
#endif

static inline double unix_seconds() {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}

static inline glm::vec3 srgb(float r, float g, float b) {
    return glm::convertSRGBToLinear(glm::vec3(r, g, b));
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

    Swapchain swapchain;

    std::shared_ptr<PerspectiveCamera> camera;

    Scene scene;

    EntityId cube0;
    EntityId cube1;

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
            this->swapchain.present();
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
                    "webgpu error type: {}, message: {}",
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
                    log_verbose("webgpu device destroyed peacefully");
                } else {
                    log_warn(
                        "webgpu device lost, reason: {}, message: {}",
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
                    log_error("webgpu request device error: {}", fmt::streamed(message));
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
#if defined(__EMSCRIPTEN__)
        auto init_width = (uint32_t)web_init_width();
        auto init_height = (uint32_t)web_init_height();
#else
        auto init_width = 720;
        auto init_height = 480;
#endif
        this->window =
            glfwCreateWindow(init_width, init_height, "TBN Engine Demo", nullptr, nullptr);

        this->swapchain = Swapchain(
            this->instance,
            this->adapter,
            this->device,
            this->window,
            Swapchain::CreateInfo {
                .create_depth_stencil_texture = true,
                .depth_stencil_format = wgpu::TextureFormat::Depth16Unorm,
            }
        );

        glfwSetWindowUserPointer(this->window, this);
        glfwSetWindowSizeCallback(this->window, Application::window_resize_callback);
        glfwSetFramebufferSizeCallback(this->window, Application::framebuffer_resize_callback);
    }

    void initialize_scene() {
        this->camera = std::make_shared<PerspectiveCamera>();
        this->camera->position = glm::vec3(0, 0, 100);
        this->camera->direction = glm::normalize(glm::vec3(0, 0., -1));

        this->scene = Scene(this->device, this->queue, this->swapchain.get_format());
        this->scene.set_camera(this->camera);

        auto light_position = glm::vec3(400, 400, -400);
        auto geometry0 = std::make_shared<BoxGeometry>(this->device, this->queue);
        auto material0 =
            std::make_shared<ColorMaterial>(this->device, this->queue, srgb(0.3, 0.6, 0.7));
        material0->update_light_position(this->queue, light_position);
        this->cube0 = this->scene.create_entity(geometry0, material0);

        auto geometry1 = std::make_shared<BoxGeometry>(this->device, this->queue);
        auto material1 = std::make_shared<UvDebugMaterial>();
        this->cube1 = this->scene.create_entity(geometry1, material1);
    }

    void draw_frame() {
        double tau = glm::tau<double>();
        double t = unix_seconds();
        {
            double period = 6.0;
            float rotation = (float)fmod(t * tau / period, tau);

            auto size = glm::vec3(60, 60, 60);
            auto position = glm::vec3(-50, 0, 0);
            auto model = glm::identity<glm::mat4x4>();

            model = glm::translate(model, position);
            model = glm::rotate(model, rotation - glm::pi<float>(), glm::vec3(1, 0, 0));
            model = glm::rotate(model, rotation, glm::vec3(0, 1, 0));
            model = glm::translate(model, -0.5f * size);
            model = glm::scale(model, size);

            this->scene.get_entity(this->cube0).set_model(model);
        }

        {
            double period = 3.0;
            float rotation = (float)fmod(t * tau / period, tau);

            auto size = glm::vec3(30, 30, 30);
            auto position = glm::vec3(50, 0, 20);
            auto model = glm::identity<glm::mat4x4>();

            model = glm::translate(model, position);
            model = glm::rotate(model, rotation - glm::pi<float>(), glm::vec3(1, 0, 0));
            model = glm::rotate(model, rotation, glm::vec3(0, 1, 0));
            model = glm::translate(model, -0.5f * size);
            model = glm::scale(model, size);

            this->scene.get_entity(this->cube1).set_model(model);
        }

        auto surface = this->swapchain.get_current_surface();
        this->scene.draw(surface);
    }

    static void window_resize_callback(GLFWwindow*, int32_t, int32_t) {}

    static void framebuffer_resize_callback(GLFWwindow* window, int32_t width, int32_t height) {
        auto this_ = (Application*)glfwGetWindowUserPointer(window);
        log_verbose("resizing surface to {}x{}", width, height);
        this_->swapchain.reconfigure_for_size((uint32_t)width, (uint32_t)height);
    }
};

int main() {
    log_level_scope(LogLevel::Verbose, [&] {
        auto application = Application {};
        application.run();
    });
}
