#include <fmt/base.h>
#include <span>
#include <webgpu/webgpu_glfw.h>

#include "log.hxx"
#include "surface.hxx"

using namespace std::literals;

Surface::Surface(const wgpu::Device& device, const CreateInfo& info) {
    auto color_texture_descriptor = wgpu::TextureDescriptor {
        .label = "Color Texture of a Surface"sv,
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
        .dimension = wgpu::TextureDimension::e2D,
        .size =
            wgpu::Extent3D {
                .width = info.width,
                .height = info.height,
                .depthOrArrayLayers = 1,
            },
        .format = info.color_format,
    };
    auto color_texture = device.CreateTexture(&color_texture_descriptor);
    this->color_texture_view = color_texture.CreateView();
    this->color_texture = color_texture;
    this->format.color_format = info.color_format;

    if (info.create_depth_stencil_texture) {
        auto depth_stencil_texture_descriptor = wgpu::TextureDescriptor {
            .label = "Depth-Stencil Texture of a Surface"sv,
            .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
            .dimension = wgpu::TextureDimension::e2D,
            .size =
                wgpu::Extent3D {
                    .width = info.width,
                    .height = info.height,
                    .depthOrArrayLayers = 1,
                },
            .format = info.depth_stencil_format,
        };
        this->depth_stencil_texture = device.CreateTexture(&depth_stencil_texture_descriptor);
        this->depth_stencil_texture_view = this->depth_stencil_texture.CreateView();
        this->format.depth_stencil_format = info.depth_stencil_format;
    }
}

bool Surface::has_depth_stencil() const {
    return this->format.depth_stencil_format == wgpu::TextureFormat::Undefined;
}

bool Surface::is_window_surface() const {
    return std::holds_alternative<wgpu::SurfaceTexture>(this->color_texture);
}

wgpu::Texture Surface::get_color_texture() const {
    if (const auto* surface_texture = std::get_if<wgpu::SurfaceTexture>(&this->color_texture)) {
        return surface_texture->texture;
    } else if (const auto* texture = std::get_if<wgpu::Texture>(&this->color_texture)) {
        return *texture;
    } else {
        fmt::println(stderr, "unreachable");
        std::abort();
    }
}

WindowSurface::WindowSurface(
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
    if (this->width == 0)
        this->width = 480;
    if (this->height == 0)
        this->height = 320;

    if (info.create_depth_stencil_texture) {
        auto depth_stencil_texture_descriptor = wgpu::TextureDescriptor {
            .label = "Depth-Stencil Texture of a Surface"sv,
            .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding,
            .dimension = wgpu::TextureDimension::e2D,
            .size =
                wgpu::Extent3D {
                    .width = this->width,
                    .height = this->height,
                    .depthOrArrayLayers = 1,
                },
            .format = info.depth_stencil_format,
        };
        this->depth_stencil_texture = this->device.CreateTexture(&depth_stencil_texture_descriptor);
        this->format.depth_stencil_format = info.create_depth_stencil_texture
                                                ? wgpu::TextureFormat::Undefined
                                                : info.depth_stencil_format;
    }

    wgpu::SurfaceCapabilities capabilities;
    this->surface.GetCapabilities(adapter, &capabilities);
    auto supported_formats = std::span(capabilities.formats, capabilities.formatCount);
    for (wgpu::TextureFormat format : supported_formats) {
        if (format == wgpu::TextureFormat::RGBA8UnormSrgb ||
            format == wgpu::TextureFormat::BGRA8UnormSrgb) {
            this->format.color_format = format;
        }
    }
    if (this->format.color_format == wgpu::TextureFormat::Undefined) {
        if (supported_formats.size() == 0) {
            this->format.color_format = wgpu::TextureFormat::BGRA8UnormSrgb;
        } else {
            this->format.color_format = capabilities.formats[0];
        }
    }

    this->reconfigure_for_size(width, height);
}

uint32_t WindowSurface::get_width() const {
    return this->width;
}

uint32_t WindowSurface::get_height() const {
    return this->height;
}

SurfaceFormat WindowSurface::get_format() const {
    return this->format;
}

Surface WindowSurface::get_current_surface() {
    wgpu::SurfaceTexture surface_texture;
    surface.GetCurrentTexture(&surface_texture);
    auto color_texture_view = surface_texture.texture.CreateView();
    auto depth_stencil_texture_view = this->depth_stencil_texture == nullptr
                                          ? wgpu::TextureView(nullptr)
                                          : this->depth_stencil_texture.CreateView();

    Surface surface;
    surface.width = this->width;
    surface.height = this->height;

    surface.format = this->format;

    surface.color_texture = surface_texture;
    surface.color_texture_view = color_texture_view;

    surface.depth_stencil_texture = this->depth_stencil_texture;
    surface.depth_stencil_texture_view = this->depth_stencil_texture.CreateView();

    return surface;
}

void WindowSurface::reconfigure_for_size(uint32_t width, uint32_t height) {
    this->width = width;
    this->height = height;
    auto surface_configuration = wgpu::SurfaceConfiguration {
        .device = this->device,
        .format = this->format.color_format,
        .width = width,
        .height = height,
    };
    surface.Configure(&surface_configuration);

    // Re-create depth texture.
    this->format.depth_stencil_format = wgpu::TextureFormat::Depth16Unorm;
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
        .format = this->format.depth_stencil_format,
    };
    this->depth_stencil_texture = this->device.CreateTexture(&depth_texture_descriptor);
}

void WindowSurface::present() {
    this->surface.Present();
}
