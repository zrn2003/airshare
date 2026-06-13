# AirShare Integration Test Matrix

Run these manual tests on x64 Windows with real audio hardware.

## Prerequisites

- Build Release x64
- At least two audio output devices (wired, Bluetooth, or mixed)
- System audio playing from Spotify, YouTube, or a local media player

## Test cases

| ID | Scenario | Steps | Expected |
|----|----------|-------|----------|
| T1 | Launch | Start `AirShare.App.exe` | Window opens, devices listed |
| T2 | Refresh | Click **Refresh** | Device list updates |
| T3 | Two wired | Select 2 wired outputs, Start Sharing | Same audio on both devices |
| T4 | Mixed BT + wired | Select 1 BT + 1 wired, Start Sharing | Audio on both; latency values shown |
| T5 | Master volume | Adjust master slider while sharing | Volume changes on all devices |
| T6 | Device volume | Adjust per-device slider | Only that device changes |
| T7 | Stop | Click **Stop Sharing** | Playback stops on all devices |
| T8 | Disconnect | Unplug/disable one device while sharing | Remaining device continues; refresh shows change |
| T9 | Reconnect | Re-enable device and refresh | Device reappears; can share again |
| T10 | Settings persist | Change buffer size/theme, restart app | Settings restored from SQLite |
| T11 | Long session | Share for 30+ minutes | No crash, stable playback |
| T12 | Sync check | Clap test with phone recording | Inter-device delay ideally under 20 ms after compensation |

## NFR notes

- Target end-to-end latency: under 100 ms
- Target inter-device sync spread: under 20 ms
- Recommended MVP demo device count: 2–4

## Troubleshooting

- **No devices**: Check Windows Sound settings; ensure outputs are enabled.
- **No audio**: Confirm default playback device is producing sound; loopback captures the default render endpoint.
- **DLL missing**: Build the native project first and confirm `AirShare.AudioEngine.dll` is next to the EXE.
- **Bluetooth drift**: Reconnect device and refresh; latency is stored per device in SQLite.
