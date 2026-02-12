#pragma once

#include <webgpu/webgpu_cpp.h>

struct SurfaceFormat {
    wgpu::TextureFormat color_format = wgpu::TextureFormat::Undefined;
    wgpu::TextureFormat depth_stencil_format = wgpu::TextureFormat::Undefined;
};

struct Surface {
    SurfaceFormat format = {
        .color_format = wgpu::TextureFormat::Undefined,
        .depth_stencil_format = wgpu::TextureFormat::Undefined,
    };

    std::variant<wgpu::Texture, wgpu::SurfaceTexture> color_texture = nullptr;
    wgpu::TextureView color_texture_view = nullptr;

    wgpu::Texture depth_stencil_texture = nullptr;
    wgpu::TextureView depth_stencil_texture_view = nullptr;

    uint32_t width = 0;
    uint32_t height = 0;

    struct CreateInfo {
        uint32_t width;
        uint32_t height;
        wgpu::TextureFormat color_format;
        bool create_depth_stencil_texture;
        wgpu::TextureFormat depth_stencil_format;
    };

    Surface() = default;
    Surface(const wgpu::Device& device, const CreateInfo& info);

    bool has_depth_stencil() const;

    bool is_window_surface() const;

    wgpu::Texture get_color_texture() const;
};
