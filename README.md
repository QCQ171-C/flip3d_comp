# Flip3DComp

DirectComposition Flip3D switcher. A faster, more complete successor to the [flip3d](https://github.com/ALTaleX531/flip3d) D3D11 prototype.

## Requirements

- Windows with DWM and DirectComposition
- CMake 3.21+
- Visual Studio / MSVC with the Windows SDK

## Build

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Run

Launch `build/Release/Flip3DComp.exe`.

Eligible windows are shown as DWM shared thumbnails on a DirectComposition 3D carousel. No D3D11 scene pass, no Windows.Graphics.Capture.

## Controls

- `Tab` / `Shift+Tab`, arrow keys, mouse wheel: scroll the carousel
- `Enter` or left click a card: activate the selected window
- `Esc`: exit
- `F5`: replay the enter animation

## Exit

Right click the tray icon and choose the "Exit"

## Notes

- **vs [flip3d](https://github.com/ALTaleX531/flip3d):** compositor-native visuals (lower overhead), smooth fractional scroll while browsing, uDWM-aligned parallel exit rotation
- **More complete:** `IAccessible` + `NotifyWinEvent` accessibility, Shell Hook live card add/remove


`Credit to ALTaleX531`
