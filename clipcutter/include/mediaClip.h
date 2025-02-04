#include "pch.h"
#ifndef MEDIACLIP_H
#define MEDIACLIP_H
#include "mediaSource.h"
#include "app.h"

typedef struct App App;
typedef struct MediaSource MediaSource;

typedef struct MediaClip {
	MediaSource* source;

	// UI
	bool isResizingLeft;
	bool isResizingRight;
	bool isBeingMoved;
	bool isHovered;
	
	ImVec2 moveStartPos;
	ImVec2 resizeStartPos;
	float width;
	float padding;
	float drawStartCutoff;
	float drawEndCutoff; 


} MediaClip;

void MediaClip_Init(MediaClip* mediaClip, MediaSource* mediaSource);
void MediaClip_Draw(App* app, MediaClip* mediaClip);

#endif