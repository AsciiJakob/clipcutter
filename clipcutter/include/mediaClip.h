#include "pch.h"
#ifndef MEDIACLIP_H
#define MEDIACLIP_H
#include "mediaSource.h"

typedef struct MediaClip {
	MediaSource* source;
	bool isSelected;
	bool isResizingLeft;
	bool isResizingRight;
	bool isBeingMoved;
	ImVec2 moveStartPos;
	
	float drawWidth;
	float drawPadding;
	float drawStartOffset;
	float drawEndOffset; 


} MediaClip;

void MediaClip_Init(MediaClip* mediaClip, MediaSource* mediaSource);

#endif