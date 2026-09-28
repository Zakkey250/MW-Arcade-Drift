# MW Arcade Drift — Core (0.1.1)

This is the complete in-game MOD. It contains no .exe files and does not require the optional editor, ConfigCheck.exe, .NET, an installer or an internet connection. The .asi is a native x86 DLL loaded by your existing ASI loader; it is executable code, not a data-only mod.

Requirements: NFS Most Wanted (2005) with an existing compatible ASI loader and one of these exact executables: NFSPatcher English 1.3 (6,029,312 bytes; SHA-256 80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C), its supported 4GB-patched variant (6,029,312 bytes; SHA-256 B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06), or Redux V3.04 Main (5,926,912 bytes; SHA-256 0C5675A08CD71FD6D31CA87E992A915054BD8B80D268BFF0561D7ECC2067E342). No game executable, game resources or ASI loader is included. Other executable variants are rejected.

Close the game and extract the scripts folder into the game directory. For an update, back up your settings; replace MWArcadeDrift.asi but keep existing MWArcadeDrift.ini, default_*.json, settings.json and vehicles/ profiles. The archive never includes personal vehicle profiles or saves. Retire any old MWCriterionDrift.asi to avoid loading two copies.

The core provides drifting, camera effects, wheel/RPM presentation, three-color slip HUD, Ctrl+D mode switching and localized generic messages. Ctrl+D ON reloads settings. First vehicle entry copies the two defaults to scripts/MWArcadeDrift/vehicles/<internal-model-name>/. Existing car profiles are preserved.

Redux V3.04 compatibility is provisional. If NFSMWOrbitCamera.asi is installed, set [Camera] Enabled=0 in MWArcadeDrift.ini before launching; both mods target the same camera function. Drift assistance, presentation and HUD remain available. Redux in-game driving and coexistence still need confirmation.

You can edit JSON with a text editor while playing, save, then toggle Ctrl+D OFF and ON. Each drift.json has enabled=true/false for that vehicle; camera.json has its own enabled switch. Keep schemaVersion=2 and the supplied numeric fields. The ASI itself validates files; invalid settings disable assistance for that vehicle and write the reason to scripts/MWArcadeDrift.log. Fix the file and turn ON again. Set settings.json language to auto, ja or en. The profile directory must be writable for automatic creation and locking.

HUD: green = enabled, amber = drifting, gray = disabled. Default changes affect newly created or missing vehicle files only. See the included configuration guides for the settings and behavior.

Optional download: MWArcadeDrift-Editor-0.1.1.zip adds a slider UI only. It is not required to play or to reload edited JSON. It contains Configurator.exe, its ConfigCheck.exe save validator, and fields.json. Installing or removing that add-on does not change game settings or core functionality. Both downloads are offline; no automatic download or update is implemented.

Verification: package and profile tests passed. In the tested alpha.4 build, a user driving session and its 51,105 telemetry rows showed no conspicuous problem; the HUD and camera ran throughout. v0.1.1 retains the established drift handling and adds exact Redux V3.04 executable compatibility plus a faster player-vehicle gate. Isolated gate timing is not an in-game FPS result. FWD visuals, the disabled gray HUD state, and Redux in-game driving were not established by that run.

Recommended tuning order: first adjust the car's in-game Performance Tuning (Handling). If that is insufficient, use the optional editor for the default or vehicle-specific drift profile. Save, then toggle Ctrl+D OFF and ON to reload.
