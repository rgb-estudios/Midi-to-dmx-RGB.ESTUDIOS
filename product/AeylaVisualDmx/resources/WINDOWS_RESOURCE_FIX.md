# Windows native-window resource fix

Confirmed on the target Windows machine on 2026-08-06:

- the previous `0.2.0` process remained alive;
- `MainWindowHandle` stayed zero for 15 seconds;
- resetting host settings and disabling audio did not create a window.

Root cause: the iPlug2 Windows APP target only compiles a native resource script when `resources/main.rc` exists. The previous AEYLA graphical branch defined resource identifiers in `resource.h` but did not provide `main.rc`, so `CreateDialog(IDD_DIALOG_MAIN, ...)` had no dialog template to instantiate.

Correction in `0.2.1-alpha.3`:

- add `resources/main.rc`;
- define `IDD_DIALOG_MAIN`, `IDD_DIALOG_PREF`, menu and accelerators;
- keep standalone no-audio startup;
- require CI to launch the executable and render `AEYLA_UI_SMOKE.png` before packaging.

The old `0.2.0` and launcher-only hotfixes are rejected artifacts and must not be redistributed.
