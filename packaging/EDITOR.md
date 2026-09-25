# MW Arcade Drift — Optional Editor (0.1.1)

Install the Core download first. This optional archive adds only three editor files to scripts/MWArcadeDrift/: Configurator.exe, ConfigCheck.exe and fields.json. It does not contain an ASI, INI, defaults, vehicle profiles, game resources or a loader. Extract into the same game folder; no existing game configuration is replaced.

Run Configurator.exe. Requires Windows .NET Framework 4.x. Keep ConfigCheck.exe and fields.json alongside it: the editor uses them for validation and controls. The game never runs either EXE. No network/download/update functionality is included. Editing and running the game do not require keeping this app open.

Use separate Drift/Camera tabs and Default/vehicle selection. Simple mode has 3 sliders per screen; Advanced exposes the numeric parameters. Simple sliders restore their positions from saved values. The editor warns before switching modes and remembers the mode separately for Default and each vehicle in editor-only `editor_ui.json` files. Unsaved changes are protected by Save/Discard/Cancel, save validation, conflict detection and backups. After saving, toggle Ctrl+D OFF then ON in the game. See EDITOR_CONFIGURATION_EN.md for the full guide.

To remove the optional editor, close it and remove ONLY Configurator.exe, ConfigCheck.exe and fields.json from scripts/MWArcadeDrift/. Leave the JSON settings, vehicles/, backups/ and core files intact. The game continues to use your settings. You can edit them with a text editor and reload with Ctrl+D ON.

# 任意の編集アプリ

コアを先に導入してください。このZIPは scripts/MWArcadeDrift/ 配下へ上記3ファイルだけを追加します。ASI・INI・デフォルト・車種設定は含まず、既存の設定を上書きしません。同じゲームフォルダーへ展開してください。

.NET Framework 4.xが必要です。Configurator.exeと同じ場所にConfigCheck.exeとfields.jsonを置いてください。これらは編集・保存用で、ゲーム側は起動しません。ネット通信や自動更新機能はありません。

削除時は編集アプリを閉じ、上記3ファイルだけを削除します。JSON・vehicles/・backups/・コアを残せば、設定とゲーム内機能はそのまま使えます。操作の詳細はEDITOR_CONFIGURATION_JA.mdを参照してください。

Start with the car's in-game Performance Tuning (Handling). Use this optional editor when you need finer drift or camera adjustments. / まずゲーム内のハンドリング調整から始め、足りない場合にこの任意アプリで細かく調整してください。
