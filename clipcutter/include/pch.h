#ifndef PCH_H
#define PCH_H

#include "log.h"

#include <Windows.h>

#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <assert.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

//#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"
#include "SDL3/SDL_opengl.h"
#include "SDL3/SDL_dialog.h"
#include <SDL3/SDL_messagebox.h>

#include <mpv/client.h>
#include <mpv/render_gl.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavcodec/avcodec.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/timestamp.h>

#include "libavutil/samplefmt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/md5.h"
#include "libavutil/mem.h"
}


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
