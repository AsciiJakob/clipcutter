cmake_minimum_required(VERSION 3.20)
project(clipcutter)

set(CMAKE_CXX_STANDARD 20)

option(USE_ASAN "Enable Address Sanitizer" OFF)

if(USE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

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
        -Wno-conversion-null # allow using NULL in the C way
        -Wno-pointer-arith # allow doing comparisons to NULL
        -Werror    # Treat warnings as errors
    )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wno-class-memaccess # ignore complaining about using memset on structs (it's fine for my datastructures)
    )
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)  # highest warnings
    # add /WX to treat warnings as errors
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


if(WIN32)
target_link_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/sdl3/lib/x64"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/libmpv/lib"
	"${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg/lib"
)
elseif(UNIX)
    target_link_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/sdl3/lib/x64"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/libmpv/lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg/lib"
    #/usr/lib
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL3
        mpv
        avformat
        avcodec
        avutil
        avfilter
        GL
        dl
        pthread
    )

    # set rpath so the binary can find shared libs in ./deps/ffmpeg/lib
    set(CMAKE_INSTALL_RPATH "\$ORIGIN/deps/ffmpeg/lib")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# seems i am linking som libraries twice on linux, TODO: remove that and test when i am on linux
target_link_libraries(${PROJECT_NAME} PRIVATE
    avformat
    avcodec
    avutil
    avfilter
)
if(WIN32)
	target_link_libraries(${PROJECT_NAME} PRIVATE
		sdl3
		libmpv
		opengl32
		kernel32
		user32
		gdi32
		# msvcrt
	)
else()
	target_link_libraries(${PROJECT_NAME} PRIVATE
		SDL3
		mpv
	)

	target_link_directories(${PROJECT_NAME} PRIVATE
		/usr/lib/
	)
	target_include_directories(${PROJECT_NAME} PRIVATE
		/usr/include/
	)
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CC_PLATFORM_WINDOWS NOMINMAX)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/release"
    RUNTIME_OUTPUT_DIRECTORY         "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}"
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:CC_BUILD_DEBUG>
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_WARNINGS
)


if(USE_ASAN)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/fsanitize=address>)
        target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/fsanitize=address>)
    else()
        target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)
        target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:-fsanitize=address>)
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE CC_USE_ASAN)
endif()

set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/imgui/backends/*.cpp
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg/bin/avcodec-61.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg/bin/avdevice-61.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg/bin/avfilter-10.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg/bin/avformat-61.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg/bin/avutil-59.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg/bin/postproc-58.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg/bin/swresample-5.dll"
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/ffmpeg/bin/swscale-8.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/sdl3/lib/x64/SDL3.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/deps/libmpv/lib/libmpv-2.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"

    COMMENT "copying runtime dependencies to build directory"
)