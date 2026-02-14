#pragma once

#include <webgpu/webgpu_cpp.h>
#include <fstream>
#include <source_location>

#include "log.hxx"

template <class T, class U, class F>
    requires std::same_as<std::invoke_result_t<F, T>, U>
U map_or(std::optional<T> optional_value, U default_value, F&& f) {
    if (optional_value.has_value()) {
        return f(optional_value.value());
    } else {
        return default_value;
    }
}

template <class T, class U, class F>
    requires std::same_as<std::invoke_result_t<F, T&>, U>
U map_or(std::optional<T> optional_value, U default_value, F&& f) {
    if (optional_value.has_value()) {
        return f(optional_value.value());
    } else {
        return default_value;
    }
}

template <class F>
auto lock_mutex(std::mutex& mutex, F&& f) -> std::invoke_result_t<F> {
    std::lock_guard<std::mutex> lock(mutex);
    return std::forward<F>(f)();
}

static inline std::string read_file(std::string_view path) {
    constexpr auto read_size = std::size_t(4096);
    auto stream = std::ifstream(path.data());
    if (!stream.is_open()) {
        log_error("file does not exist at {}", path);
        abort();
    }
    auto out = std::string();
    auto buf = std::string(read_size, '\0');
    while (stream.read(&buf[0], read_size)) {
        out.append(buf, 0, (size_t)stream.gcount());
    }
    out.append(buf, 0, (size_t)stream.gcount());
    return out;
}

template <class T = void>
[[noreturn]]
static inline T todo(std::source_location source_location = std::source_location::current()) {
    log_error(
        "UNIMPLEMENTED @ {}:{}:{}, in function {}",
        source_location.file_name(),
        source_location.line(),
        source_location.column(),
        source_location.function_name()
    );
    abort();
}

template <class T>
    requires std::is_floating_point_v<T>
constexpr T degrees_to_radians(T x) {
    return x * 0.01745329252;
}

static inline bool format_is_srgb(wgpu::TextureFormat format) {
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

static inline bool format_is_float(wgpu::TextureFormat format) {
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
