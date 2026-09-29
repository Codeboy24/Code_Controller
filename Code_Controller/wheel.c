#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "controller.h"
#include "snippets.h"
#include "wheel.h"

#define WHEEL_PI 3.14159265358979323846
#define WHEEL_OUTER_RADIUS 226
#define WHEEL_INNER_RADIUS 91
#define WHEEL_TEXT_RADIUS 158
#define WHEEL_SEGMENT_STEPS 12

static HWND g_window;
static HDC g_memory_dc;
static HBITMAP g_bitmap;
static HBITMAP g_previous_bitmap;
static void *g_pixels;
static BITMAPINFO g_bitmap_info;
static HBRUSH g_normal_brush;
static HBRUSH g_selected_brush;
static HBRUSH g_hub_brush;
static HPEN g_outline_pen;
static HFONT g_font;

int WheelSelectionFromStick(SHORT x, SHORT y, int item_count)
{
    double angle;
    double sector;
    double position;
    int selected;
    if (item_count <= 1 || (x == 0 && y == 0))
        return 0;

    angle = atan2((double)y, (double)x);
    sector = 2.0 * WHEEL_PI / item_count;
    position = (WHEEL_PI / 2.0 - angle) / sector;
    selected = (int)floor(position + 0.5);
    selected %= item_count;
    if (selected < 0)
        selected += item_count;
    return selected;
}

BOOL WheelInitialize(HWND window)
{
    ZeroMemory(&g_bitmap_info, sizeof(g_bitmap_info));
    g_bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    g_bitmap_info.bmiHeader.biWidth = APP_WINDOW_SIZE;
    g_bitmap_info.bmiHeader.biHeight = -APP_WINDOW_SIZE;
    g_bitmap_info.bmiHeader.biPlanes = 1;
    g_bitmap_info.bmiHeader.biBitCount = 32;
    g_bitmap_info.bmiHeader.biCompression = BI_RGB;

    g_memory_dc = CreateCompatibleDC(NULL);
    if (!g_memory_dc)
        return FALSE;
    g_bitmap = CreateDIBSection(g_memory_dc, &g_bitmap_info, DIB_RGB_COLORS,
                                &g_pixels, NULL, 0);
    if (!g_bitmap) {
        DeleteDC(g_memory_dc);
        g_memory_dc = NULL;
        return FALSE;
    }
    g_previous_bitmap = (HBITMAP)SelectObject(g_memory_dc, g_bitmap);
    g_normal_brush = CreateSolidBrush(RGB(29, 40, 57));
    g_selected_brush = CreateSolidBrush(RGB(35, 141, 221));
    g_hub_brush = CreateSolidBrush(RGB(19, 26, 37));
    g_outline_pen = CreatePen(PS_SOLID, 2, RGB(105, 139, 169));
    g_font = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (!g_normal_brush || !g_selected_brush || !g_hub_brush ||
        !g_outline_pen || !g_font) {
        WheelUninitialize();
        return FALSE;
    }
    g_window = window;
    return TRUE;
}

void WheelUninitialize(void)
{
    if (g_memory_dc && g_previous_bitmap)
        SelectObject(g_memory_dc, g_previous_bitmap);
    if (g_bitmap)
        DeleteObject(g_bitmap);
    if (g_memory_dc)
        DeleteDC(g_memory_dc);
    if (g_normal_brush)
        DeleteObject(g_normal_brush);
    if (g_selected_brush)
        DeleteObject(g_selected_brush);
    if (g_hub_brush)
        DeleteObject(g_hub_brush);
    if (g_outline_pen)
        DeleteObject(g_outline_pen);
    if (g_font)
        DeleteObject(g_font);
    g_window = NULL;
    g_memory_dc = NULL;
    g_bitmap = NULL;
    g_previous_bitmap = NULL;
    g_pixels = NULL;
    g_normal_brush = NULL;
    g_selected_brush = NULL;
    g_hub_brush = NULL;
    g_outline_pen = NULL;
    g_font = NULL;
}

