# Plant Irrigation System — Agent Briefing

## What This Project Is

A simple web dashboard for an automated plant irrigation system. The ESP32 microcontroller reads sensors and POSTs the data to this Flask server. The dashboard displays live sensor readings and allows manual pump control.

No authentication, no database — intentionally kept simple so the ESP32 can talk to it directly over the local network.

---

## Project Structure

```
PlantIrrigationSystem/
├── app.py              # Flask app — all routes live here
├── templates/
│   └── index.html      # Dashboard (sensor readings + pump control)
└── static/
    └── css/            # (empty for now, dashboard styles are inline)
```

---

## How to Run

```bash
pip install flask
python app.py
# Server runs at http://0.0.0.0:5000
```

---

## Routes

| Method | URL | Description |
|--------|-----|-------------|
| GET | `/` | Serves the dashboard |
| POST | `/toggle-pump` | Toggles pump ON/OFF, returns `{"status": "ON"}` or `{"status": "OFF"}` |

---

## Dashboard (index.html)

Displays a 2-column grid with six tiles:
- **Moisture Level** — hardcoded placeholder (`45%`)
- **Time Since Last Irrigation** — hardcoded placeholder (`02:15:30`)
- **Temperature** — hardcoded placeholder (`23°C`)
- **Humidity** — hardcoded placeholder (`55%`)
- **Water Pump Status** — live, updated via JS fetch to `/toggle-pump`
- **Current Time & Date** — live, updated every second via JS

---

## ESP32 Integration — What Needs to Be Built

### The ESP32's job
The ESP32 reads two sensors and sends the data to this Flask server:
1. **Soil moisture sensor** — soil moisture level
2. **Light sensor (LDR or similar)** — detects daylight vs. darkness

### Irrigation logic
**Water the plant ONLY when BOTH are true:**
- Soil moisture is **below a threshold** (soil is dry)
- **No daylight** detected (it is dark / nighttime)

**Do NOT water if either condition is false** — moisture is fine, or it's daytime (watering in sunlight wastes water and can scorch leaves).

```
if moisture < MOISTURE_THRESHOLD and light_level < LIGHT_THRESHOLD:
    turn_pump_on()
else:
    turn_pump_off()
```

### What still needs to be added to Flask

1. **POST `/sensor-data`** — ESP32 POSTs sensor readings here on a regular interval (moisture, light level, temperature, humidity).
2. **GET `/sensor-data`** — dashboard polls this to fetch the latest readings and update the tiles in real time (replacing the hardcoded placeholders).
3. **GET `/pump-command`** — ESP32 polls this to know whether to activate the physical relay. Server applies the irrigation logic and returns `ON` or `OFF`.

### What still needs to be updated in the dashboard

Replace the hardcoded tile values with a `setInterval` fetch to `/sensor-data` (same pattern as the clock already does).
