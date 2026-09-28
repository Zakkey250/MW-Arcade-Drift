# Redux 3.04 Main 対応メモ

対象は `Need For Speed Most Wanted REDUX 3.04 - Main/speed.exe` の SHA-256
`0C5675A08CD71FD6D31CA87E992A915054BD8B80D268BFF0561D7ECC2067E342`
（5,926,912 バイト）に限定する。実行コードの対象セクションとドリフト側の
フックガードを照合済み。起動・走行での動作は別途確認する。

`tools/Install-Redux.ps1` は、既存の `dinput8.dll`、他の ASI、SAVE を保持して
`scripts/MWArcadeDrift.asi` と設定・既定プロファイルを配置する。作業証跡と
導入前ファイルのバックアップは `evidence/redux-install-日時` に保存する。

Redux に導入済みの `NFSMWOrbitCamera.asi` は、本 MOD のカメラフック対象と
同じアドレスを参照する。干渉を避けるため、Redux 用 INI では `[Camera] Enabled=0`
とする。ドリフト補助、車輪・回転数の演出、HUD は有効のまま。ゲーム内の
起動成功、車両でのドリフト・HUD・他 MOD との共存は未検証。

起動前には Redux の `speed.exe` が終了していることを確認する。導入後は
Redux の通常の起動方法で立ち上げ、`scripts/MWArcadeDrift.log` の `ACTIVE`
行と実走で確認する。異常時にはゲームを終了し、導入証跡の `previous` に
退避された同名ファイルがあれば復元する。新規導入で `previous` が空なら
今回配置した 5 ファイルのみを取り除き、車両別プロファイルなど後から作られた
ファイルは保全する。
