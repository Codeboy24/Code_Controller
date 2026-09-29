#ifndef CONTROLLER_CODER_CONTROLLER_H
#define CONTROLLER_CODER_CONTROLLER_H

#include "app.h"

BOOL ControllerInitialize(AppState *app);
BOOL ControllerStart(AppState *app);
void ControllerStop(AppState *app);
void ControllerUninitialize(AppState *app);
ControllerInput ControllerRead(AppState *app);

#endif
