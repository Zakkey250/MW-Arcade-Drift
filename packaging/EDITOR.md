# MW Arcade Drift Editor 0.2.0

Install MW Arcade Drift 0.2.0 first. This optional archive updates only Configurator.exe, ConfigCheck.exe and fields.json in scripts/MWArcadeDrift/. It contains no ASI, INI, defaults or vehicle profiles.

Close the game and editor, then extract scripts into the same game directory. Run Configurator.exe with ConfigCheck.exe and fields.json beside it. Windows .NET Framework 4.x is required. The editor does not need to stay open while playing.

Choose Arcade mode or Special mode, then Default or a vehicle. Drift and Camera have separate tabs. Arcade offers grouped Simple sliders and individual Advanced controls. Special has five percentage controls in both views. Defaults seed new or missing vehicle files; existing per-car settings take precedence. Special race AI uses its own calibration and is not retuned by player Special profiles.

Save, then cycle Ctrl+D back to the edited mode in the game. Unsaved changes have Save/Discard/Cancel protection. Save validation, conflict detection and backups protect existing files. See EDITOR_CONFIGURATION_EN.md and EDITOR_RELEASE_NOTES.md.

To remove the editor, close it and remove only the three editor files. Leave INIs, JSON, vehicles/, modes/ and backups/ intact.

---

コア0.2.0を先に導入してください。ゲームと編集アプリを終了してscriptsフォルダーを展開し、3ファイルを更新します。このZIPに本体・INI・標準設定・車種別設定は含まれません。

「アーケードモード／スペシャルモード」から編集対象を選び、標準設定または車種別設定を編集します。保存後はゲーム内でCtrl+Dを一周させ、編集したモードへ戻すと反映されます。スペシャルのレースAIは専用調整を使うため、プレイヤー設定の変更は対戦相手へ反映されません。詳しくはEDITOR_CONFIGURATION_JA.mdを参照してください。

削除時はConfigurator.exe、ConfigCheck.exe、fields.jsonだけを削除してください。設定やバックアップを残せば、ゲームはそのまま利用できます。
