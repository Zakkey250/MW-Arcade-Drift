# MW Arcade Drift — Core (0.1.0)

This is the complete in-game MOD. It contains no .exe files and does not require the optional editor, ConfigCheck.exe, .NET, an installer or an internet connection. The .asi is a native x86 DLL loaded by your existing ASI loader; it is executable code, not a data-only mod.

Requirements: NFS Most Wanted (2005), supported English 1.3 executable (NFSPatcher target, 6,029,312 bytes; SHA-256 80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C or its 4GB-patched variant B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06), and an existing compatible ASI loader. No game executable, game resources or ASI loader is included. Other executable variants are rejected.

Close the game and extract the scripts folder into the game directory. For an update, back up your settings; replace MWArcadeDrift.asi but keep existing MWArcadeDrift.ini, default_*.json, settings.json and vehicles/ profiles. The archive never includes personal vehicle profiles or saves. Retire any old MWCriterionDrift.asi to avoid loading two copies.

The core provides drifting, camera effects, wheel/RPM presentation, three-color slip HUD, Ctrl+D mode switching and localized generic messages. Ctrl+D ON reloads settings. First vehicle entry copies the two defaults to scripts/MWArcadeDrift/vehicles/<internal-model-name>/. Existing car profiles are preserved.

You can edit JSON with a text editor while playing, save, then toggle Ctrl+D OFF and ON. Each drift.json has enabled=true/false for that vehicle; camera.json has its own enabled switch. Keep schemaVersion=2 and the supplied numeric fields. The ASI itself validates files; invalid settings disable assistance for that vehicle and write the reason to scripts/MWArcadeDrift.log. Fix the file and turn ON again. Set settings.json language to auto, ja or en. The profile directory must be writable for automatic creation and locking.

HUD: green = enabled, amber = drifting, gray = disabled. Default changes affect newly created or missing vehicle files only. See the included configuration guides for the settings and behavior.

Optional download: MWArcadeDrift-Editor-0.1.0.zip adds a slider UI only. It is not required to play or to reload edited JSON. It contains Configurator.exe, its ConfigCheck.exe save validator, and fields.json. Installing or removing that add-on does not change game settings or core functionality. Both downloads are offline; no automatic download or update is implemented.

Verification: package and profile tests passed. In the tested alpha.4 build, a user driving session and its 51,105 telemetry rows showed no conspicuous problem; the HUD and camera ran throughout. This v0.1.0 build changes the version string and ships the same handling code. FWD visuals and the disabled gray HUD state were not established by that run.

Recommended tuning order: first adjust the car's in-game Performance Tuning (Handling). If that is insufficient, use the optional editor for the default or vehicle-specific drift profile. Save, then toggle Ctrl+D OFF and ON to reload.
