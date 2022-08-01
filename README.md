[日本語READMEはこちら](README.ja.md)

# ProconXInputTE

  I wanted to play [TETRIS EFFECT](http://www.tetriseffect.game/) with Nintendo Switch Pro Controller 🎮 with vibration!!!


---

## About

  An XInput user-mode driver for Nintendo Switch Pro Controller (USB-wired mode) using ViGEm bus.

  This driver application supports VIBRATION!


## Features

  - Input sticks and buttons
  - Output Vibration
  - Swap buttons (A/B, X/Y) as x360 layout: start app with `--use-x360-layout` option.


## Download

  See [Releases](https://github.com/ttsuki/ProconXInputTE/releases/latest) page.


## How to use

  0. Install [ViGEm Bus Driver](https://github.com/ViGEm/ViGEmBus/releases).
  1. (optional) Install [HidHide](https://github.com/ViGEm/HidHide/releases) and configure for hiding controllers from other apps.
  2. Connect your Nintendo Switch Pro Controller with USB cable to PC.
  3. Start the driver application [ProconXInputTE_x86.exe](https://github.com/ttsuki/ProconXInputTE/releases/latest).
  4. Play your favorite game 😊

---

## For developpers

### Build Environment

  - Visual Studio 2019/2022 with CMake


### Requirements

  - ViGEm Bus Driver: https://github.com/ViGEm/ViGEmBus


### Third-party library (submodules)

  - ViGEm Client: https://github.com/ViGEm/ViGEmClient


### Sub-projects

- ViGEm C++ Client (RAII wrapper)

    is in [ViGEmClient/](ViGEmClient/) directory.

- Pro Controller user-mode driver

    is in [ProControllerHid](ProControllerHid/) directory.

    - It supports sticks/buttons input, 6-axis IMU sensor input, and basic rumbling output.

    - If you use it directly in your project, you do not need ViGEm Client.


### Thanks to

  - Reverse Engineering Note: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering

---

## License
  [MIT (C) 2019 ttsuki](LICENSE)  
  https://github.com/ttsuki/ProconXInputTE/  

  The files in the directory `ThirdParty/ViGEmClient` are separated LICENSEed by [ThirdParty/ViGEmClient/LICENSE](ThirdParty/ViGEmClient/LICENSE) .
