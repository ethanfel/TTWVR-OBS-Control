# TTWVR OBS Control

Save the last few minutes of gameplay, or start and stop OBS recording and streaming from your Pip-Boy. The OBS tab shows a preview of your current scene and whether you’re recording or live.

## Install

1. Download the mod ZIP from the [latest release](https://github.com/ethanfel/TTWVR-OBS-Control/releases/latest). Install and enable it in the same mod profile as TTWVR 1.1.7 and JIP LN NVSE.
2. On the gaming PC, open OBS → **Tools → WebSocket Server Settings**. Enable the server and leave authentication on. The addon reads the password from OBS.
3. Launch Fallout and open **DATA → OBS**.

### Portable OBS on Windows

In `Data/NVSE/Plugins/TTWVROBS.ini`, set `ConfigFile` to the full path of the portable copy’s WebSocket settings. Replace `D:\OBS` with your portable OBS folder.

```ini
[OBS]
ConfigFile=D:\OBS\config\obs-studio\plugin_config\obs-websocket\config.json
```

Standard Windows installs can leave `ConfigFile` blank. For native Linux OBS with Proton, use the path to the same file as seen by Wine.

## Controls

- **WristBoy** — Move one tab past **Radio**. Press **A** to start or stop recording, **X** for streaming, or **Y** for replay.
- **PauseBoy** — Select **[ OBS ]**, then select the action on screen.

## Save a replay

In OBS, open **Settings → Output**, enable **Replay Buffer**, and set **Maximum Replay Time** to **300** (5 minutes) or **600** (10 minutes). If OBS shows a memory limit, allow enough for your recording quality.

Before playing, select **Start replay buffer** in the Pip-Boy. After a moment you want to keep, select **Save replay**. **Clip saved** means OBS has written the clip to your recording folder. The buffer keeps rolling after each save. To start buffering whenever OBS opens, add `--startreplaybuffer` to your OBS shortcut.
