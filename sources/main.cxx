#include <GLFW/glfw3.h>
#include <dawn/webgpu_cpp_print.h>
#include <fmt/ostream.h>
#include <glm/ext.hpp>
#include <glm/gtc/color_space.hpp>
#include <span>
#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>

#include "camera/perspective.hxx"
#include "entity.hxx"
#include "geometry/box.hxx"
#include "geometry/model.hxx"
#include "log.hxx"
#include "material/color.hxx"
#include "material/uv_debug.hxx"
#include "scene.hxx"
#include "swapchain.hxx"
#include "texture_blitter.hxx"

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

template <class T>
    requires std::is_integral_v<T> && std::is_unsigned_v<T>
static inline T div_ceil(T x, T y) {
    return (x + y - 1) / y;
}

const std::string_view POSTPROCESS_SHADER_CODE = R"(

@group(0) @binding(0) var input_texture_color: texture_2d<f32>;
@group(0) @binding(1) var input_texture_depth: texture_depth_2d;

@group(1) @binding(0) var output_texture: texture_storage_2d<rgba8unorm, write>;

@group(2) @binding(0) var<uniform> screen_extend: vec2<u32>;
@group(2) @binding(1) var<uniform> srgb_output: u32;

@compute @workgroup_size(16, 16, 1) fn main(@builtin(global_invocation_id) id: vec3<u32>) {
    let input_depth: f32 = textureLoad(input_texture_depth, id.xy, 0);
    let input_color: vec4<f32> = textureLoad(input_texture_color, id.xy, 0);

    let bottom_color = vec4<f32>(0.08021982031446832, 0.11697066775851084, 0.21586050011389926, 1.0);
    let top_color = vec4<f32>(0.05126945837404324, 0.11697066775851084, 0.35153259950043936, 1.0);
    let background_color: vec4<f32> = mix(
        top_color,
        bottom_color,
        f32(id.y) / f32(screen_extend.y),
    );
    let output_color: vec4<f32> = select(input_color, background_color, input_depth == 1.0);

    if (srgb_output == 1) {
        textureStore(output_texture, id.xy, output_color);
    } else {
        textureStore(
            output_texture,
            id.xy,
            vec4<f32>(
                pow(output_color.r, 1.0 / 2.2),
                pow(output_color.g, 1.0 / 2.2),
                pow(output_color.b, 1.0 / 2.2),
                output_color.a,
            ),
        );
    }
}

)";

class Postprocessor {
    wgpu::Device device;
    wgpu::Queue queue;

    Canvas input_canvas;
    Canvas output_canvas;

    /// Input textures.
    wgpu::BindGroup bind_group_0;
    /// Output textures.
    wgpu::BindGroup bind_group_1;
    /// Other bindings.
    wgpu::BindGroup bind_group_2;

    wgpu::Buffer uniform_screen_extend;
    wgpu::Buffer uniform_srgb_output;

    wgpu::ComputePipeline pipeline;

    TextureBlitter blitter;
    wgpu::TextureFormat previous_output_format = wgpu::TextureFormat::Undefined;

  public:
    Postprocessor() = default;

