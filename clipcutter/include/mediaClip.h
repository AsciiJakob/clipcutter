#include "pch.h"
#ifndef MEDIACLIP_H
#define MEDIACLIP_H
#include "mediaSource.h"
#include "app.h"

typedef struct App App;
typedef struct MediaSource MediaSource;

struct MediaClip {
	MediaSource* source;

    int timelineEventsIndex;

	// UI
	bool isResizingLeft;
	bool isResizingRight;
	bool isBeingMoved;
	bool isHovered;
	
	ImVec2 moveStartPos;
	ImVec2 resizeStartPos;
	float width; // TODO: This var might be bad practice and could lead to bugs. Must be changed when startCutoff or endCutoff is changed
	float padding;
	float drawStartCutoff;
	float drawEndCutoff; 


};

void MediaClip_Init(MediaClip* mediaClip, MediaSource* mediaSource);
void MediaClip_Draw(App* app, MediaClip* mediaClip, int index);
bool MediaClip_IsBeingPlayed(App* app, MediaClip* mediaClip);
void App_DeleteMediaClip(App* app, MediaClip* mediaClip);

#endif
