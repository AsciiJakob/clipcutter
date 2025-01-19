#ifndef APP_H
#define APP_H
#include "pch.h"
#include "mediaSource.h"
#include "mediaClip.h"

typedef struct Events {
	u32 wakeupOnMpvRenderUpdate, wakeupOnMpvEvents;
};


typedef struct App {
	SDL_Window* window;
    SDL_GLContext gl_context;
	mpv_handle* mpv;
	mpv_render_context* mpv_gl;
	GLuint mpv_texture;
	Events events;
    ImGuiIO io;

	int mpv_width; 
	int mpv_height;

	MediaSource* mediaSources[200];
	MediaClip* mediaClips[200];
} app;



void App_Init(App* app);

#endif
