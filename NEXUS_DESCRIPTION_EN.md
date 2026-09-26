# Short description

Brake-initiated arcade drifting for NFS Most Wanted (2005), with an NFS 2015-inspired action camera, slip indicator, per-car profiles, and an optional editor.

# Full description

MW Arcade Drift v0.1.0 adds brake-initiated arcade drifting to NFS Most Wanted (2005). Brake while steering to enter a drift. Use the handbrake for a deeper entry or extra angle mid-drift, then counter-steer and straighten to finish the slide. A green/amber/gray mode indicator and visible counter-steering complete the effect. RWD vehicles show rear-wheel spin, AWD/4WD vehicles show all-wheel spin, and FWD vehicles retain counter-steering without added wheelspin.

**NFS 2015-inspired action camera:** The drift camera brings a more dramatic view to each slide. Enable or disable the camera effect independently in the camera settings; drift handling remains available either way.

**Tuning:** Start with the car's in-game Performance Tuning (Handling). If you still want a different balance, use the optional editor to adjust the default or individual car's drift profile. The game creates a profile on first vehicle entry. Ctrl+D toggles the mode; turning it back ON reloads saved profiles. The editor separates Drift and Camera settings, offers simple and advanced controls, supports English and Japanese, and protects unsaved changes. The Core download works alone and contains no EXE.

**Requirements:** NFS Most Wanted (2005), an existing ASI loader, and the NFSPatcher English 1.3 executable (SHA-256 `80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C`) or its supported 4GB-patched variant (`B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06`). Other executables are not supported. The game and ASI loader are not included.

**Install:** Close the game, then extract Core's `scripts/` folder into the game directory. Keep existing `MWArcadeDrift.ini` and JSON profiles when updating. Install Editor separately only if you want its slider UI; it requires .NET Framework 4.x. Do not load the old `MWCriterionDrift.asi` alongside `MWArcadeDrift.asi`.

The mod currently affects the player vehicle only. AI racers and police are unchanged. It aims for an arcade feel inspired by Criterion NFS games, rather than a one-to-one physics reproduction. The reviewed alpha.4 driving session showed no conspicuous issue or mod fault. FWD visual behavior was not covered by that session.

Source and updates: https://github.com/Zakkey250/MW-Arcade-Drift
