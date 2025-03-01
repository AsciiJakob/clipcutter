cmake_minimum_required(VERSION 3.20)
project(clipcutter)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)

# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# option(USE_ASAN "Enable Address Sanitizer" OFF)

file(GLOB_RECURSE SOURCES 
    src/*.cc
    # "deps/imgui/*.cpp"
    deps/imgui/imgui_demo.cpp
    deps/imgui/imgui_draw.cpp
    deps/imgui/imgui_tables.cpp
    deps/imgui/imgui_widgets.cpp
    deps/imgui/imgui.cpp
    deps/imgui/backends/imgui_impl_opengl3.cpp
    deps/imgui/backends/imgui_impl_sdl3.cpp
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE 
        -Wall        # warn all
        -Wextra      # extra warnings
        -Wpedantic   # Strictly ISO C/C++
        -Wno-unused-value # ignore Wunused-value
        -Werror    # Treat warnings as errors
    )
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)  # Highest warnings
    # Or use /W3 for moderate warnings
    # Add /WX to treat warnings as errors
endif()

target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
    deps/sdl3/include
    deps/imgui
    deps/imgui/backends
    deps/libmpv/include
	deps/ffmpeg/include
)
target_include_directories(${PROJECT_NAME} PRIVATE
    include
)


target_link_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/sdl3/lib/x64"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/libmpv/lib"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg/lib"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    sdl3
    libmpv
    avformat
    avcodec
    avutil
    opengl32
    kernel32
    user32
    gdi32
    # msvcrt
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CC_PLATFORM_WINDOWS NOMINMAX)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/release")

target_compile_definitions(${PROJECT_NAME} PRIVATE CC_BUILD_DEBUG _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)


# fix later
# target_compile_definitions(${PROJECT_NAME} PRIVATE
#     $<$<CONFIG:Debug>:CC_BUILD_DEBUG>
#     $<$<CONFIG:Release>:CC_BUILD_RELEASE>
# )

if(USE_ASAN)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /fsanitize=address)
        target_link_options(${PROJECT_NAME} PRIVATE /fsanitize=address)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
        target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE CC_USE_ASAN)
endif()

# target_precompile_headers(${PROJECT_NAME} PRIVATE
#     include/pch.h
# )

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui/backends/*.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)


# if(MSVC)
#     # For MSVC, set warning level to /W0 (disable all warnings)
#     set_source_files_properties(
#         ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui/*.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui/backends/*.cpp
#         PROPERTIES COMPILE_FLAGS "/W0"
#     )
# else()
#     # For GCC/Clang, use -w to disable all warnings
#     set_source_files_properties(
#         ${CMAKE_CURRENT_SOURCE_DIR}/deps/sdl3/include/SDL3/*.h
#         ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui/*.cpp
#         ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui/backends/*.cpp
#         PROPERTIES COMPILE_FLAGS "-w"
#     )
# endif()

