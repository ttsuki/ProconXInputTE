# ProconXInputTE

I wanted to play [TETRIS EFFECT](http://www.tetriseffect.game/) with Nintendo Switch Pro controller with vibration!!!

---

## About

An XInput user-mode driver for Nintendo Switch Pro Controller (USB-wired mode) using ViGEm.

## Features

- USB wired mode Pro Controller user-mode driver.
- XInput support with ViGEm.
- Vibration support.

## How to use

 0. Install [ViGEm Bus Driver](https://github.com/ViGEm/ViGEmBus/releases).
 1. Connect your Nintendo Switch Pro Controller with USB cable to PC.
 2. Start the driver application.
 3. Play your favorite game :blush:

---

## Build Environment
 - Visual Studio 2019

## Requirements
 - ViGEm Bus Driver: [https://github.com/ViGEm/ViGEmBus](https://github.com/ViGEm/ViGEmBus/)

## Third-party library / submodules
 - HIDAPI: https://github.com/signal11/hidapi
 - ViGEm Client: https://github.com/ViGEm/ViGEmClient
   - Boost.Asio (from NuGet package 1.70 *-src)

## Thanks to
 - Reverse Engineering Note: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering
 
---
## Bugs
  - Unstable. Suddenly stops input or stops output(rumbling)...

---
## License
 MIT
