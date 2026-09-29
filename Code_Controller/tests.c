#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "snippets.h"
#include "wheel.h"

static void TestRadialDirections(void)
{
    assert(WheelSelectionFromStick(0, 32767, 8) == 0);
    assert(WheelSelectionFromStick(23170, 23170, 8) == 1);
    assert(WheelSelectionFromStick(32767, 0, 8) == 2);
    assert(WheelSelectionFromStick(23170, -23170, 8) == 3);
    assert(WheelSelectionFromStick(0, -32768, 8) == 4);
    assert(WheelSelectionFromStick(-23170, -23170, 8) == 5);
    assert(WheelSelectionFromStick(-32768, 0, 8) == 6);
    assert(WheelSelectionFromStick(-23170, 23170, 8) == 7);
    assert(WheelSelectionFromStick(0, 0, 8) == 0);
}

static void TestHistorySuggestions(void)
{
    AppState app = { 0 };
    AppState many_variables = { 0 };
    char output[APP_SNIPPET_LENGTH];
    char label[64];
    const char *context;
    unsigned int i;

    assert(SnippetContextCount(&app, 0) == 2);
    SnippetRemember(&app, "items = []\n");
    assert(SnippetContextCount(&app, 0) == 3);
    context = SnippetContextLabel(&app, 0, 2, label, sizeof(label));
    assert(context && strcmp(context, "for item in items") == 0);
    assert(SnippetBuildContext(&app, 0, 2, output, sizeof(output)));
    assert(strcmp(output, "for item in items:\n    ") == 0);
    assert(SnippetBuildMain(0, output, sizeof(output)));
    assert(strcmp(output, "for i in range(10):\n    ") == 0);
    assert(!SnippetBuildMain(APP_WHEEL_ITEMS, output, sizeof(output)));

    SnippetRemember(&app, "other = []\n");
    context = SnippetContextLabel(&app, 0, 2, label, sizeof(label));
    assert(context && strcmp(context, "for item in other") == 0);
    SnippetRemember(&app, "items = []\n");
    context = SnippetContextLabel(&app, 0, 2, label, sizeof(label));
    assert(context && strcmp(context, "for item in items") == 0);
    context = SnippetContextLabel(&app, 0, 3, label, sizeof(label));
    assert(context && strcmp(context, "for item in other") == 0);

    for (i = 0; i < APP_VARIABLE_LENGTH; ++i) {
        char assignment[32];
        _snprintf_s(assignment, sizeof(assignment), _TRUNCATE,
                    "name%u = []\n", i);
        SnippetRemember(&many_variables, assignment);
    }
    assert(SnippetContextCount(&many_variables, 0) == APP_WHEEL_ITEMS);
    context = SnippetContextLabel(&many_variables, 0, 2, label, sizeof(label));
    assert(context && strcmp(context, "for item in name15") == 0);
}

int main(void)
{
    TestRadialDirections();
    TestHistorySuggestions();
    puts("Controller Coder tests passed.");
    return 0;
}
