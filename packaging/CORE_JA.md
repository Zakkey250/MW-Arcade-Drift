# MW Arcade Drift — コア (0.2.0)

MW Arcade Drift AIを含む本体です。コアだけで走行・カメラ・HUD・車種別設定が動作し、編集アプリや.NETは不要です。ゲーム本体、セーブ、ASIローダー、EventLoaderは含みません。対応実行ファイルはCORE_EN.mdを参照してください。

## 導入・更新

ゲームを終了してscriptsフォルダーをゲームフォルダーへ配置します。新規導入では同梱ファイルを使用してください。

0.1.1などから更新する場合はバックアップを取り、MWArcadeDrift.asiを更新し、MWArcadeDriftAI.asiとMWArcadeDriftAI.iniを追加します。AI部分も同じMODの構成ファイルなので、2つのASIを一緒に更新してください。既存のINI、default_*.json、settings.json、vehicles/は保持します。MWArcadeDrift/modes/special/の標準設定2ファイルは、未導入の場合のみ追加してください。既にスペシャルを試用している場合、その設定も保持します。旧名MWCriterionDrift.asiは二重読込を避けるため退避してください。

## 操作

Ctrl+Dで「ノーマルモード → アーケードモード → スペシャルモード → ノーマルモード」と切り替えます。初期状態はアーケードです。ノーマルは補助なし、アーケードは操舵とブレーキから進入するドリフト、スペシャルは滑りやすさを重視した設定です。ハンドブレーキでも滑り出しを助けられます。

設定はモード別・車種別です。初めて乗車すると標準設定から車種別ファイルを作成し、既存ファイルは上書きしません。任意のEditor 0.2.0またはテキストエディターで保存した後、Ctrl+Dを一周させて対象モードへ戻すと再読込します。詳しくはCONFIGURATION_JA.mdを参照してください。

EventLoaderの対応イベントでは自動的にスペシャルへ切り替わり、解除されると元のモードへ戻ります。イベント中の強制切替が有効な間は、手動切替を利用できません。AI補助はEventLoaderのNeoAIが指定した車両で使用します。このMODを導入しただけで全AI・警察・一般車の挙動が変わることはありません。

パッド・キーボードはMWArcadeDrift.iniの[Input] ControlDevice=PadKeyboard、ハンコンはControlDevice=Wheelを使用し、変更後は再起動してください。ゲーム側の操作割り当てを使用します。スペシャルのペダル入力は純正のままです。

HUDは緑=有効、アンバー=ドリフト中、グレー=無効です。言語・HUD位置などはsettings.jsonで設定できます。診断ログは両INIで初期状態OFFです。必要な場合だけ[Diagnostics] Logging=1にし、調査後は0へ戻してください。

Redux V3.04への対応は暫定です。NFSMWOrbitCamera.asiを使用している場合は、起動前にMWArcadeDrift.iniの[Camera] Enabled=0にしてください。

編集アプリは別ダウンロードMWArcadeDrift-Editor-0.2.0.zipです。今回の変更はRELEASE_NOTES.mdを参照してください。
