# MW Arcade Drift — Core (0.2.0)

The core includes MW Arcade Drift AI as part of the same MOD. Drifting, camera, HUD and per-car profiles work without the optional Editor or .NET. No game executable, game assets, saves, ASI loader or EventLoader is included.

## Requirements

Use an existing compatible ASI loader and one of these executables:

- NFSPatcher English 1.3: 6,029,312 bytes; SHA-256 80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C.
- Supported 4GB variant: 6,029,312 bytes; SHA-256 B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06.
- Redux V3.04 Main: 5,926,912 bytes; SHA-256 0C5675A08CD71FD6D31CA87E992A915054BD8B80D268BFF0561D7ECC2067E342.

Other executable variants are rejected. Redux compatibility remains provisional. If NFSMWOrbitCamera.asi is installed, set [Camera] Enabled=0 in MWArcadeDrift.ini before launching.

## Install or update

Close the game and extract scripts into the game directory. A new installation uses all included files.

When updating from 0.1.1, back up your settings, replace MWArcadeDrift.asi, and add MWArcadeDriftAI.asi and MWArcadeDriftAI.ini. Keep both ASIs at the same release version. Preserve existing INIs, default_*.json, settings.json and vehicles/. Add only missing default files under MWArcadeDrift/modes/special/. Existing Special preview profiles should also be kept. Retire any legacy MWCriterionDrift.asi to avoid duplicate loading.

## Play and configure

Ctrl+D cycles Normal → Arcade → Special → Normal. Startup defaults to Arcade. Normal disables assistance. Arcade enters a drift with steering and braking; Special uses a more slippery grip setup. The handbrake can also help initiate a slide.

Profiles are separate for each mode and vehicle. First vehicle entry creates missing per-car files from that mode's defaults without overwriting existing files. Save with the optional Editor 0.2.0 or a text editor, then cycle Ctrl+D back to the edited mode to reload. See CONFIGURATION_EN.md.

Compatible EventLoader events can force Special and restore your previous mode afterward. Manual cycling is unavailable while the event owns the mode. NeoAI can request assistance for selected cars; installing this MOD alone does not automatically change all opponents, police or traffic.

Use [Input] ControlDevice=PadKeyboard or ControlDevice=Wheel in MWArcadeDrift.ini and restart after changing it. Existing game control bindings remain in use. Special keeps native pedal input.

HUD colors are green = enabled, amber = drifting, gray = disabled. Language and HUD placement are in settings.json. Both components ship with diagnostic logging OFF. Enable [Diagnostics] Logging=1 only for troubleshooting and return it to 0 afterward.

MWArcadeDrift-Editor-0.2.0.zip is a separate optional download. See RELEASE_NOTES.md for changes since 0.1.1.
