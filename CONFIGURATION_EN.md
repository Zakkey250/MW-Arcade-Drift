# MW Arcade Drift 0.1.0 — Profile editor

The editor is an optional separate download. The Core requires no EXE: edit JSON in a text editor, save, then switch Ctrl+D OFF and ON. UI instructions below apply only when the optional editor is installed.

Run `scripts/MWArcadeDrift/Configurator.exe` (.NET Framework 4.x). Choose Default or a vehicle at the top, then use separate Drift and Camera tabs. Each screen has three grouped sliders in Simple mode. Advanced exposes all 17 drift or 20 camera parameters and the corresponding enable switch. A Simple slider at 50 means the values at load; switching mode/language or saving establishes a new baseline.

First entry into a vehicle copies `default_drift.json` and `default_camera.json` to `vehicles/<internal model name>/drift.json` and `camera.json`. Existing files are preserved. Defaults only seed new/missing profiles. Use Reload to refresh the editor vehicle list.

Save, then switch drift mode OFF and ON using **Ctrl+D**. ON reloads that vehicle and common settings; there is no continuous file polling or reload on pause/resume. Setting drift `enabled` to false prohibits that vehicle from drifting, including when Ctrl+D is pressed. Re-enable the profile, save, then press ON. Camera has its own independent switch. Temporary Ctrl+D state is not saved to JSON.

HUD colors: green = enabled, amber = drifting, gray = disabled. Position/size/opacity are separate common HUD settings in `settings.json`, intentionally outside both tuning screens. Notifications use the game's generic messages. Language options are System, Japanese and English; unsupported system languages or unavailable Japanese glyphs fall back to English.

Unsaved edits trigger Save / Discard / Cancel on close, vehicle change, reload or folder change. Enter and Escape choose Cancel. Failed saves block closing and preserve the edit buffer. Before saving, the same C++ validator used by the mod checks ranges and cross-field constraints. External edits cause a conflict instead of an overwrite. Originals and target paths are stored in `backups/` before replacement. Failed replacement attempts restore originals. A shared file lock coordinates the game and editor. Forced termination/power loss does not restore unsaved buffers.

Old v1 files are preserved on migration; v2 uses the new default and vehicle files. Invalid profiles disable assistance for that vehicle until fixed and reloaded. Details are logged in `scripts/MWArcadeDrift.log`.

Offline checks cover controller regressions, profile generation, validation, save safeguards, independent D3D9 rendering and bilingual UI. The preceding alpha.4 game session recorded startup, Ctrl+D notifications and continuous HUD drawing without a reported visual defect. Default handling and camera values remain the previously approved baseline. The optional N2O API is deferred until a consumer contract is available.

## Drivetrain presentation (v0.1.0)
The native front torque share selects the presentation automatically. FWD retains counter-steering and native wheel/engine speed; no artificial wheelspin is added on either axle. RWD adds rear-wheel spin; AWD/4WD adds spin on all four wheels. Unknown/invalid layouts keep native rotation and RPM. Counter-steering and its smooth return remain enabled for every layout. This does not simulate new load transfer or change tire forces, torque distribution, handling values, or AT shift decisions.

The icon now renders to the D3D9 backbuffer from Present after player entry. No game render callsite is patched. Green = enabled, amber = drifting, gray = disabled. The alpha.4 drive session recorded successful HUD drawing and no reported visual defect. FWD was not driven in that session.

Tuning order: begin with the game's Performance Tuning (Handling). Use the optional editor for default or per-car profiles only if the in-game range is not enough. Save, then toggle Ctrl+D OFF and ON.
