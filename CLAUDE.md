# Plant Irrigation System — Agent Briefing

## What This Project Is

A web-based dashboard for an automated plant irrigation system. Users log in, view live sensor data from an ESP32 microcontroller, and can manually toggle a water pump. The system also applies automatic irrigation logic based on sensor readings.

## Current State (as of 2026-09-09)

The **Flask backend and frontend are complete and working**. The **ESP32 firmware has been intentionally removed** from this repo — it was stripped out so the frontend/backend can be worked on independently. The ESP32 code will be written and flashed separately.

---

## Project Structure

```
PlantIrrigationSystem/
├── app.py              # Flask app — all routes live here
├── db_model.py         # SQLAlchemy User model (username + hashed password)
├── setup_db.py         # One-time script: run this to create instance/database.db
├── instance/
│   └── database.db     # SQLite database (auto-created by setup_db.py)
├── templates/
│   ├── Login.html      # Login form with flash message support
│   ├── Register.html   # Registration form with flash message support
│   └── index.html      # Main dashboard (sensor readings + pump control)
└── static/
    └── css/
        └── Auth.css    # Shared styles for Login and Register pages
```

---

## How to Run

```bash
# 1. Install dependencies (Flask, Flask-SQLAlchemy, Werkzeug)
pip install flask flask-sqlalchemy werkzeug

# 2. Create the database (only needed once)
python setup_db.py

# 3. Start the Flask server
python app.py
# Server runs at http://localhost:5000
```

---

## Routes

| Method | URL | Description |
|--------|-----|-------------|
| GET/POST | `/register` | Register a new user |
| GET/POST | `/login` | Log in |
| GET | `/` or `/index` | Main dashboard (shows username + sensor data) |
| POST | `/toggle-pump` | Manually toggle pump ON/OFF, returns `{"status": "ON"}` or `{"status": "OFF"}` |

**Auth:** Sessions are used (Flask `session`). `session['username']` is set on login. The index page falls back to `'Guest'` if no session exists — there is currently no login-required redirect guard on `/index`.

---

## Dashboard (index.html)

Displays a 2-column grid with six tiles:
- **Moisture Level** — currently hardcoded `45%` (placeholder)
- **Time Since Last Irrigation** — currently hardcoded `02:15:30` (placeholder)
- **Temperature** — currently hardcoded `23°C` (placeholder)
- **Humidity** — currently hardcoded `55%` (placeholder)
- **Water Pump Status** — live, updates via JS fetch to `/toggle-pump`
- **Current Time & Date** — live, updated every second via JS

The hardcoded values are placeholders. **The next step is to replace them with real data from the ESP32.**

---

## ESP32 Integration — What Needs to Be Built

### The ESP32's job
The ESP32 microcontroller reads two sensors and sends that data to the Flask server:
1. **Soil moisture sensor** — reads the moisture level of the soil (as a percentage or raw value)
2. **Light sensor (LDR or similar)** — detects whether it is daytime (daylight present) or nighttime (dark)

### Irrigation logic (automatic watering rule)
**Water the plant ONLY when BOTH conditions are true:**
- Soil moisture is **below a set threshold** (soil is dry enough to need water)
- **No daylight** is detected by the light sensor (it is dark / nighttime)

**Do NOT water if EITHER condition is false:**
- Moisture is already sufficient → don't water (plant doesn't need it)
- Daylight is detected → don't water (watering during sunlight causes evaporation and potential leaf scorch)

In pseudocode:
```
if moisture < MOISTURE_THRESHOLD and light_level < LIGHT_THRESHOLD:
    turn_pump_on()
else:
    turn_pump_off()
```

### What to add to the Flask backend
1. **A POST endpoint** (e.g., `/sensor-data`) that the ESP32 POSTs to on a regular interval. The payload should include moisture level, light level, temperature, and humidity. Store the latest reading in memory or the database.
2. **A GET endpoint** (e.g., `/sensor-data`) that the frontend polls to fetch the latest sensor values and update the dashboard tiles in real time (replacing the hardcoded placeholders).
3. **Pump command endpoint** — the ESP32 should also poll the server (e.g., `/pump-command`) to know whether it should activate the physical pump relay. The server applies the irrigation logic and returns `ON` or `OFF`. Currently `/toggle-pump` tracks pump state as a server-side boolean; this needs to be extended so the automatic logic can also set pump state.

### What to add to the frontend (index.html)
- Replace the hardcoded sensor tile values with a `setInterval` fetch to `/sensor-data` that updates them live (similar to how the clock is already updated every second).

---

## Database

SQLite via SQLAlchemy. Schema is simple — only one table:

**User**
| Column | Type | Notes |
|--------|------|-------|
| id | Integer | Primary key |
| username | String(150) | Unique |
| password_hash | String(200) | Werkzeug pbkdf2 hash |

No sensor data is persisted to the database yet. If historical irrigation logs are needed, a new model should be added.

---

## Known Issues / TODOs

- `/index` has no `@login_required` guard — any unauthenticated user can reach the dashboard
- `pump_state` in `app.py` is a global in-memory boolean — it resets to `OFF` every time the server restarts
- Sensor data tiles are all hardcoded placeholder values — not yet connected to the ESP32
- No logout route exists yet
- `app.secret_key` in `app.py` is hardcoded — should be moved to an environment variable before any real deployment
