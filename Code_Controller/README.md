# Controller Coder 0.01

A small, native Win32 controller-driven Python snippet wheel for Windows. The
app polls XInput on a dedicated 1 ms waitable-timer loop, displays a
non-activating layered overlay, and inserts the chosen snippet into the
foreground application using Unicode `SendInput`.

## Build

Install CMake and a Windows C compiler (Visual Studio or MinGW-w64), then run
from this directory:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

With MinGW, select its generator if it is not CMake's default:

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build --config Release
```

## Controls

- **RT:** Open the primary wheel. Move the left stick to choose a construct.
- **RT again:** Open contextual suggestions for the currently highlighted
  construct.
- **A:** Insert the highlighted item and close the overlay.
- **RT on the contextual wheel:** Return to the primary wheel.
- **B:** Cancel and close the overlay.
- **View/Back:** Exit the application.

The primary wheel positions are top `for`, top-right `while`, right `if / else`,
bottom-right `def`, bottom `return`, bottom-left `print()`, left `import`, and
top-left `class`.

The `for` contextual wheel always offers `range(10)` and `items = []`. Once
the tool itself has injected a simple assignment such as `items = []`, it also
offers loops over the variable names it has learned during the current run.
For safety and to avoid taking focus from the editor, this version does not
inspect editor buffers, clipboard contents, or code typed outside the tool.

## Notes

- Keep the target editor in the foreground. The overlay is created with
  `WS_EX_NOACTIVATE` so opening it should not steal focus.
- Windows restricts `SendInput` across integrity levels; run the target and
  this app at the same privilege level.
- The XInput DLL is loaded only from the Windows system directory. XInput
  polling runs independently of rendering, using a high-resolution waitable
  timer when available and a 1 ms timer-resolution request as a fallback; the
  overlay redraws at 60 Hz.
- The tool inserts literal starter templates (including indentation and
  trailing spaces on block snippets). It does not parse the target document or
  format code.
