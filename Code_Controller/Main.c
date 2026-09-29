#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "app.h"
#include "controller.h"
#include "snippets.h"
#include "wheel.h"

static AppState g_app;

static void SetWheelMode(WheelMode mode)
{
    g_app.mode = mode;
    if (mode == WHEEL_CLOSED) {
        ShowWindow(g_app.window, SW_HIDE);
        g_app.selected_construct = -1;
        g_app.status[0] = '\0';
        return;
    }

    g_app.status[0] = '\0';
    if (mode == WHEEL_CONTEXTUAL)
        g_app.selection = 0;

    SetWindowPos(g_app.window, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ShowWindow(g_app.window, SW_SHOWNOACTIVATE);
    WheelRender(&g_app);
}

static BOOL InjectText(const char *text)
{
    int wide_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          text, -1, NULL, 0);
    if (wide_length <= 0) {
        lstrcpynW(g_app.status, L"Could not convert selected text.", APP_STATUS_LENGTH);
        return FALSE;
    }

    WCHAR wide_text[APP_SNIPPET_LENGTH];
    if (wide_length > APP_SNIPPET_LENGTH ||
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
                            wide_text, APP_SNIPPET_LENGTH) != wide_length) {
        lstrcpynW(g_app.status, L"Selected text is too long to inject.", APP_STATUS_LENGTH);
        return FALSE;
    }

    INPUT inputs[(APP_SNIPPET_LENGTH - 1) * 2];
    UINT count = 0;
    int i;
    for (i = 0; i < wide_length - 1; ++i) {
        inputs[count].type = INPUT_KEYBOARD;
        if (wide_text[i] == L'\n') {
            inputs[count].ki.wVk = VK_RETURN;
            inputs[count].ki.wScan = 0;
            inputs[count].ki.dwFlags = 0;
        } else {
            inputs[count].ki.wVk = 0;
            inputs[count].ki.wScan = wide_text[i];
            inputs[count].ki.dwFlags = KEYEVENTF_UNICODE;
        }
        inputs[count].ki.time = 0;
        inputs[count].ki.dwExtraInfo = 0;
        ++count;
        inputs[count] = inputs[count - 1];
        if (wide_text[i] == L'\n') {
            inputs[count].ki.dwFlags = KEYEVENTF_KEYUP;
        } else {
            inputs[count].ki.dwFlags |= KEYEVENTF_KEYUP;
        }
        ++count;
    }

    if (count == 0 || SendInput(count, inputs, sizeof(INPUT)) != count) {
        lstrcpynW(g_app.status,
                  L"Text injection failed. Keep the target editor in the foreground.",
                  APP_STATUS_LENGTH);
        return FALSE;
    }
    return TRUE;
}

static void ConfirmSelection(void)
{
    char snippet[APP_SNIPPET_LENGTH];
    if (g_app.mode == WHEEL_PRIMARY) {
        if (!SnippetBuildMain(g_app.selection, snippet, sizeof(snippet))) {
            lstrcpynW(g_app.status, L"No construct is selected.", APP_STATUS_LENGTH);
            return;
        }
    } else if (g_app.mode == WHEEL_CONTEXTUAL) {
        if (!SnippetBuildContext(&g_app, g_app.selected_construct,
                                 g_app.selection, snippet, sizeof(snippet))) {
            lstrcpynW(g_app.status, L"No contextual suggestion is selected.", APP_STATUS_LENGTH);
            return;
        }
    } else {
        return;
    }

    if (!InjectText(snippet)) {
        WheelRender(&g_app);
        return;
    }

    SnippetRemember(&g_app, snippet);
    SetWheelMode(WHEEL_CLOSED);
}

