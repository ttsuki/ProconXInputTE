# ProconXInputTE

[TETRIS EFFECT](http://www.tetriseffect.game/)を**振動ありで**SwitchのProCon🎮で遊びたかったので作りました！

---

## About - なんこれ？

ViGEm busを経由して、USB接続のNintendo Switch Pro ControllerをXBox controllerの入力に変換するドライバーアプリです。

なんと、**振動に対応しています！**


## Features - 主な機能

- スティック・ボタン入力
- 振動出力
- A/B, X/Y ボタン入れ替え: `--use-x360-layout`オプションをつけて起動

## Download - ダウンロード

[Releases](https://github.com/ttsuki/ProconXInputTE/releases/latest)ページから最新版をダウンロードできます.

## How to use - つかいかた

 0. [ViGEm Bus Driver](https://github.com/ViGEm/ViGEmBus/releases)が必要なので、インストールします。
 1. 必要に応じて [HidHide](https://github.com/ViGEm/HidHide/releases) をインストールして設定します。(特定アプリ以外から、ProConを見えなくしてくれます)
 2. プロコンをUSBケーブルでPCにつなぎます。
 3. [ProconXInputTE_x86.exe](https://github.com/ttsuki/ProconXInputTE/releases/latest)を起動します。
 4. お気に入りのゲームを遊びましょう😊

---


## For developpers

### Build Environment

  - Visual Studio 2019/2022 with CMake


### Requirements

  - ViGEm Bus Driver: https://github.com/ViGEm/ViGEmBus


### Third-party library (submodules)

  - ViGEm Client: https://github.com/ViGEm/ViGEmClient


### Sub-projects - 副産物

- ViGEm C++ Client (RAII wrapper)

    [`ViGEmClient/`](ViGEmClient/) ディレクトリにあります。

- Pro Controller user-mode driver

    [`ProControllerHid/`](ProControllerHid/) ディレクトリにあります。

    - スティック・ボタン入力の他、6軸IMU(加速度・ジャイロ)センサ入力、振動出力をサポートしています。

    - このプロジェクト単体で利用する場合は、ViGEm Clientは不要です。


### Thanks to

  - Reverse Engineering Note: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering


---

## License

  MIT (C) 2019 ttsuki  
  https://github.com/ttsuki/ProconXInputTE/  

  The files in the directory `ThirdParty/ViGEmClient` are separated LICENSEed by [ThirdParty/ViGEmClient/LICENSE](ThirdParty/ViGEmClient/LICENSE).