# MW Arcade Drift Editor 0.1.1

This is an **optional editor-only update** for Core 0.1.0. Download `MWArcadeDrift-Editor-0.1.1.zip` and extract its `scripts/` folder into the game directory while the game and editor are closed. The ZIP contains `Configurator.exe`, `ConfigCheck.exe`, `fields.json`, documentation, and a dependency license. It contains no ASI, game executable, game assets, or default/vehicle JSON profiles. Existing profiles stay in place.

The Simple sliders now restore their positions from saved values after Save, Reload, and Simple/Advanced changes instead of resetting to 50. Here, 50 means the shipped default. A group with individually edited Advanced values may show an approximate Simple position; moving it recalculates that group from the shipped defaults.

Switching Simple/Advanced now asks for confirmation. The editor remembers the last mode independently for Default and every vehicle using an optional `editor_ui.json` file beside that profile's JSON files. The game ignores these editor-only files. Cancelling the switch leaves the mode and file unchanged. Existing Save/Discard/Cancel, validation, backups, and conflict protection remain in place.

The editor's isolated save/mode tests and extracted-package checks passed. In-game acceptance of this editor update remains separate from those offline checks.
