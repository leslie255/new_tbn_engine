#pragma once

#include <vector>
#include <webgpu/webgpu_cpp.h>

struct ShaderInfo {
    wgpu::ShaderModule shader_module = nullptr;
    std::string entry_point = "main";
    std::vector<wgpu::ConstantEntry> constants = {};
};
