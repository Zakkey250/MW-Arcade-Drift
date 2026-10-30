# MW Arcade Drift

**MW Arcade Drift 0.2.0** for **Need for Speed: Most Wanted (2005)** adds Arcade and Special drift modes, a drift camera, a slip indicator and per-car settings. **MW Arcade Drift AI is included as part of the same MOD.** The optional **MW Arcade Drift Editor 0.2.0** is a separate download.

Press **Ctrl+D** to cycle **Normal → Arcade → Special → Normal**. Arcade uses brake-and-steering drift entry; Special offers a more slippery setup using adjustable tyre grip and drag. Neither requires separately editing the car's handling data. The handling does not reproduce another game's physics exactly.

**Ctrl+D：ノーマルモード → アーケードモード → スペシャルモード → ノーマルモード。** アーケードはブレーキと操舵からドリフトへ入り、スペシャルはグリップと空気抵抗を調整した滑りやすい挙動です。両モードの設定は別々に保存できます。

## Downloads

- [MW Arcade Drift 0.2.0](https://github.com/Zakkey250/MW-Arcade-Drift/releases/tag/v0.2.0): required core, including the AI component, standard profiles and guides. No EXE is included.
- [MW Arcade Drift Editor 0.2.0](https://github.com/Zakkey250/MW-Arcade-Drift/releases/tag/editor-v0.2.0): optional Windows editor for Arcade/Special drift and camera settings. Requires .NET Framework 4.x.

See the [Core patch notes](RELEASE_NOTES_0.2.0.md) and [Editor patch notes](EDITOR_RELEASE_NOTES_0.2.0.md) for changes since 0.1.1.

Close the game and extract Core's `scripts/` folder into the game directory. For updates, replace **both** `MWArcadeDrift.asi` and `MWArcadeDriftAI.asi`, keep existing INIs, JSON settings and vehicle profiles, and add only missing defaults under `MWArcadeDrift/modes/special/`. Retire any legacy `MWCriterionDrift.asi`. Install the Editor separately if desired; its archive contains no gameplay files or profiles.

更新時はゲームを終了し、2つのASIを更新してください。既存INI・JSON・車種別設定を保持し、スペシャル用の標準設定は不足ファイルだけ追加します。導入の詳細は [日本語ガイド](packaging/CORE_JA.md)、調整は [設定ガイド](CONFIGURATION_JA.md) を参照してください。

EventLoader can select Special for a compatible event and restore the previous mode afterward. Its NeoAI can request drift assistance for selected cars. Installing this MOD alone does not change every AI opponent or police vehicle. EventLoader and events are not included. Special race AI uses its own calibration; editing a player Special profile does not retune opponents.

## Compatibility and configuration

Supported executables are NFSPatcher English 1.3 (6,029,312 bytes; SHA-256 `80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C`), its supported 4GB variant (`B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06`), and Redux V3.04 Main (5,926,912 bytes; `0C5675A08CD71FD6D31CA87E992A915054BD8B80D268BFF0561D7ECC2067E342`). A compatible ASI loader is required and is not included. Other executable variants are rejected. No game executable, game assets or personal saves are distributed.

Redux compatibility remains provisional. If `NFSMWOrbitCamera.asi` is installed, set `[Camera] Enabled=0` in `MWArcadeDrift.ini` before launching to avoid competing camera controls.

Use the optional editor or edit JSON directly. Select the mode and vehicle, save, then cycle Ctrl+D back to that mode. Existing vehicle profiles take precedence over defaults. Set `[Input] ControlDevice=PadKeyboard` or `Wheel` in `MWArcadeDrift.ini` and restart for your input device. Diagnostic logs are OFF by default in both components. See the [English configuration guide](CONFIGURATION_EN.md).

## Building

Use Visual Studio 2022 C++ x86 tools and the Windows SDK. Run `tools/Build.ps1` to build and test the core, bundled AI component and editor, then `tools/Package-Split.ps1` to create the two release ZIPs.

## License

Original project code, configuration defaults and documentation are licensed under the [MIT License](LICENSE). Bundled MinHook and nlohmann/json retain their own licenses. The MIT License grants no rights to *Need for Speed* or other third-party material.
