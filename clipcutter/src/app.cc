#include "pch.h"
#include "app.h"

void App_Init(App* app) {
	memset(app, 0, sizeof App);
	app->mpv_width = 1280;
	app->mpv_height = 720;
	app->mpv = nullptr;
	app->mpv_gl = nullptr;
}