# touch2main

A spice2x `-k` DLL that redirects touches on the touchscreen (secondary monitor) to the primary monitor. No registry changes.

## Use
- 64-bit: `spice64.exe -k touch2main_x64.dll`
- 32-bit: `spice.exe -k touch2main_x86.dll`

Toggle with the tray icon (left click) or **Ctrl+Alt+T**. Right-click the tray icon to turn it off until the game restarts.
While enabled, the touchscreen itself can't be touched (an invisible overlay catches touches and replays them on the main screen).

## Build
Pushing to GitHub builds both DLLs automatically (Actions tab -> latest run -> Artifacts).
Local: in an "x64 Native Tools Command Prompt":
`cl /O2 /EHsc /LD /MT touch2main.cpp user32.lib shell32.lib /Fe:touch2main_x64.dll`