    Postprocessor(
        wgpu::Device device,
        wgpu::Queue queue,
        uint32_t width,
        uint32_t height,
        bool srgb_output
    )
        : device(std::move(device))
        , queue(std::move(queue)) {
        this->input_canvas = Canvas(
            this->device,
            {
                .width = width,
                .height = height,
                .color_format = wgpu::TextureFormat::RGBA16Float,
                .create_depth_stencil_texture = true,
                .depth_stencil_format = wgpu::TextureFormat::Depth32Float,
                .texture_usages = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                                  wgpu::TextureUsage::RenderAttachment |
                                  wgpu::TextureUsage::TextureBinding,
            }
        );
        this->output_canvas = Canvas(
            this->device,
            {
                .width = width,
                .height = height,
                .color_format = wgpu::TextureFormat::RGBA8Unorm,
                .create_depth_stencil_texture = false,
                .texture_usages = wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::CopyDst |
                                  wgpu::TextureUsage::StorageBinding |
                                  wgpu::TextureUsage::TextureBinding,
            }
        );

        auto uniform_screen_extend_descriptor = wgpu::BufferDescriptor {
            .label = "screen_extend",
            .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
            .size = sizeof(glm::uvec2),
        };
        this->uniform_screen_extend = this->device.CreateBuffer(&uniform_screen_extend_descriptor);

        auto screen_extend = glm::uvec2(this->output_canvas.width, this->output_canvas.height);
        this->queue
            .WriteBuffer(this->uniform_screen_extend, 0, &screen_extend, sizeof(screen_extend));

        auto uniform_srgb_output_descriptor = wgpu::BufferDescriptor {
            .label = "srgb_output",
            .usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst,
            .size = sizeof(uint32_t),
        };
        this->uniform_srgb_output = this->device.CreateBuffer(&uniform_srgb_output_descriptor);

        auto srgb_output_ = (uint32_t)srgb_output;
        this->queue
            .WriteBuffer(this->uniform_screen_extend, 0, &srgb_output_, sizeof(srgb_output_));

        auto input_texture_formats = std::array {
            this->input_canvas.format.color_format,
            this->input_canvas.format.depth_stencil_format,
        };
        auto bind_group_0_layout = create_texture_bind_group_layout(
            this->device,
            input_texture_formats,
            true,
            "Postprocessor input"sv
        );
        auto input_texture_views = std::array {
            this->input_canvas.color_texture_view,
            this->input_canvas.depth_stencil_texture_view,
        };
        assert(this->input_canvas.color_texture_view != nullptr);
        assert(this->input_canvas.depth_stencil_texture_view != nullptr);
        this->bind_group_0 = create_texture_bind_group(
            this->device,
            bind_group_0_layout,
            input_texture_views,
            "Postprocessor input"sv
        );

        auto output_texture_formats = std::array {
            this->output_canvas.format.color_format,
        };
        auto output_texture_views = std::array {
            this->output_canvas.color_texture_view,
        };
        auto bind_group_1_layout = create_texture_bind_group_layout(
            this->device,
            output_texture_formats,
            false,
            "Postprocessor output"sv
        );
        this->bind_group_1 = create_texture_bind_group(
            this->device,
            bind_group_1_layout,
            output_texture_views,
            "Postprocessor output"sv
        );

        auto bind_group_2_layout_entries = std::array {
            wgpu::BindGroupLayoutEntry {
                .binding = 0,
                .visibility = wgpu::ShaderStage::Compute,
                .buffer =
                    wgpu::BufferBindingLayout {
                        .type = wgpu::BufferBindingType::Uniform,
                        .hasDynamicOffset = false,
                        .minBindingSize = sizeof(glm::uvec2),
                    },
            },
            wgpu::BindGroupLayoutEntry {
                .binding = 1,
                .visibility = wgpu::ShaderStage::Compute,
                .buffer =
                    wgpu::BufferBindingLayout {
                        .type = wgpu::BufferBindingType::Uniform,
                        .hasDynamicOffset = false,
                        .minBindingSize = sizeof(uint32_t),
                    },
            },
        };
        auto bind_group_2_layout_descriptor = wgpu::BindGroupLayoutDescriptor {
            .label = "Postprocessor uniforms"sv,
            .entryCount = bind_group_2_layout_entries.size(),
            .entries = bind_group_2_layout_entries.data(),
        };
        auto bind_group_2_layout =
            this->device.CreateBindGroupLayout(&bind_group_2_layout_descriptor);
        auto bind_group_2_entries = std::array {
            wgpu::BindGroupEntry {
                .binding = 0,
                .buffer = this->uniform_screen_extend,
                .offset = 0,
                .size = this->uniform_screen_extend.GetSize(),
            },
            wgpu::BindGroupEntry {
                .binding = 1,
                .buffer = this->uniform_srgb_output,
                .offset = 0,
                .size = this->uniform_srgb_output.GetSize(),
            },
        };
        auto bind_group_2_descriptor = wgpu::BindGroupDescriptor {
            .label = "Postprocessor uniforms"sv,
            .layout = bind_group_2_layout,
            .entryCount = bind_group_2_entries.size(),
            .entries = bind_group_2_entries.data(),
        };
        this->bind_group_2 = this->device.CreateBindGroup(&bind_group_2_descriptor);

        auto bind_group_layouts = std::array {
            bind_group_0_layout,
            bind_group_1_layout,
            bind_group_2_layout,
        };
        auto pipeline_layout_descriptor = wgpu::PipelineLayoutDescriptor {
            .label = "Postprocessor"sv,
            .bindGroupLayoutCount = bind_group_layouts.size(),
            .bindGroupLayouts = bind_group_layouts.data(),
        };
        auto pipeline_layout = this->device.CreatePipelineLayout(&pipeline_layout_descriptor);

        auto shader_source = wgpu::ShaderSourceWGSL({
            .nextInChain = nullptr,
            .code = wgpu::StringView(POSTPROCESS_SHADER_CODE),
        });
        auto shader_module_descriptor = wgpu::ShaderModuleDescriptor {
            .nextInChain = &shader_source,
            .label = "Postprocessor"sv,
        };
        auto shader_module = this->device.CreateShaderModule(&shader_module_descriptor);

        auto bind_groups = std::array {
            this->bind_group_0,
            this->bind_group_1,
        };
        auto compute_state = wgpu::ComputeState {
            .module = shader_module,
            .entryPoint = "main"sv,
            .constantCount = 0,
            .constants = nullptr,
        };
        auto pipeline_descriptor = wgpu::ComputePipelineDescriptor {
            .label = "Postprocessor"sv,
            .layout = pipeline_layout,
            .compute = compute_state,
        };
        this->pipeline = this->device.CreateComputePipeline(&pipeline_descriptor);
    }

