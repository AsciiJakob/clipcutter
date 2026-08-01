#ifndef UI_H
#define UI_H
#include "pch.h"
#include "app.h"

#define TIMELINE_GRID_TICKS_HEIGHT 15
#define TIMELINE_GRID_PRECISION 20 // technically pixels per step/line in grid?

double UI_GetNiceNumber(double rawStep);
void UI_DrawEditor(App* app);


#endif
