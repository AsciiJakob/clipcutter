#ifndef PCH_H
#define PCH_H

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <imgui.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include "SDL.h"
#include "SDL_opengl.h"

#include <mpv/client.h>
#include <mpv/render_gl.h>

#define cc_unused(x) x

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned long      u32;
typedef unsigned long long u64;

typedef signed char        s8;
typedef signed short       s16;
typedef signed long        s32;
typedef signed long long   s64;

typedef bool b8;

#endif // PCH_H