    Canvas get_input_canvas() const {
        return this->input_canvas;
    }

    void run_postprocess_onto(Canvas result_canvas) {
        if (this->previous_output_format != result_canvas.format.color_format) {
            this->blitter = TextureBlitter(
                this->device,
                this->queue,
                {
                    .src_format = wgpu::TextureFormat::RGBA8Unorm,
                    .dst_format = result_canvas.format.color_format,
                    .width = this->output_canvas.width,
                    .height = this->output_canvas.height,
                }
            );
            this->previous_output_format = result_canvas.format.color_format;
        }

        auto encoder = this->device.CreateCommandEncoder();

        auto compute_pass = encoder.BeginComputePass();
        compute_pass.SetPipeline(this->pipeline);
        compute_pass.SetBindGroup(0, this->bind_group_0);
        compute_pass.SetBindGroup(1, this->bind_group_1);
        compute_pass.SetBindGroup(2, this->bind_group_2);
        compute_pass.DispatchWorkgroups(
            div_ceil(this->output_canvas.width, 16u),
            div_ceil(this->output_canvas.height, 16u)
        );
        compute_pass.End();

        this->blitter.blit(
            encoder,
            this->output_canvas.color_texture_view,
            result_canvas.color_texture_view
        );

        auto command_buffer = encoder.Finish();
        this->queue.Submit(1, &command_buffer);
    }

