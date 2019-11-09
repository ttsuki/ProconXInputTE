# ProconXInputTE

I wanted to play [TETRIS EFFECT](http://www.tetriseffect.game/) with Nintendo Switch Pro controller :video_game: with vibration!!!

---

## About

An XInput user-mode driver for Nintendo Switch Pro Controller (USB-wired mode) using ViGEm.

## Features

- USB wired mode Pro Controller user-mode driver.
- XInput support with ViGEm.
- Vibration support.

## Download

See [Releases](https://github.com/ttsuki/ProconXInputTE/releases/latest) page.

## How to use

 0. Install [ViGEm Bus Driver](https://github.com/ViGEm/ViGEmBus/releases).
 1. Connect your Nintendo Switch Pro Controller with USB cable to PC.
 2. Start the driver application [ProconXInputTE_x86.exe](https://github.com/ttsuki/ProconXInputTE/releases/latest).
 3. Play your favorite game :blush:


---

# For developpers

Also you can use this project as a Nintendo Switch Pro controller :video_game: user mode driver SDK.  
This drivers supported 6-axis accelerometer/gyroscope sensor inputs (but disabled for XInput).  
See the `ProControllerHid` project directory and [a test code](/ProconXInputTE/Tests/ProControllerTest.cpp).  

## Build Environment
 - Visual Studio 2019

## Requirements
 - ViGEm Bus Driver: [https://github.com/ViGEm/ViGEmBus](https://github.com/ViGEm/ViGEmBus/)

## Third-party library / submodules
 - ViGEm Client: https://github.com/ViGEm/ViGEmClient

## Thanks to
 - Reverse Engineering Note: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
 
---
## Bugs
  - Unstable. Suddenly stops input or stops output(rumbling)...

---
## License
 MIT (C) 2019 ttsuki  
 https://github.com/ttsuki/ProconXInputTE/  
