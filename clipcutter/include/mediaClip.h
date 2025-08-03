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
    bool isSelected;
	
	ImVec2 moveStartPos;
	ImVec2 resizeStartPos;
	float width; // TODO: This var might be bad practice and could lead to bugs. Must be changed when startCutoff or endCutoff is changed
	float padding;
	float startCutoff;
	float endCutoff; 
    
    // these "draw" variables represent the values seen as clips are resized or moved.
    float drawPadding;
    float drawWidth;


};

struct ClipSplitResult {
    MediaClip* clipLeft;
    MediaClip* clipRight;
};

void MediaClip_Init(MediaClip* mediaClip, MediaSource* mediaSource);
ClipSplitResult MediaClip_Split(App* app, MediaClip* clip, float timestamp);
ImVec2 MediaClip_Draw_DrawTracks(App* app, MediaClip* mediaClip, int clipIndex, float drawClipLeftPadding, float drawClipWidth, bool isGhostClip);
void MediaClip_Draw(App* app, MediaClip* mediaClip, int index);
bool MediaClip_IsBeingPlayed(App* app, MediaClip* mediaClip);
void App_DeleteMediaClip(App* app, MediaClip* mediaClip);

#endif
