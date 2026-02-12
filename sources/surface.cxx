#include <fmt/base.h>
#include <webgpu/webgpu_glfw.h>

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
