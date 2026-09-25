# MW Arcade Drift

Brake-initiated arcade drifting for **Need for Speed: Most Wanted (2005)**. Core **0.1.0** includes a smooth drift camera, a small slip indicator, counter-steering visuals and per-car JSON profiles. The optional Editor is **0.1.1**. The handling aims for a Criterion-inspired arcade feel; it does not reproduce another game's physics exactly.

**Tuning order:** Start with the car's in-game Performance Tuning (Handling). If you need finer control, use the optional editor to adjust the default or per-car drift profile. Save, then toggle drift mode OFF and ON with **Ctrl+D** to reload.

**調整の順序：** まずゲーム内の「パフォーマンスチューン」でハンドリングを調整してください。足りない場合に、任意の編集アプリでデフォルトまたは車種別のドリフトプロファイルを調整します。保存後は **Ctrl+D** でOFF→ONに切り替えて再読込します。

Brake while steering to enter a drift. The handbrake adds entry or mid-drift angle. RWD uses rear-wheel spin effects, AWD/4WD uses all four wheels, and FWD retains counter-steering without added wheelspin. The camera and wheel effects apply to the player vehicle; AI racers and police are unchanged.

## Downloads

- [Core 0.1.0](https://github.com/Zakkey250/MW-Arcade-Drift/releases/tag/v0.1.0): required, contains the ASI, defaults and guides. **No EXE** is included.
- [Editor 0.1.1](https://github.com/Zakkey250/MW-Arcade-Drift/releases/tag/editor-v0.1.1): optional standalone release with a Windows slider UI for Drift and Camera settings. Requires .NET Framework 4.x. The Core works without it.

Close the game and extract Core's `scripts/` folder into the game directory. Preserve existing `MWArcadeDrift.ini` and `scripts/MWArcadeDrift/*.json` profiles on updates. Extract Editor's `scripts/` folder separately if desired; it adds no gameplay files. Remove a legacy `MWCriterionDrift.asi` before loading the new ASI.

Supported executable: NFSPatcher English 1.3 `speed.exe`, 6,029,312 bytes, SHA-256 `80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C`, or the supported 4GB-patched variant `B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06`. An existing compatible ASI loader is required. Other executable variants are rejected. The game executable, game resources and ASI loader are not distributed here.

The previous alpha.4 build had a user drive session with no conspicuous issue; its log recorded active HUD drawing, camera use and 288 completed drifts without a mod fault. FWD visuals and the disabled gray icon were not established by that session. Core 0.1.0 keeps the same gameplay implementation. Editor 0.1.1 fixes saved Simple slider positions and remembers Simple/Advanced mode separately for Default and each vehicle. See [Core release notes](RELEASE_NOTES_0.1.0.md) and [Editor release notes](EDITOR_RELEASE_NOTES_0.1.1.md).

## Building

The native ASI and tests use Visual Studio 2022 C++ x86 tools and the Windows SDK. In PowerShell, run `tools/Build.ps1`, then `tools/Package-Split.ps1` to create separate Core and Editor ZIPs. This repository contains source, defaults and third-party dependency licenses; it contains no game assets or personal save profiles. The original project source has no reuse license specified. Bundled MinHook and nlohmann/json retain their own licenses.

---

日本語：ブレーキ操作からドリフトへ入り、ハンドブレーキで角度を追加できます。コアのみで走行・カメラ・アイコン・Ctrl+D切替・車種別設定が動作します。編集アプリは任意です。導入と設定の詳細は [日本語ガイド](packaging/CORE_JA.md) と [設定ガイド](CONFIGURATION_JA.md) を参照してください。