  private:
    static wgpu::BindGroupLayout create_texture_bind_group_layout(
        const wgpu::Device& device,
        std::span<wgpu::TextureFormat> texture_formats,
        bool is_input,
        wgpu::StringView label = {}
    ) {
        auto layout_entries = std::vector<wgpu::BindGroupLayoutEntry> {};
        layout_entries.reserve(texture_formats.size());
        uint32_t binding_index = 0;
        for (auto& format : texture_formats) {
            if (!is_input) {
                layout_entries.push_back(wgpu::BindGroupLayoutEntry {
                    .binding = binding_index,
                    .visibility = wgpu::ShaderStage::Compute,
                    .storageTexture =
                        wgpu::StorageTextureBindingLayout {
                            .access = wgpu::StorageTextureAccess::WriteOnly,
                            .format = format,
                            .viewDimension = wgpu::TextureViewDimension::e2D,
                        },
                });
            } else {
                wgpu::TextureSampleType sample_type;
                switch (format) {
                case wgpu::TextureFormat::Depth16Unorm:
                case wgpu::TextureFormat::Depth24Plus:
                case wgpu::TextureFormat::Depth24PlusStencil8:
                case wgpu::TextureFormat::Depth32Float:
                case wgpu::TextureFormat::Depth32FloatStencil8: {
                    sample_type = wgpu::TextureSampleType::Depth;
                } break;
                default: {
                    sample_type = wgpu::TextureSampleType::Float;
                } break;
                }
                layout_entries.push_back(wgpu::BindGroupLayoutEntry {
                    .binding = binding_index,
                    .visibility = wgpu::ShaderStage::Compute,
                    .texture =
                        wgpu::TextureBindingLayout {
                            .sampleType = sample_type,
                            .viewDimension = wgpu::TextureViewDimension::e2D,
                            .multisampled = false,
                        },
                });
            }
            ++binding_index;
        }
        auto layout_descriptor = wgpu::BindGroupLayoutDescriptor {
            .label = label,
            .entryCount = layout_entries.size(),
            .entries = layout_entries.data(),
        };
        auto bind_group_layout = device.CreateBindGroupLayout(&layout_descriptor);
        return bind_group_layout;
    }

