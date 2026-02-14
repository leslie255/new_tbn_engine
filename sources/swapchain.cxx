#include <dawn/webgpu_cpp_print.h>
#include <fmt/ostream.h>
#include <span>
#include <webgpu/webgpu_glfw.h>

#include "log.hxx"
#include "swapchain.hxx"
#include "utils.hxx"

using namespace std::literals;

static inline wgpu::Texture create_depth_stencil_texture(
    const wgpu::Device& device,
    uint32_t width,
    uint32_t height,
    wgpu::TextureFormat format
) {
    auto depth_stencil_texture_descriptor = wgpu::TextureDescriptor {
        .label = "Depth-Stencil Texture"sv,
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
        .dimension = wgpu::TextureDimension::e2D,
        .size =
            wgpu::Extent3D {
                .width = width,
                .height = height,
                .depthOrArrayLayers = 1,
            },
        .format = format,
    };
    return device.CreateTexture(&depth_stencil_texture_descriptor);
}

static inline wgpu::TextureFormat find_suitable_format(
    const Swapchain::CreateInfo& info,
    std::span<wgpu::TextureFormat const> supported_formats
) {
    auto result = wgpu::TextureFormat::Undefined;
    if (get_current_log_level() <= LogLevel::Verbose) {
        auto s = std::string("");
        for (size_t i = 0; i < supported_formats.size(); ++i) {
            fmt::format_to(std::back_inserter(s), "{}", fmt::streamed(supported_formats[i]));
            if (i + 1 < supported_formats.size()) {
                s.append(", ");
            }
        }
        log_verbose("supported surface formats: {}", s);
    }
    for (auto format : supported_formats) {
        if (info.prefer_srgb && format_is_srgb(format)) {
            result = format;
            break;
        }
        if (!info.prefer_srgb && info.prefer_float && format_is_float(format)) {
            result = format;
            break;
        }
        if (!info.prefer_srgb && !info.prefer_float && !format_is_float(format)) {
            result = format;
            break;
        }
    }

    /// Safe guard against if no texture is found at all.
    if (result == wgpu::TextureFormat::Undefined) {
        if (supported_formats.size() == 0) {
            result = wgpu::TextureFormat::BGRA8Unorm;
            log_warn(
                "swapchain creation: window does not report supporting any texture format at all, trying format {} as a last resort",
                fmt::streamed(result)
            );
        } else {
            result = supported_formats[0];
            log_warn(
                "swapchain creation: no suitable texture format is found, using the first availible one instead: {}",
                fmt::streamed(result)
            );
        }
    } else if (info.prefer_srgb && !format_is_srgb(result)) {
        log_info(
            "swapchain creation: requested SRGB texture but window does not support SRGB output, using {} instead",
            fmt::streamed(result)
        );
    } else if (info.prefer_float && !format_is_float(result)) {
        log_info(
            "swapchain creation: requested float texture but window does not support SRGB output, using {} instead",
            fmt::streamed(result)
        );
    }

    return result;
}

Swapchain::Swapchain(
    const wgpu::Instance& instance,
    const wgpu::Adapter& adapter,
    wgpu::Device device,
    GLFWwindow* window,
    const CreateInfo& info
)
    : device(std::move(device)) {
    this->surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);

    glfwGetFramebufferSize(window, (int32_t*)&this->width, (int32_t*)&this->height);
    log_verbose("detected initial window size: {}x{}", this->width, this->height);
    if (this->width == 0 || this->height == 0) {
        log_warn("window size has zero pixels, using 480x320 instead");
        this->width = 480;
        this->height = 320;
    }

    wgpu::SurfaceCapabilities capabilities;
    this->surface.GetCapabilities(adapter, &capabilities);
    log_verbose("surface texture usages: {}", fmt::streamed(capabilities.usages));
    this->format.color_format = find_suitable_format(
        info,
        std::span(capabilities.formats, (size_t)capabilities.formatCount)
    );

    if (info.create_depth_stencil_texture) {
        if (info.depth_stencil_format == wgpu::TextureFormat::Undefined) {
            log_error(
                "swapchain create info malformed: if create_depth_stencil_texture is true, depth_stencil_format must be specified (i.e. not wgpu::TextureFormat::Undefined)"
            );
            abort();
        }
        this->depth_stencil_texture =
            create_depth_stencil_texture(this->device, width, height, info.depth_stencil_format);
        this->format.depth_stencil_format = info.depth_stencil_format;
    }

    auto surface_configuration = wgpu::SurfaceConfiguration {
        .device = this->device,
        .format = this->format.color_format,
        .width = width,
        .height = height,
    };
    surface.Configure(&surface_configuration);
}

uint32_t Swapchain::get_width() const {
    return this->width;
}

uint32_t Swapchain::get_height() const {
    return this->height;
}

CanvasFormat Swapchain::get_format() const {
    return this->format;
}

Canvas Swapchain::get_current_canvas() {
    wgpu::SurfaceTexture surface_texture;
    surface.GetCurrentTexture(&surface_texture);
    auto color_texture_view = surface_texture.texture.CreateView();
    auto depth_stencil_texture_view = this->depth_stencil_texture == nullptr
                                          ? wgpu::TextureView(nullptr)
                                          : this->depth_stencil_texture.CreateView();

    Canvas surface;
    surface.width = this->width;
    surface.height = this->height;

    surface.format = this->format;

    surface.color_texture = surface_texture;
    surface.color_texture_view = color_texture_view;

    surface.depth_stencil_texture = this->depth_stencil_texture;
    surface.depth_stencil_texture_view = depth_stencil_texture_view;

    return surface;
}

void Swapchain::reconfigure_for_size(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        log_warn(
            "reconfigure_for_size called with {}x{}, which has zero pixels, using 480x320 instead",
            width,
            height
        );
        this->width = 480;
        this->height = 320;
    } else {
        this->width = width;
        this->height = height;
    }
    auto surface_configuration = wgpu::SurfaceConfiguration {
        .device = this->device,
        .format = this->format.color_format,
        .width = this->width,
        .height = this->height,
    };
    surface.Configure(&surface_configuration);

    // Re-create depth texture.
    if (this->depth_stencil_texture != nullptr) {
        this->depth_stencil_texture = create_depth_stencil_texture(
            this->device,
            this->width,
            this->height,
            this->format.depth_stencil_format
        );
    }
}

void Swapchain::present() {
    this->surface.Present();
}
