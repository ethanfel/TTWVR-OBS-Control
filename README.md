# TTWVR OBS Control

Start or stop OBS recording and streaming from your Pip-Boy. The OBS tab shows a preview of your current scene and whether you’re recording or live.

## Install

1. Download the mod ZIP from the [latest release](https://github.com/ethanfel/TTWVR-OBS-Control/releases/latest). Install and enable it in the same mod profile as TTWVR 1.1.7 and JIP LN NVSE.
2. On the gaming PC, open OBS → **Tools → WebSocket Server Settings**. Enable the server and leave authentication on. The addon reads the password from OBS.
3. Launch Fallout and open **DATA → OBS**.

## Controls

- **WristBoy** — Move one tab past **Radio**. Press **A** to start or stop recording, or **X** to start or stop streaming.
- **PauseBoy** — Select **[ OBS ]**, then choose the recording or streaming button.

For portable OBS or native Linux OBS with Proton, set `ConfigFile` in `Data/NVSE/Plugins/TTWVROBS.ini` to the game-visible path of your OBS WebSocket `config.json`.