    static wgpu::BindGroup create_texture_bind_group(
        const wgpu::Device& device,
        wgpu::BindGroupLayout layout,
        std::span<wgpu::TextureView> texture_views,
        wgpu::StringView label = {}
    ) {
        auto entries = std::vector<wgpu::BindGroupEntry> {};
        entries.reserve(texture_views.size());
        uint32_t binding_index = 0;
        for (auto& texture_view : texture_views) {
            entries.push_back(wgpu::BindGroupEntry {
                .binding = binding_index,
                .textureView = texture_view,
            });
            ++binding_index;
        }
        auto descriptor = wgpu::BindGroupDescriptor {
            .label = label,
            .layout = layout,
            .entryCount = entries.size(),
            .entries = entries.data(),
        };
        auto bind_group = device.CreateBindGroup(&descriptor);
        return bind_group;
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

    EntityId entity0;
    EntityId entity1;
    EntityId entity2;

    GLFWwindow* window;

    bool needs_resize = false;

    Postprocessor postprocessor;

    void run() {
        this->initialize_wgpu();
        this->initialize_window_and_swapchain();
        this->initialize_postprocessor();
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
        wgpu::AdapterInfo adapter_info;
        this->adapter.GetInfo(&adapter_info);
        log_info("GPU: {}", fmt::streamed(adapter_info.description));

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

    void initialize_window_and_swapchain() {
        if (!glfwInit()) {
            log_error("GLFW initialization error");
            abort();
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#if defined(__EMSCRIPTEN__)
        auto init_width = (uint32_t)web_init_width();
        auto init_height = (uint32_t)web_init_height();
#else
        auto init_width = 960;
        auto init_height = 540;
#endif
        this->window =
            glfwCreateWindow(init_width, init_height, "TBN Engine Demo", nullptr, nullptr);

        this->swapchain = Swapchain(
            this->instance,
            this->adapter,
            this->device,
            this->window,
            Swapchain::CreateInfo {
                .create_depth_stencil_texture = false,
                .prefer_srgb = false,
                .prefer_float = false,
            }
        );

        glfwSetWindowUserPointer(this->window, this);
        glfwSetWindowSizeCallback(this->window, Application::window_resize_callback);
        glfwSetFramebufferSizeCallback(this->window, Application::framebuffer_resize_callback);
    }

    void initialize_scene() {
        this->scene =
            Scene(this->device, this->queue, this->postprocessor.get_input_canvas().format);

        this->camera = std::make_shared<PerspectiveCamera>();
        this->camera->position = glm::vec3(0, 0, 100);
        this->camera->direction = glm::normalize(glm::vec3(0, 0., -1));

        this->scene.set_camera(this->camera);

        auto light_position = glm::vec3(400, 400, -400);
        auto model0 = Model<uint32_t>::from_glb_file("assets/models/ico_sphere.glb");
        assert(model0.check_indices_all_in_bounds());
        auto geometry0 = std::make_shared<ModelGeometry>(this->device, this->queue, model0);
        auto material0 =
            std::make_shared<ColorMaterial>(this->device, this->queue, srgb(0.3, 0.6, 0.7));
        material0->update_light_position(this->queue, light_position);
        this->entity0 = this->scene.create_entity(geometry0, material0);

        auto geometry1 = std::make_shared<BoxGeometry>(this->device, this->queue);
        auto material1 = std::make_shared<UvDebugMaterial>();
        this->entity1 = this->scene.create_entity(geometry1, material1);

        auto model2 = Model<uint32_t>::from_glb_file("assets/models/cat.glb");
        assert(model2.check_indices_all_in_bounds());
        auto geometry2 = std::make_shared<ModelGeometry>(this->device, this->queue, model2);
        auto material2 =
            std::make_shared<ColorMaterial>(this->device, this->queue, srgb(0.8, 0.8, 0.8));
        material2->update_light_position(this->queue, light_position);
        this->entity2 = this->scene.create_entity(geometry2, material2);
    }

    void initialize_postprocessor() {
        this->postprocessor = Postprocessor(
            this->device,
            this->queue,
            this->swapchain.get_width(),
            this->swapchain.get_height(),
            format_is_srgb(this->swapchain.get_format().color_format)
        );
    }

    void draw_frame() {
        if (needs_resize) {
            uint32_t width;
            uint32_t height;
            glfwGetFramebufferSize(this->window, (int32_t*)&width, (int32_t*)&height);
            this->swapchain.reconfigure_for_size(width, height);
            this->postprocessor = Postprocessor(
                this->device,
                this->queue,
                this->swapchain.get_width(),
                this->swapchain.get_height(),
                format_is_srgb(this->swapchain.get_format().color_format)
            );
        }

        double tau = glm::tau<double>();
        double t = unix_seconds();
        {
            double period = 6.0;
            float rotation = (float)fmod(t * tau / period, tau);

            auto size = glm::vec3(40, 40, 40);
            auto position = glm::vec3(-70, 0, 0);
            auto model = glm::identity<glm::mat4x4>();

            model = glm::translate(model, position);
            model = glm::rotate(model, rotation - glm::pi<float>(), glm::vec3(1, 0, 0));
            model = glm::rotate(model, rotation, glm::vec3(0, 1, 0));
            model = glm::scale(model, size);

            this->scene.get_entity(this->entity0).set_model(model);
        }

        {
            double period = 3.0;
            float rotation = (float)fmod(t * tau / period, tau);

            auto size = glm::vec3(30, 30, 30);
            auto position = glm::vec3(70, 0, 20);
            auto model = glm::identity<glm::mat4x4>();

            model = glm::translate(model, position);
            model = glm::rotate(model, rotation - glm::pi<float>(), glm::vec3(1, 0, 0));
            model = glm::rotate(model, rotation, glm::vec3(0, 1, 0));
            model = glm::translate(model, -0.5f * size);
            model = glm::scale(model, size);

            this->scene.get_entity(this->entity1).set_model(model);
        }

        {
            double period = 5.0;
            float rotation = (float)fmod(t * tau / period, tau);

            auto size = glm::vec3(8, 8, 8);
            auto position = glm::vec3(0, -24, 0);
            auto model = glm::identity<glm::mat4x4>();

            model = glm::translate(model, position);
            model = glm::rotate(model, rotation, glm::vec3(0, 1, 0));
            model = glm::scale(model, size);

            this->scene.get_entity(this->entity2).set_model(model);
        }

        auto input_canvas = this->postprocessor.get_input_canvas();
        this->scene.draw(input_canvas);

        this->postprocessor.run_postprocess_onto(this->swapchain.get_current_canvas());
    }

    static void window_resize_callback(GLFWwindow*, int32_t, int32_t) {}

    static void framebuffer_resize_callback(GLFWwindow* window, int32_t, int32_t) {
        auto this_ = (Application*)glfwGetWindowUserPointer(window);
        this_->needs_resize = true;
    }
};

int main() {
    log_level_scope(LogLevel::Verbose, [&] {
        auto application = Application {};
        application.run();
    });
}
