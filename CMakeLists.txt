cmake_minimum_required(VERSION 3.22)
project(app)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# === GNU++20 === #

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# === Global Compiler Flags === #

add_compile_options("-fno-exceptions")
add_compile_options("-fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=")
add_compile_options("-g")

# remove -DNDEBUG from release flags
string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")

set(CMAKE_CXX_FLAGS "-O2")
set(CMAKE_CXX_FLAGS_DEBUG "-O0")

add_executable(app
  "sources/main.cxx"
  "sources/log.cxx"
  "sources/object.cxx"
  "sources/entity.cxx"
  "sources/scene.cxx"
  "sources/canvas.cxx"
  "sources/swapchain.cxx"
  "sources/texture_blitter.cxx"
  "sources/camera/base.cxx"
  "sources/camera/perspective.cxx"
  "sources/camera/orthographic.cxx"
  "sources/geometry/base.cxx"
  "sources/geometry/box.cxx"
  "sources/material/base.cxx"
  "sources/material/uv_debug.cxx"
  "sources/material/color.cxx"
)

# === Project Compiler Flags === #

target_compile_options(app PRIVATE "-Wall" "-Wextra")

# === Dependencies === #

# dawn
set(DAWN_FETCH_DEPENDENCIES ON)
set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC)
add_subdirectory("libraries/dawn" EXCLUDE_FROM_ALL)

# fmt
set(FMT_USE_CONCEPTS ON)
add_subdirectory("libraries/fmt" EXCLUDE_FROM_ALL)

# glm
set(GLM_ENABLE_CXX_20 ON)
set(GLM_ENABLE_LANG_EXTENSIONS ON)
set(GLM_ENABLE_FAST_MATH ON)
if(CMAKE_SYSTEM_PROCESSOR EQUAL "arm64")
    set(GLM_ENABLE_SIMD_NEON ON)
elseif(CMAKE_SYSTEM_PROCESSOR EQUAL "x86_64")
    set(GLM_ENABLE_SIMD_AVX2 ON)
endif()
add_subdirectory("libraries/glm" EXCLUDE_FROM_ALL)

# fastgltf
set(FASTGLTF_COMPILE_AS_CPP20 ON)
add_subdirectory(libraries/fastgltf)

# === Linking === #

target_link_libraries(app PRIVATE fmt glm fastgltf)
if(EMSCRIPTEN)
    set_target_properties(app PROPERTIES SUFFIX ".html")
    target_link_libraries(app PRIVATE emdawnwebgpu_cpp webgpu_glfw)
    target_link_options(app PRIVATE "-sASYNCIFY=1" "-sUSE_GLFW=3" "-sEXIT_RUNTIME=0" "-sASSERTIONS")
    target_link_options(app PRIVATE "-sEXPORTED_RUNTIME_METHODS=['setCanvasSize','requestFullscreen','ccall','cwrap']")
    target_link_options(app PRIVATE "--shell-file=${CMAKE_SOURCE_DIR}/shell.html")
else()
    target_link_libraries(app PRIVATE webgpu_dawn webgpu_glfw glfw)
endif()

