import network
import time
from machine import Pin, ADC
from microdot import Microdot, Response
import secrets

# --- Pin configuration (adjust to your wiring) ---
moisture_adc = ADC(Pin(34), atten=ADC.ATTN_11DB)
light_adc    = ADC(Pin(35), atten=ADC.ATTN_11DB)
pump_pin     = Pin(26, Pin.OUT)

# --- Thresholds (tune after testing with your sensors) ---
# Moisture: raw ADC value — below this means the soil is dry enough to water
MOISTURE_THRESHOLD = 2000
# Light: raw ADC value — below this means it is dark (no daylight)
LIGHT_THRESHOLD = 500

# --- State ---
manual_pump = None  # None = auto mode, True/False = manual override

app = Microdot()


def read_sensors():
    return {
        'moisture': moisture_adc.read(),
        'light':    light_adc.read(),
    }


def should_water(sensors):
    return (
        sensors['moisture'] < MOISTURE_THRESHOLD and
        sensors['light']    < LIGHT_THRESHOLD
    )


def apply_irrigation():
    """Called periodically. Skipped if a manual override is active."""
    global manual_pump
    if manual_pump is not None:
        return
    sensors = read_sensors()
    pump_pin.value(1 if should_water(sensors) else 0)


@app.route('/')
def index(request):
    with open('index.html', 'r') as f:
        return Response(f.read(), content_type='text/html')


@app.route('/sensor-data')
def sensor_data(request):
    sensors = read_sensors()
    pump_on = pump_pin.value() == 1
    return {
        'moisture': sensors['moisture'],
        'light':    sensors['light'],
        'pump':     'ON' if pump_on else 'OFF',
    }


@app.route('/toggle-pump', methods=['POST'])
def toggle_pump(request):
    global manual_pump
    current = pump_pin.value()
    new_state = 0 if current else 1
    pump_pin.value(new_state)
    manual_pump = bool(new_state)  # lock into manual mode
    return {'status': 'ON' if new_state else 'OFF'}


@app.route('/auto-mode', methods=['POST'])
def auto_mode(request):
    """Return to automatic irrigation logic, clearing any manual override."""
    global manual_pump
    manual_pump = None
    apply_irrigation()
    return {'mode': 'auto'}


def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(secrets.SSID, secrets.PASSWORD)
    print('Connecting to WiFi', end='')
    while not wlan.isconnected():
        print('.', end='')
        time.sleep(0.5)
    print('\nConnected:', wlan.ifconfig()[0])


connect_wifi()

# Run irrigation check every 60 seconds in the background
import asyncio

async def irrigation_loop():
    while True:
        apply_irrigation()
        await asyncio.sleep(60)

async def main():
    asyncio.create_task(irrigation_loop())
    await app.start_server(port=80, debug=True)

asyncio.run(main())
