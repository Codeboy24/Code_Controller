#ifndef CONTROLLER_CODER_WHEEL_H
#define CONTROLLER_CODER_WHEEL_H

#include "app.h"

BOOL WheelInitialize(HWND window);
void WheelUninitialize(void);
void WheelRender(AppState *app);
int WheelSelectionFromStick(SHORT x, SHORT y, int item_count);

#endif