static void DrawSegment(HDC dc, int index, int count, BOOL selected)
{
    POINT points[WHEEL_SEGMENT_STEPS + 3];
    double step = 2.0 * WHEEL_PI / count;
    double center_angle = WHEEL_PI / 2.0 - index * step;
    double start_angle = center_angle + step / 2.0;
    double end_angle = center_angle - step / 2.0;
    int i;
    HBRUSH brush = selected ? g_selected_brush : g_normal_brush;
    HBRUSH old_brush = (HBRUSH)SelectObject(dc, brush);
    HPEN old_pen = (HPEN)SelectObject(dc, g_outline_pen);

    points[0].x = APP_WINDOW_SIZE / 2;
    points[0].y = APP_WINDOW_SIZE / 2;
    for (i = 0; i <= WHEEL_SEGMENT_STEPS; ++i) {
        double angle = start_angle +
                       (end_angle - start_angle) * i / WHEEL_SEGMENT_STEPS;
        points[i + 1].x = APP_WINDOW_SIZE / 2 +
                          (LONG)(cos(angle) * WHEEL_OUTER_RADIUS);
        points[i + 1].y = APP_WINDOW_SIZE / 2 -
                          (LONG)(sin(angle) * WHEEL_OUTER_RADIUS);
    }
    Polygon(dc, points, WHEEL_SEGMENT_STEPS + 2);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
}

