#include "pch.h"
#include "app.h"

static void* get_proc_address_func(void* fn_ctx, const char* name) {
    cc_unused(fn_ctx);
    return (void*) SDL_GL_GetProcAddress(name);
}

bool initWindow(App* app) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        log_fatal("SDL failed to init: %s", SDL_GetError());
        return false;
    }

    SDL_Window* window = nullptr;
    SDL_GLContext* gl_context = &app->gl_context;
    const char* glsl_version = "#version 100";


    { // Setup SDL GL
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

        // Create window with graphics context
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        window = SDL_CreateWindow("Clipcutter", 1280, 720, window_flags);
        if (window == nullptr) {
            log_fatal("SDL_CreateWindow(): %s\n", SDL_GetError());
            return false;
        }
        app->window = window;

        app->gl_context = SDL_GL_CreateContext(window);
        SDL_GL_MakeCurrent(window, *gl_context);
        SDL_GL_SetSwapInterval(1); // Enable vsync
    }

		GLuint* mpv_texture = &app->mpv_texture;


    { // Setup MPV
		// init render texture
		glGenTextures(1, mpv_texture);
		glBindTexture(GL_TEXTURE_2D, *mpv_texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        mpv_handle* mpv = nullptr;
		mpv = mpv_create();
		if (!mpv) {
            log_fatal("mpv_create()");
			return false;
		}
        app->mpv = mpv;

		mpv_set_option_string(mpv, "vo", "libmpv");
		mpv_set_option_string(mpv, "video-timing-offset", "0"); // prevent the application from being locked to the framerate of the video

		// Some few options can only be set before mpv_initialize().
		if (mpv_initialize(mpv) < 0) {
            log_fatal("mpv_initialize()");
            return false;
		}

		mpv_request_log_messages(mpv, "debug");

        mpv_opengl_init_params opengl_init_params = {
			.get_proc_address = get_proc_address_func,
            .get_proc_address_ctx = NULL
        };

        s32 advancedControl = 1;

        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_API_TYPE, (void*) MPV_RENDER_API_TYPE_OPENGL},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &opengl_init_params},
            {MPV_RENDER_PARAM_ADVANCED_CONTROL, &advancedControl},
            {(mpv_render_param_type) 0, nullptr}  // Correct termination for the array
        };

        if (mpv_render_context_create(&app->mpv_gl, mpv, params) < 0) {
            log_fatal("mpv_render_context_create()");
			return false;
        }

        app->events.wakeupOnMpvRenderUpdate = SDL_RegisterEvents(1);
        app->events.wakeupOnMpvEvents = SDL_RegisterEvents(1);
        if (app->events.wakeupOnMpvRenderUpdate == (Uint32)-1 || app->events.wakeupOnMpvEvents == (Uint32)-1) {
            log_fatal("could not register required MPV events");
			return false;
        }

        mpv_set_wakeup_callback(mpv, [](void* ctx) {
            SDL_Event event;
            App* app = (App*) ctx;
            event.type = app->events.wakeupOnMpvEvents;
            SDL_PushEvent(&event);
            }, app);

        mpv_render_context_set_update_callback(app->mpv_gl, [](void* ctx) {
            SDL_Event event;
            App* app = (App*) ctx;
            event.type = app->events.wakeupOnMpvRenderUpdate;
            SDL_PushEvent(&event);
            }, app);
    }



    { // Setup Dear IMGUI
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForOpenGL(window, *gl_context);
        ImGui_ImplOpenGL3_Init(glsl_version);
    }
    return true;
}


void renderMpvTexture(App* app) {
            int w, h;

            SDL_GetWindowSize(app->window, &w, &h);
            mpv_opengl_fbo opengl_fbo = { 0, w, h, 0}; // default framebuffer

            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_OPENGL_FBO, &opengl_fbo},
                //{MPV_RENDER_PARAM_FLIP_Y, &flipY}, // Flip rendering (needed due to flipped GL coordinate system).
                {static_cast<mpv_render_param_type>(0), nullptr} // Correctly terminate the array using MPV_RENDER_PARAM_NONE
            };

            mpv_render_context_render(app->mpv_gl, params);
            //SDL_GL_SwapWindow(window);

			// Read pixels from OpenGL framebuffer
            std::vector<uint8_t> pixels(w* h * 4); // RGBA8888
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

            // Update OpenGL texture with the pixel data
            glBindTexture(GL_TEXTURE_2D, app->mpv_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
}
