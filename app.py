from flask import Flask, render_template, jsonify
from datetime import datetime

app = Flask(__name__)

pump_state = False

@app.route('/')
def index():
    return render_template('index.html', current_time=datetime.now().strftime("%Y-%m-%d %H:%M:%S"))

@app.route('/toggle-pump', methods=['POST'])
def toggle_pump():
    global pump_state
    pump_state = not pump_state
    return jsonify({'status': 'ON' if pump_state else 'OFF'})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