static void DrawLabel(HDC dc, const WCHAR *text, int index, int count)
{
    double angle = WHEEL_PI / 2.0 - index * (2.0 * WHEEL_PI / count);
    int center_x = APP_WINDOW_SIZE / 2 +
                   (int)(cos(angle) * WHEEL_TEXT_RADIUS);
    int center_y = APP_WINDOW_SIZE / 2 -
                   (int)(sin(angle) * WHEEL_TEXT_RADIUS);
    RECT rectangle = { center_x - 47, center_y - 26,
                       center_x + 47, center_y + 26 };
    DrawTextW(dc, text, -1, &rectangle,
              DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
}

static void DrawWheelText(HDC dc, AppState *app, int count)
{
    WCHAR text[96];
    WCHAR status[APP_STATUS_LENGTH];
    ControllerInput input = ControllerRead(app);
    HFONT old_font = (HFONT)SelectObject(dc, g_font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(248, 250, 255));

    if (app->mode == WHEEL_PRIMARY) {
        int i;
        for (i = 0; i < count; ++i)
            DrawLabel(dc, SnippetMainLabel(i), i, count);
        lstrcpynW(text, L"Python", (int)(sizeof(text) / sizeof(text[0])));
    } else {
        char label[64];
        int i;
        for (i = 0; i < count; ++i) {
            const char *context_label =
                SnippetContextLabel(app, app->selected_construct, i,
                                    label, sizeof(label));
            WCHAR wide_label[64];
            if (context_label &&
                MultiByteToWideChar(CP_UTF8, 0, context_label, -1, wide_label,
                                    (int)(sizeof(wide_label) / sizeof(wide_label[0]))) > 0)
                DrawLabel(dc, wide_label, i, count);
        }
        lstrcpynW(text, L"Suggestions", (int)(sizeof(text) / sizeof(text[0])));
    }

    {
        HBRUSH old_brush = (HBRUSH)SelectObject(dc, g_hub_brush);
        HPEN old_pen = (HPEN)SelectObject(dc, g_outline_pen);
        Ellipse(dc, APP_WINDOW_SIZE / 2 - WHEEL_INNER_RADIUS,
                APP_WINDOW_SIZE / 2 - WHEEL_INNER_RADIUS,
                APP_WINDOW_SIZE / 2 + WHEEL_INNER_RADIUS,
                APP_WINDOW_SIZE / 2 + WHEEL_INNER_RADIUS);
        SelectObject(dc, old_pen);
        SelectObject(dc, old_brush);
    }

    {
        RECT center = { APP_WINDOW_SIZE / 2 - 78, APP_WINDOW_SIZE / 2 - 40,
                        APP_WINDOW_SIZE / 2 + 78, APP_WINDOW_SIZE / 2 + 4 };
        DrawTextW(dc, text, -1, &center, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    {
        RECT center = { APP_WINDOW_SIZE / 2 - 79, APP_WINDOW_SIZE / 2 + 3,
                        APP_WINDOW_SIZE / 2 + 79, APP_WINDOW_SIZE / 2 + 38 };
        if (app->mode == WHEEL_PRIMARY) {
            lstrcpynW(text, L"A: insert   RT: options", (int)(sizeof(text) / sizeof(text[0])));
        } else {
            lstrcpynW(text, L"A: insert   RT: back", (int)(sizeof(text) / sizeof(text[0])));
        }
        DrawTextW(dc, text, -1, &center,
                  DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
    }

    if (!input.connected)
        lstrcpynW(status, L"Controller not detected", APP_STATUS_LENGTH);
    else if (app->status[0])
        lstrcpynW(status, app->status, APP_STATUS_LENGTH);
    else if (app->mode == WHEEL_PRIMARY)
        lstrcpynW(status, L"Move stick; RT opens suggestions; B cancels",
                  APP_STATUS_LENGTH);
    else
        lstrcpynW(status, L"Context wheel: A inserts, RT returns, B cancels",
                  APP_STATUS_LENGTH);
    {
        RECT footer = { 12, APP_WINDOW_SIZE - 40, APP_WINDOW_SIZE - 12,
                        APP_WINDOW_SIZE - 8 };
        DrawTextW(dc, status, -1, &footer,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    SelectObject(dc, old_font);
}

void WheelRender(AppState *app)
{
    POINT destination;
    POINT source = { 0, 0 };
    SIZE size = { APP_WINDOW_SIZE, APP_WINDOW_SIZE };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    int count;
    int i;
    DWORD *pixels = (DWORD *)g_pixels;

    if (!g_window || !pixels || app->mode == WHEEL_CLOSED)
        return;

    count = app->mode == WHEEL_PRIMARY
                ? APP_WHEEL_ITEMS
                : SnippetContextCount(app, app->selected_construct);
    if (count < 1)
        count = 1;
    memset(g_pixels, 0, APP_WINDOW_SIZE * APP_WINDOW_SIZE * sizeof(DWORD));

    for (i = 0; i < count; ++i)
        DrawSegment(g_memory_dc, i, count, i == app->selection);
    {
        HPEN old_pen = (HPEN)SelectObject(g_memory_dc, g_outline_pen);
        HBRUSH old_brush = (HBRUSH)SelectObject(g_memory_dc, GetStockObject(NULL_BRUSH));
        for (i = 0; i < count; ++i) {
            double angle = WHEEL_PI / 2.0 -
                           (i - 0.5) * (2.0 * WHEEL_PI / count);
            MoveToEx(g_memory_dc, APP_WINDOW_SIZE / 2, APP_WINDOW_SIZE / 2, NULL);
            LineTo(g_memory_dc,
                   APP_WINDOW_SIZE / 2 + (int)(cos(angle) * WHEEL_OUTER_RADIUS),
                   APP_WINDOW_SIZE / 2 - (int)(sin(angle) * WHEEL_OUTER_RADIUS));
        }
        SelectObject(g_memory_dc, old_brush);
        SelectObject(g_memory_dc, old_pen);
    }
    DrawWheelText(g_memory_dc, app, count);

    for (i = 0; i < APP_WINDOW_SIZE * APP_WINDOW_SIZE; ++i) {
        DWORD color = pixels[i] & 0x00FFFFFF;
        if (color) {
            DWORD red = ((color >> 16) & 0xFF) * 216 / 255;
            DWORD green = ((color >> 8) & 0xFF) * 216 / 255;
            DWORD blue = (color & 0xFF) * 216 / 255;
            pixels[i] = 0xD8000000 | (red << 16) | (green << 8) | blue;
        }
    }

    destination.x = (GetSystemMetrics(SM_CXSCREEN) - APP_WINDOW_SIZE) / 2;
    destination.y = (GetSystemMetrics(SM_CYSCREEN) - APP_WINDOW_SIZE) / 2;
    if (!UpdateLayeredWindow(g_window, NULL, &destination, &size, g_memory_dc,
                             &source, 0, &blend, ULW_ALPHA)) {
        _snwprintf_s(app->status, APP_STATUS_LENGTH, _TRUNCATE,
                     L"Overlay rendering failed (Windows error %lu).",
                     GetLastError());
    }
}
