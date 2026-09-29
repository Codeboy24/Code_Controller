#ifndef CONTROLLER_CODER_SNIPPETS_H
#define CONTROLLER_CODER_SNIPPETS_H

#include <stddef.h>

#include "app.h"

const WCHAR *SnippetMainLabel(int index);
BOOL SnippetBuildMain(int index, char *output, size_t capacity);
int SnippetContextCount(const AppState *app, int construct);
const char *SnippetContextLabel(const AppState *app, int construct, int index,
                                char *label, size_t label_capacity);
BOOL SnippetBuildContext(const AppState *app, int construct, int index,
                         char *output, size_t capacity);
void SnippetRemember(AppState *app, const char *snippet);

#endif
