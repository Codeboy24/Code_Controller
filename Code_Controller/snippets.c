#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "snippets.h"

static const WCHAR *const main_labels[APP_WHEEL_ITEMS] = {
    L"for loop",
    L"while loop",
    L"if / else",
    L"def function",
    L"return",
    L"print()",
    L"import module",
    L"class definition"
};

static const char *const main_snippets[APP_WHEEL_ITEMS] = {
    "for i in range(10):\n    ",
    "while condition:\n    ",
    "if condition:\n    \nelse:\n    ",
    "def function_name():\n    ",
    "return value\n",
    "print(value)\n",
    "import module\n",
    "class ClassName:\n    pass\n"
};

const WCHAR *SnippetMainLabel(int index)
{
    if (index < 0 || index >= APP_WHEEL_ITEMS)
        return L"";
    return main_labels[index];
}

static BOOL CopyText(const char *text, char *output, size_t capacity)
{
    size_t length = strlen(text);
    if (length + 1 > capacity)
        return FALSE;
    memcpy(output, text, length + 1);
    return TRUE;
}

BOOL SnippetBuildMain(int index, char *output, size_t capacity)
{
    if (index < 0 || index >= APP_WHEEL_ITEMS)
        return FALSE;
    return CopyText(main_snippets[index], output, capacity);
}

int SnippetContextCount(const AppState *app, int construct)
{
    if (construct == 0)
        return 2 + (app->variable_count < APP_WHEEL_ITEMS - 2
                        ? (int)app->variable_count
                        : APP_WHEEL_ITEMS - 2);
    return 2;
}

const char *SnippetContextLabel(const AppState *app, int construct, int index,
                                char *label, size_t label_capacity)
{
    int variable_index;
    if (!label || label_capacity == 0 || index < 0 ||
        index >= SnippetContextCount(app, construct))
        return NULL;

    if (index == 0) {
        if (construct == 0)
            return "range(10)";
        return "Insert construct";
    }

    if (index == 1) {
        if (construct == 0)
            return "Create items list";
        if (construct == 1)
            return "while True";
        if (construct == 2)
            return "if __name__";
        if (construct == 3)
            return "def helper";
        if (construct == 4)
            return "return None";
        if (construct == 5)
            return "print()";
        if (construct == 6)
            return "import os";
        return "class Example";
    }

    variable_index = (int)app->variable_count - (index - 1);
    if (variable_index >= 0 && variable_index < (int)app->variable_count) {
        _snprintf_s(label, label_capacity, _TRUNCATE, "for item in %s",
                    app->variables[variable_index]);
        return label;
    }
    return NULL;
}

BOOL SnippetBuildContext(const AppState *app, int construct, int index,
                         char *output, size_t capacity)
{
    char snippet[APP_SNIPPET_LENGTH];
    int variable_index;

    if (!output || capacity == 0 || index < 0 ||
        index >= SnippetContextCount(app, construct))
        return FALSE;

    if (construct == 0) {
        if (index == 0)
            return CopyText("for i in range(10):\n    ", output, capacity);
        if (index == 1)
            return CopyText("items = []\n", output, capacity);
        variable_index = (int)app->variable_count - (index - 1);
        if (variable_index >= 0 && variable_index < (int)app->variable_count) {
            _snprintf_s(snippet, sizeof(snippet), _TRUNCATE,
                        "for item in %s:\n    ", app->variables[variable_index]);
            return CopyText(snippet, output, capacity);
        }
        return FALSE;
    }

    if (index == 0)
        return SnippetBuildMain(construct, output, capacity);
    if (construct == 1)
        return CopyText("while True:\n    ", output, capacity);
    if (construct == 2)
        return CopyText("if __name__ == \"__main__\":\n    ", output, capacity);
    if (construct == 3)
        return CopyText("def helper():\n    ", output, capacity);
    if (construct == 4)
        return CopyText("return None\n", output, capacity);
    if (construct == 5)
        return CopyText("print()\n", output, capacity);
    if (construct == 6)
        return CopyText("import os\n", output, capacity);
    if (construct == 7)
        return CopyText("class Example:\n    pass\n", output, capacity);
    return FALSE;
}

static BOOL IsIdentifierStart(char character)
{
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z') || character == '_';
}

static BOOL IsIdentifierPart(char character)
{
    return IsIdentifierStart(character) ||
           (character >= '0' && character <= '9');
}

static void RememberVariable(AppState *app, const char *name, size_t length)
{
    unsigned int i;
    char candidate[APP_VARIABLE_NAME_LENGTH];
    if (length == 0 || length >= sizeof(candidate))
        return;
    memcpy(candidate, name, length);
    candidate[length] = '\0';

    for (i = 0; i < app->variable_count; ++i) {
        if (strcmp(app->variables[i], candidate) == 0)
            break;
    }
    if (i < app->variable_count) {
        if (i + 1 < app->variable_count) {
            memmove(app->variables[i], app->variables[i + 1],
                    (app->variable_count - i - 1) * sizeof(app->variables[0]));
        }
        strcpy_s(app->variables[app->variable_count - 1],
                 sizeof(app->variables[app->variable_count - 1]), candidate);
        return;
    }
    if (app->variable_count < APP_VARIABLE_LENGTH) {
        strcpy_s(app->variables[app->variable_count],
                 sizeof(app->variables[app->variable_count]), candidate);
        ++app->variable_count;
    } else {
        memmove(app->variables, app->variables + 1,
                (APP_VARIABLE_LENGTH - 1) * sizeof(app->variables[0]));
        strcpy_s(app->variables[APP_VARIABLE_LENGTH - 1],
                 sizeof(app->variables[APP_VARIABLE_LENGTH - 1]), candidate);
    }
}

static void LearnAssignments(AppState *app, const char *snippet)
{
    const char *line = snippet;
    while (*line) {
        const char *cursor = line;
        const char *start;
        const char *end;
        while (*cursor == ' ' || *cursor == '\t')
            ++cursor;
        start = cursor;
        if (IsIdentifierStart(*cursor)) {
            ++cursor;
            while (IsIdentifierPart(*cursor))
                ++cursor;
            end = cursor;
            while (*cursor == ' ' || *cursor == '\t')
                ++cursor;
            if (*cursor == '=' && cursor[1] != '=')
                RememberVariable(app, start, (size_t)(end - start));
        }
        while (*line && *line != '\n')
            ++line;
        if (*line == '\n')
            ++line;
    }
}

void SnippetRemember(AppState *app, const char *snippet)
{
    size_t length = strlen(snippet);
    if (app->history_count < APP_HISTORY_LENGTH) {
        memcpy(app->history[app->history_count], snippet, length + 1);
        ++app->history_count;
    } else {
        memmove(app->history, app->history + 1,
                (APP_HISTORY_LENGTH - 1) * sizeof(app->history[0]));
        memcpy(app->history[APP_HISTORY_LENGTH - 1], snippet, length + 1);
    }
    LearnAssignments(app, snippet);
}
