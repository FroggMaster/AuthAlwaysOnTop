# AuthAlwaysOnTop
A lightweight utility that keeps the Windows **CredentialUIBroker** prompt always visible and in focus. It detects the window, brings it to the foreground, and ensures it stays on top, preventing it from getting hidden behind other windows during credential prompts.

Created out of necessity for browser extensions like **Bitwarden**, which cannot authoritatively bring the Windows authentication window to the front.

# Usage

Simply run `AuthAlwaysOnTop.exe`. The application will run silently in the background and monitor for the Windows **CredentialUIBroker** dialog. When detected, the utility will automatically:

- Bring the prompt window to the front.
- Force focus and top-most status, even if other applications attempt to cover it.
- Restore it from a minimized state if needed.

### Tray Icon

By default, the app displays a tray icon for quick access:

- **Right-click** the tray icon to:
  - Hide or show the tray icon.
  - View help.
  - Exit the application.

Tray icon visibility is persisted via a `config.ini` file stored in the same folder as the executable.

### Hotkey

A global hotkey is registered to toggle the tray icon:

**`Ctrl + Win + Alt + Scroll Lock`**

Use this hotkey to show or hide the tray icon at any time.

### Configuration

A `config.ini` file will be created automatically alongside the executable the first time you toggle the tray icon.

below is an example `config.ini`:
```ini
[Settings]
; TrayIconVisible determines whether the system tray icon is shown on launch. (use hotkey to toggle: Ctrl + Win + Alt + Scroll Lock)
; 1 = Show tray icon (default)
; 0 = Hide tray icon 
TrayIconVisible=1
