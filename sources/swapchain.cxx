#include <span>
#include <webgpu/webgpu_glfw.h>
#include <dawn/webgpu_cpp_print.h>
#include <fmt/ostream.h>

#include "log.hxx"
#include "swapchain.hxx"

using namespace std::literals;

static inline bool is_srgb(wgpu::TextureFormat format) {
    switch (format) {
    case wgpu::TextureFormat::RGBA8UnormSrgb:
    case wgpu::TextureFormat::BGRA8UnormSrgb:
    case wgpu::TextureFormat::BC1RGBAUnormSrgb:
    case wgpu::TextureFormat::BC2RGBAUnormSrgb:
    case wgpu::TextureFormat::BC3RGBAUnormSrgb:
    case wgpu::TextureFormat::BC7RGBAUnormSrgb:
    case wgpu::TextureFormat::ETC2RGB8UnormSrgb:
    case wgpu::TextureFormat::ETC2RGB8A1UnormSrgb:
    case wgpu::TextureFormat::ETC2RGBA8UnormSrgb:
    case wgpu::TextureFormat::ASTC4x4UnormSrgb:
    case wgpu::TextureFormat::ASTC5x4UnormSrgb:
    case wgpu::TextureFormat::ASTC5x5UnormSrgb:
    case wgpu::TextureFormat::ASTC6x5UnormSrgb:
    case wgpu::TextureFormat::ASTC6x6UnormSrgb:
    case wgpu::TextureFormat::ASTC8x5UnormSrgb:
    case wgpu::TextureFormat::ASTC8x6UnormSrgb:
    case wgpu::TextureFormat::ASTC8x8UnormSrgb:
    case wgpu::TextureFormat::ASTC10x5UnormSrgb:
    case wgpu::TextureFormat::ASTC10x6UnormSrgb:
    case wgpu::TextureFormat::ASTC10x8UnormSrgb:
    case wgpu::TextureFormat::ASTC10x10UnormSrgb:
    case wgpu::TextureFormat::ASTC12x10UnormSrgb:
    case wgpu::TextureFormat::ASTC12x12UnormSrgb: return true;
    default: return false;
    }
}

static inline bool is_float(wgpu::TextureFormat format) {
    switch (format) {
    case wgpu::TextureFormat::R16Float:
    case wgpu::TextureFormat::R32Float:
    case wgpu::TextureFormat::RG16Float:
    case wgpu::TextureFormat::RG32Float:
    case wgpu::TextureFormat::RGBA16Float:
    case wgpu::TextureFormat::RGBA32Float:
    case wgpu::TextureFormat::Depth32Float:
    case wgpu::TextureFormat::Depth32FloatStencil8:
    case wgpu::TextureFormat::BC6HRGBFloat: return true;
    default: return false;
    }
}
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
    wgpu::TextureFormat result = wgpu::TextureFormat::Undefined;
    for (wgpu::TextureFormat format : supported_formats) {
        if (info.prefer_srgb && is_srgb(format)) {
            result = format;
            break;
        }
        if (info.prefer_float && is_float(format)) {
            result = format;
            break;
        }
    }

    /// Safe guard against if no texture is found at all.
    if (result == wgpu::TextureFormat::Undefined) {
        if (supported_formats.size() == 0) {
            result = wgpu::TextureFormat::BGRA8UnormSrgb;
            log_error(
                "swapchain creation: window does not report supporting any texture format at all, trying format {} as a last resort",
                fmt::streamed(result)
            );
        } else {
            log_error(
                "swapchain creation: no suitable texture format is found, using the first availible one instead: {}",
                fmt::streamed(result)
            );
            result = supported_formats[0];
        }
    } else if (info.prefer_srgb && !is_srgb(result)) {
        log_info(
            "swapchain creation: requested SRGB texture but window does not support SRGB output, using {} instead",
            fmt::streamed(result)
        );
    } else if (info.prefer_float && !is_float(result)) {
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

Canvas Swapchain::get_current_surface() {
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
    surface.depth_stencil_texture_view = this->depth_stencil_texture.CreateView();

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
