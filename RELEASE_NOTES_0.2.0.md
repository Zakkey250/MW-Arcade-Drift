# MW Arcade Drift 0.2.0

0.1.1からの変更点

- Ctrl+Dの切替を「ノーマルモード → アーケードモード → スペシャルモード → ノーマルモード」の3段階に変更しました。ノーマルではドリフト補助を停止します。
- 滑りやすさを重視したスペシャルモードを追加しました。前後のグリップと横滑り中の空気抵抗を調整でき、車両のハンドリングデータを別途編集する必要はありません。
- スペシャル専用の標準設定・車種別設定を追加しました。アーケード用とは別に保存でき、任意のEditor 0.2.0から調整できます。
- パッド・キーボード向けの入力補助と、ハンコン向けの直接入力設定を追加しました。スペシャルでは操舵とカウンターステアを補助します。
- EventLoaderに対応しました。対応イベントではスペシャルモードへ自動で切り替わり、解除後は元のモードへ戻ります。
- MW Arcade Drift AIを本体に同梱しました。EventLoaderのNeoAIが指定した車両で、アーケード／スペシャルのドリフト補助を利用できます。通常のAI車両すべてへ自動適用する機能ではありません。
- 診断ログは初期状態でOFFです。

更新時はゲームを終了し、同梱の2つのASIを更新してください。既存のINI・車種別設定は保持し、新しいスペシャル用の標準設定は未導入のファイルだけ追加してください。Editorは別ダウンロードです。EventLoader本体とイベントは含まれません。

---

Changes since 0.1.1

- Ctrl+D now cycles **Normal → Arcade → Special → Normal**. Normal disables drift assistance.
- Added Special mode for a more slippery drift setup, with adjustable tyre grip and drag during slides. No separate editing of car handling data is required.
- Added separate Special defaults and per-car profiles, editable with the optional Editor 0.2.0.
- Added gamepad/keyboard input assistance and a direct-input option for steering wheels. Special assists steering and countersteering.
- Added EventLoader support: compatible events can select Special automatically and restore your previous mode afterward.
- Bundled MW Arcade Drift AI as part of this MOD. EventLoader's NeoAI can use Arcade or Special assistance on selected vehicles; ordinary AI traffic and opponents are not automatically changed.
- Diagnostic logging is OFF by default.

Close the game before updating both included ASIs. Keep existing INIs and vehicle profiles; add only missing Special defaults. The Editor is a separate download. EventLoader and events are not included.