static LRESULT CALLBACK OverlayProc(HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_CREATE:
        if (!WheelInitialize(window))
            return -1;
        if (!SetTimer(window, APP_REDRAW_TIMER, APP_REDRAW_INTERVAL_MS, NULL))
            return -1;
        return 0;

    case WM_TIMER:
        if (wparam == APP_REDRAW_TIMER && g_app.mode != WHEEL_CLOSED) {
            ControllerInput input = ControllerRead(&g_app);
            int count = (g_app.mode == WHEEL_PRIMARY)
                            ? APP_WHEEL_ITEMS
                            : SnippetContextCount(&g_app, g_app.selected_construct);
            if (input.connected && count > 0 &&
                (input.stick_x != 0 || input.stick_y != 0))
                g_app.selection = WheelSelectionFromStick(input.stick_x, input.stick_y, count);
            WheelRender(&g_app);
        }
        return 0;

    case WM_APP_BUTTON:
        if (wparam == APP_BUTTON_RT) {
            if (g_app.mode == WHEEL_CLOSED) {
                g_app.selected_construct = -1;
                g_app.selection = 0;
                SetWheelMode(WHEEL_PRIMARY);
            } else if (g_app.mode == WHEEL_PRIMARY) {
                ControllerInput input = ControllerRead(&g_app);
                if (input.connected &&
                    (input.stick_x != 0 || input.stick_y != 0))
                    g_app.selection = WheelSelectionFromStick(input.stick_x,
                                                               input.stick_y,
                                                               APP_WHEEL_ITEMS);
                g_app.selected_construct = g_app.selection;
                SetWheelMode(WHEEL_CONTEXTUAL);
            } else {
                g_app.selection = g_app.selected_construct;
                SetWheelMode(WHEEL_PRIMARY);
            }
        } else if (wparam == APP_BUTTON_A) {
            ConfirmSelection();
        } else if (wparam == APP_BUTTON_B && g_app.mode != WHEEL_CLOSED) {
            SetWheelMode(WHEEL_CLOSED);
        } else if (wparam == APP_BUTTON_EXIT) {
            DestroyWindow(g_app.window);
        }
        return 0;

    case WM_APP_CONTROLLER_ERROR:
        lstrcpynW(g_app.status, L"Controller polling stopped: timer creation failed.",
                  APP_STATUS_LENGTH);
        if (g_app.mode != WHEEL_CLOSED)
            WheelRender(&g_app);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY:
        KillTimer(window, APP_REDRAW_TIMER);
        WheelUninitialize();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show)
{
    WNDCLASSEXW window_class;
    MSG message;
    (void)previous;
    (void)command_line;
    (void)show;

    ZeroMemory(&g_app, sizeof(g_app));
    g_app.instance = instance;
    g_app.selection = 0;
    g_app.selected_construct = -1;
    g_app.mode = WHEEL_CLOSED;
    InitializeCriticalSection(&g_app.input_lock);

    if (!ControllerInitialize(&g_app)) {
        MessageBoxW(NULL, L"XInput could not be loaded from the Windows system directory.",
                    L"Controller Coder", MB_OK | MB_ICONERROR);
        DeleteCriticalSection(&g_app.input_lock);
        return 1;
    }

    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance;
    window_class.lpfnWndProc = OverlayProc;
    window_class.lpszClassName = APP_WINDOW_CLASS;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    if (!RegisterClassExW(&window_class)) {
        MessageBoxW(NULL, L"Could not register the overlay window class.",
                    L"Controller Coder", MB_OK | MB_ICONERROR);
        ControllerUninitialize(&g_app);
        DeleteCriticalSection(&g_app.input_lock);
        return 1;
    }

    g_app.window = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        APP_WINDOW_CLASS, L"Controller Coder", WS_POPUP,
        0, 0, APP_WINDOW_SIZE, APP_WINDOW_SIZE,
        NULL, NULL, instance, NULL);
    if (!g_app.window) {
        MessageBoxW(NULL, L"Could not create the radial overlay window.",
                    L"Controller Coder", MB_OK | MB_ICONERROR);
        ControllerUninitialize(&g_app);
        DeleteCriticalSection(&g_app.input_lock);
        return 1;
    }

    if (!ControllerStart(&g_app)) {
        MessageBoxW(NULL, L"Could not start the controller polling thread.",
                    L"Controller Coder", MB_OK | MB_ICONERROR);
        DestroyWindow(g_app.window);
        ControllerUninitialize(&g_app);
        DeleteCriticalSection(&g_app.input_lock);
        return 1;
    }

    while (GetMessageW(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    ControllerStop(&g_app);
    ControllerUninitialize(&g_app);
    DeleteCriticalSection(&g_app.input_lock);
    return (int)message.wParam;
}
