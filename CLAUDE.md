# Plant Irrigation System — Agent Briefing

## What This Project Is

An automated plant irrigation system that runs entirely on an **ESP32 microcontroller**. The ESP32 reads sensors, applies irrigation logic, and serves a web dashboard over WiFi — no separate server needed.

## Stack

- **MicroPython** — runs on the ESP32
- **Microdot** — lightweight Flask-like web framework for MicroPython (must be uploaded to the ESP32 alongside these files)

---

## Project Structure

```
PlantIrrigationSystem/
├── main.py              # Entry point: WiFi connection, web server, sensor reading, irrigation logic
├── index.html           # Dashboard — served directly by the ESP32
├── secrets.py           # WiFi credentials (gitignored — copy from secrets.example.py)
├── secrets.example.py   # Template for secrets.py
└── CLAUDE.md
```

---

## How to Deploy

1. Install [Microdot](https://github.com/miguelgrinberg/microdot) — download `microdot.py` from the repo
2. Copy `secrets.example.py` to `secrets.py` and fill in your WiFi credentials
3. Flash MicroPython firmware to the ESP32 if not already done
4. Upload all files to the ESP32 (`main.py`, `index.html`, `microdot.py`, `secrets.py`)
5. The ESP32 will connect to WiFi on boot and print its IP address — open that in a browser

---

## Sensors & Pins

Defined at the top of `main.py` — adjust to match your wiring:

| Variable | Default Pin | Purpose |
|----------|-------------|---------|
| `moisture_adc` | GPIO 34 | Soil moisture sensor (analog) |
| `light_adc` | GPIO 35 | Light sensor / LDR (analog) |
| `pump_pin` | GPIO 26 | Pump relay (digital output) |

---

## Irrigation Logic

**Water the plant ONLY when BOTH are true:**
- Soil moisture raw ADC reading is **below `MOISTURE_THRESHOLD`** (soil is dry)
- Light sensor raw ADC reading is **below `LIGHT_THRESHOLD`** (it is dark — no daylight)

**Do NOT water if either is false** — moisture is sufficient, or it's daytime.

```python
if moisture < MOISTURE_THRESHOLD and light < LIGHT_THRESHOLD:
    pump_on()
else:
    pump_off()
```

Thresholds are defined in `main.py` and need to be tuned after testing with the actual sensors.

The irrigation check runs automatically every 60 seconds in a background async loop.

---

## Routes

| Method | URL | Description |
|--------|-----|-------------|
| GET | `/` | Serves the dashboard HTML |
| GET | `/sensor-data` | Returns `{moisture, light, pump}` as JSON |
| POST | `/toggle-pump` | Manually toggles pump, locks into manual mode |
| POST | `/auto-mode` | Clears manual override, returns to automatic logic |

---

## Dashboard (index.html)

- Polls `/sensor-data` every 5 seconds to update moisture, light, and pump status tiles
- **Toggle Pump** button — manually overrides the pump
- **Return to Auto** button — clears manual override and hands control back to the irrigation logic
- Clock updates every second via JS

---

## Known TODOs

- Thresholds (`MOISTURE_THRESHOLD`, `LIGHT_THRESHOLD`) need calibration with real sensors
- Temperature and humidity tiles were removed (would need a DHT11/DHT22 sensor — add if available)
- No persistent logging of irrigation events
