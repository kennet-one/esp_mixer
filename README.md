# esp_mixer

ESP-IDF firmware for the KeeMASH environmental dashboard and gesture
controller. The node keeps its original display workflow and legacy KeeMASH
sensor replies while using the shared reliable V2 mesh runtime.

## Target

- ESP32-D0WDQ6 revision 1.0
- 4 MB flash
- ESP-IDF 6.0.1
- 240 MHz, no PSRAM
- A/B OTA layout with rollback
- `keemash_mesh_core` v0.6.2

## Hardware

| Device | Interface | Pins/address |
|---|---|---|
| SSD1327 128x128 display | SPI2 | SCLK GPIO18, MOSI GPIO23, CS GPIO5, DC GPIO2 |
| DHT22 | single wire | GPIO4 |
| MH-Z19B | UART2, 9600 baud | RX GPIO16, TX GPIO17 |
| MAX44009 | I2C | `0x4A` |
| MAX30105 | I2C | `0x57` |
| DS3231 | I2C | `0x68` |
| PAJ7620 | I2C | `0x73` |
| Shared I2C bus | I2C0 | SDA GPIO21, SCL GPIO22 |

Pressure telemetry is intentionally absent because the assembled device does
not currently have a supported barometer.

All pins are configurable in `menuconfig`. Mesh credentials are local
configuration and must never be committed.

## Runtime

- Sensors are sampled outside the display render path.
- The display renders an immutable cached snapshot at 10 Hz.
- A sensor failure does not block the display, mesh, or other sensors.
- Unavailable values render as `--`.
- The pulse sensor LED is enabled only on the pulse screen.
- Reliable V2 carries node info, topology, logs, tasks, memory, controls,
  typed sensor snapshots, time, and remote OTA.
- Legacy replies remain `04...` (CO2), `05...` (temperature), `06...`
  (humidity), and `07...` (lux).

## Build And Flash

From the MASH workspace:

```powershell
tools\idf.cmd -ProjectPath "C:\Users\kennet\Desktop\To Git\esp_mixer" reconfigure
tools\idf.cmd -ProjectPath "C:\Users\kennet\Desktop\To Git\esp_mixer" build
tools\idf.cmd -ProjectPath "C:\Users\kennet\Desktop\To Git\esp_mixer" -Port COM10 flash
```

The first ESP-IDF installation requires USB because it replaces the old
single-app Arduino layout. Later updates use remote OTA v2 through `node0`.

## Validated Baseline

- Display initializes and renders on the assembled device.
- DHT22, MH-Z19B, MAX30105, and DS3231 initialize successfully.
- Reliable mesh works both directly and through an ESP-MESH parent.
- Task, CPU, RAM, flash, uptime, topology, and log telemetry are visible in
  the `node0` web UI.
- Remote OTA v2 switches A/B slots and the node returns automatically.
- `ppm_echo`, `temp_echo`, `humi_echo`, and `sens_echo` return valid legacy
  lines through the KeeMASH serial path.

At the latest hardware check MAX44009 (`0x4A`) and PAJ7620 (`0x73`) did not
answer on I2C. The firmware keeps running and reports those values as
unavailable. The I2C scan also saw an unidentified device at `0x5F`.

## Third-Party Code

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
