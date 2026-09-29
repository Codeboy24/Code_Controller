#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include <mmsystem.h>
#include <string.h>

#include "app.h"
#include "controller.h"

#ifndef XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE 7849
#endif

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

typedef HANDLE (WINAPI *CreateWaitableTimerExWFunction)(
    LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD);
typedef DWORD (WINAPI *XInputGetStateFunction)(DWORD, XINPUT_STATE *);

static XInputGetStateFunction GetStateFunction(AppState *app)
{
    XInputGetStateFunction get_state;
    _Static_assert(sizeof(get_state) == sizeof(app->xinput_get_state),
                   "Windows function pointers must have matching sizes");
    memcpy(&get_state, &app->xinput_get_state, sizeof(get_state));
    return get_state;
}

BOOL ControllerInitialize(AppState *app)
{
    static const WCHAR *const dll_names[] = {
        L"xinput1_4.dll",
        L"xinput1_3.dll",
        L"xinput9_1_0.dll"
    };
    unsigned int i;

    for (i = 0; i < sizeof(dll_names) / sizeof(dll_names[0]); ++i) {
        app->xinput_module = LoadLibraryExW(dll_names[i], NULL,
                                            LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (app->xinput_module)
            break;
    }
    if (!app->xinput_module)
        return FALSE;

    app->xinput_get_state = GetProcAddress(app->xinput_module, "XInputGetState");
    if (!app->xinput_get_state) {
        FreeLibrary(app->xinput_module);
        app->xinput_module = NULL;
        return FALSE;
    }
    return TRUE;
}

ControllerInput ControllerRead(AppState *app)
{
    ControllerInput input;
    EnterCriticalSection(&app->input_lock);
    input = app->input;
    LeaveCriticalSection(&app->input_lock);
    return input;
}

static DWORD WINAPI PollController(void *parameter)
{
    AppState *app = (AppState *)parameter;
    HANDLE timer = NULL;
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    FARPROC create_timer_proc = kernel
                                    ? GetProcAddress(kernel, "CreateWaitableTimerExW")
                                    : NULL;
    CreateWaitableTimerExWFunction create_timer = NULL;
    LARGE_INTEGER due_time;
    WORD previous_buttons = 0;
    BOOL previous_rt = FALSE;
    BOOL timer_resolution_requested = FALSE;
    XInputGetStateFunction get_state = GetStateFunction(app);

    _Static_assert(sizeof(create_timer) == sizeof(create_timer_proc),
                   "Windows function pointers must have matching sizes");
    if (create_timer_proc)
        memcpy(&create_timer, &create_timer_proc, sizeof(create_timer));
    if (create_timer)
        timer = create_timer(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                             TIMER_MODIFY_STATE | SYNCHRONIZE);
    if (!timer) {
        timer_resolution_requested = (timeBeginPeriod(1) == TIMERR_NOERROR);
        timer = CreateWaitableTimerW(NULL, FALSE, NULL);
    }
    if (!timer) {
        if (timer_resolution_requested)
            timeEndPeriod(1);
        PostMessageW(app->window, WM_APP_CONTROLLER_ERROR, 0, 0);
        return 1;
    }

    due_time.QuadPart = -10000;
    if (!SetWaitableTimer(timer, &due_time, 1, NULL, NULL, FALSE)) {
        CloseHandle(timer);
        if (timer_resolution_requested)
            timeEndPeriod(1);
        PostMessageW(app->window, WM_APP_CONTROLLER_ERROR, 0, 0);
        return 1;
    }

    for (;;) {
        HANDLE handles[2] = { app->stop_event, timer };
        DWORD wait_result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        XINPUT_STATE state;
        DWORD result;
        WORD buttons;
        WORD pressed;
        BOOL rt_down;
        ControllerInput input;

        if (wait_result == WAIT_OBJECT_0)
            break;
        if (wait_result != WAIT_OBJECT_0 + 1) {
            PostMessageW(app->window, WM_APP_CONTROLLER_ERROR, 0, 0);
            break;
        }

        ZeroMemory(&state, sizeof(state));
        result = get_state(0, &state);
        input.stick_x = 0;
        input.stick_y = 0;
        input.connected = (result == ERROR_SUCCESS);
        if (input.connected) {
            input.stick_x = state.Gamepad.sThumbLX;
            input.stick_y = state.Gamepad.sThumbLY;
            if ((LONGLONG)input.stick_x * input.stick_x +
                    (LONGLONG)input.stick_y * input.stick_y <
                XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE *
                    XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
                input.stick_x = 0;
                input.stick_y = 0;
            }
            buttons = state.Gamepad.wButtons;
            rt_down = previous_rt
                          ? state.Gamepad.bRightTrigger >=
                                XINPUT_GAMEPAD_TRIGGER_THRESHOLD / 2
                          : state.Gamepad.bRightTrigger >=
                                XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
        } else {
            buttons = 0;
            rt_down = FALSE;
        }

        EnterCriticalSection(&app->input_lock);
        app->input = input;
        LeaveCriticalSection(&app->input_lock);

        pressed = (WORD)(buttons & (WORD)~previous_buttons);
        if (rt_down && !previous_rt)
            PostMessageW(app->window, WM_APP_BUTTON, APP_BUTTON_RT, 0);
        if (pressed & XINPUT_GAMEPAD_A)
            PostMessageW(app->window, WM_APP_BUTTON, APP_BUTTON_A, 0);
        if (pressed & XINPUT_GAMEPAD_B)
            PostMessageW(app->window, WM_APP_BUTTON, APP_BUTTON_B, 0);
        if (pressed & XINPUT_GAMEPAD_BACK)
            PostMessageW(app->window, WM_APP_BUTTON, APP_BUTTON_EXIT, 0);
        previous_buttons = buttons;
        previous_rt = rt_down;
    }

    CancelWaitableTimer(timer);
    CloseHandle(timer);
    if (timer_resolution_requested)
        timeEndPeriod(1);
    return 0;
}

BOOL ControllerStart(AppState *app)
{
    app->stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!app->stop_event)
        return FALSE;
    app->poll_thread = CreateThread(NULL, 0, PollController, app, 0, NULL);
    if (!app->poll_thread) {
        CloseHandle(app->stop_event);
        app->stop_event = NULL;
        return FALSE;
    }
    return TRUE;
}

void ControllerStop(AppState *app)
{
    if (app->stop_event)
        SetEvent(app->stop_event);
    if (app->poll_thread) {
        WaitForSingleObject(app->poll_thread, INFINITE);
        CloseHandle(app->poll_thread);
        app->poll_thread = NULL;
    }
    if (app->stop_event) {
        CloseHandle(app->stop_event);
        app->stop_event = NULL;
    }
}

void ControllerUninitialize(AppState *app)
{
    if (app->xinput_module) {
        FreeLibrary(app->xinput_module);
        app->xinput_module = NULL;
        app->xinput_get_state = NULL;
    }
}
