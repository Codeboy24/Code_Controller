#ifndef CONTROLLER_CODER_APP_H
#define CONTROLLER_CODER_APP_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define APP_WINDOW_CLASS L"ControllerCoderOverlay"
#define APP_WINDOW_SIZE 560
#define APP_WHEEL_ITEMS 8
#define APP_STATUS_LENGTH 160
#define APP_SNIPPET_LENGTH 256
#define APP_HISTORY_LENGTH 16
#define APP_VARIABLE_LENGTH 16
#define APP_VARIABLE_NAME_LENGTH 32
#define APP_REDRAW_TIMER 1
#define APP_REDRAW_INTERVAL_MS 16

#define WM_APP_BUTTON (WM_APP + 1)
#define WM_APP_CONTROLLER_ERROR (WM_APP + 2)

typedef enum WheelMode {
    WHEEL_CLOSED,
    WHEEL_PRIMARY,
    WHEEL_CONTEXTUAL
} WheelMode;

typedef enum AppButton {
    APP_BUTTON_RT,
    APP_BUTTON_A,
    APP_BUTTON_B,
    APP_BUTTON_EXIT
} AppButton;

typedef struct ControllerInput {
    SHORT stick_x;
    SHORT stick_y;
    BOOL connected;
} ControllerInput;

typedef struct AppState {
    HINSTANCE instance;
    HWND window;
    WheelMode mode;
    int selection;
    int selected_construct;
    WCHAR status[APP_STATUS_LENGTH];

    CRITICAL_SECTION input_lock;
    ControllerInput input;
    HANDLE stop_event;
    HANDLE poll_thread;
    HMODULE xinput_module;
    FARPROC xinput_get_state;

    char history[APP_HISTORY_LENGTH][APP_SNIPPET_LENGTH];
    unsigned int history_count;
    char variables[APP_VARIABLE_LENGTH][APP_VARIABLE_NAME_LENGTH];
    unsigned int variable_count;
} AppState;

#endif
