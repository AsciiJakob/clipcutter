local USE_ASAN = true

project "clipcutter"
    kind("WindowedApp")
    language "C++"
    cppdialect "C++20"
    warnings "Extra"

    files {
        "src/**.cc", "include/**.h",
        "deps/imgui/*.cpp", "deps/imgui/backends/imgui_impl_opengl3.cpp", "deps/imgui/backends/imgui_impl_sdl3.cpp"
    }

    includedirs { "include", "deps/sdl3/include", "deps/imgui", "deps/imgui/backends", "deps/libmpv/include", "deps/ffmpeg/include" }
    libdirs { "deps/sdl3/lib/x64", "deps/libmpv/lib", "deps/ffmpeg/lib" }
    links { "libmpv", "sdl3", "opengl32", "avformat" }

    pchheader "pch.h"
    pchsource "src/pch.cc"

    filter { "system:windows" }
        defines { "CC_PLATFORM_WINDOWS" }

    filter { "configurations:Debug" }
        runtime "Debug"
        targetdir "build/bin/debug"
        objdir "build/obj/debug"
        defines { "CC_BUILD_DEBUG" }

        if USE_ASAN then
            sanitize { "Address" }
            defines { "CC_USE_ASAN" }
        end

    filter { "configurations:Release" }
        runtime "Release"
        targetdir "build/bin/release"
        objdir "build/obj/release"
        defines { "CC_BUILD_RELEASE" }

    filter { "files:deps/imgui/*.cpp" }
        flags { "NoPCH" }

    filter { "files:deps/imgui/backends/*.cpp" }
        flags { "NoPCH" }