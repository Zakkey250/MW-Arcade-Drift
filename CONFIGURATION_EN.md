# MW Arcade Drift 0.2.0 — Configuration guide

## Modes

Ctrl+D cycles Normal → Arcade → Special → Normal. Startup defaults to Arcade. In MWArcadeDrift.ini, [Drift] InitialMode=0/1/2 selects the starting mode. Enabled=0 starts in Normal.

Arcade enters a drift with braking while steering; the handbrake adds angle. Special uses adjustable tyre grip and drag for a more slippery setup. Results vary with the car, drivetrain and speed, so tune to taste. Normal disables drift assistance.

Compatible EventLoader events can select Special automatically, then restore your previous mode. Manual cycling is unavailable while the event owns the mode.

## Editor and profiles

Install the optional Editor 0.2.0 and run scripts/MWArcadeDrift/Configurator.exe using Windows .NET Framework 4.x. Select Arcade mode or Special mode, then Default or a vehicle, and use the Drift/Camera tabs. The active file path is shown in the window.

Arcade profiles are in scripts/MWArcadeDrift/. Special profiles are under modes/special/. Each family has default_drift.json, default_camera.json and vehicles/<model>/drift.json and camera.json. Shared language and HUD settings remain in the top-level settings.json.

First vehicle entry creates only missing per-car files from that mode's defaults. Existing vehicle files take precedence and are never overwritten by changing defaults. Select the vehicle to adjust an existing profile. Reload refreshes the vehicle list.

After saving, cycle Ctrl+D back to the edited mode to reload. Pause/resume alone does not reload settings. If EventLoader owns the mode, leave the event before cycling. The editor need not remain open. A text editor can also edit the JSON files.

## Special controls

| Control | Meaning |
| --- | --- |
| Front tyre grip | Percentage of original front grip. |
| Rear sliding grip | Rear grip used during sliding, including braking and handbrake conditions. |
| Rear grip limit — drive and lateral | Limits combined rear tyre force; reducing it also reduces available drive force. |
| Rear lateral tyre grip | Affects rear lateral grip and related skid resistance. |
| Drag during drift | Percentage of aerodynamic drag during slides. Lower values reduce drag, without restoring speed lost to brakes or collisions. |

Both Simple and Advanced show actual percentages. Grip at 100 retains the original value. Adjustments fade out at low speed and apply temporarily during driving, without changing shared car data or saved handling values.

Special uses native wheel rotation and engine RPM. Special race AI has its own calibration; editing player Special profiles does not retune opponents.

## Arcade and camera controls

Arcade Simple mode groups settings into turning force, slip angle/handbrake and speed retention. Camera has distance, rotation/look-ahead and smoothing groups. A slider at 50 represents the shipped defaults. Advanced exposes individual parameters. Mixed Advanced values appear approximately in Simple; moving a grouped slider recalculates its related values.

Switching editing mode asks for confirmation. The last Simple/Advanced choice is saved per family and vehicle in editor_ui.json. Drift and camera have separate enable switches. A disabled vehicle profile remains disabled when selected with Ctrl+D.

Arcade wheel/RPM presentation follows the drivetrain: RWD adds rear-wheel spin, AWD/4WD adds spin on all wheels, and FWD retains native rotation.

## Input device

Set [Input] ControlDevice=PadKeyboard or Wheel in MWArcadeDrift.ini and restart. Missing or invalid values use PadKeyboard.

PadKeyboard smooths Arcade steering/throttle assistance, while braking responds immediately. In Special, only steering and countersteering are assisted; pedals remain native. Wheel bypasses these input corrections. The game's bindings and deadzone settings remain in use.

## HUD, save protection and diagnostics

HUD colors are green = enabled, amber = drifting, gray = disabled. Adjust placement, size and opacity in settings.json. Language options are auto/ja/en; English is used when Japanese cannot be displayed.

Unsaved changes have Save/Discard/Cancel protection. Saving validates values, checks external changes and creates backups/. Invalid profiles disable assistance for that vehicle until corrected and reloaded. Keep schemaVersion=2 for Arcade and all camera profiles, and 1 for Special drift profiles.

Diagnostic logging is OFF by default. For troubleshooting only, set [Diagnostics] Logging=1 in the relevant INI. Player driving telemetry also requires [Drift] Telemetry=1. Restore both to 0 afterward. Logs are created beside their ASIs in scripts/.
