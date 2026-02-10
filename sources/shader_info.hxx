#pragma once

#include <webgpu/webgpu_cpp.h>
#include <vector>

struct ShaderInfo {
    wgpu::ShaderModule shader_module = nullptr;
    std::vector<wgpu::ConstantEntry> constants = {};
};

