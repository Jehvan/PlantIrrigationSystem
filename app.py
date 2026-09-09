from flask import Flask, request, render_template, flash, redirect, url_for, jsonify
from db_model import db, User
from datetime import datetime
app = Flask(__name__)
app.secret_key = 'HSAD3294HSHDAPAF12DSAWO'

# Configure DB
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///database.db'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db.init_app(app)  # IMPORTANT: bind db to this app
@app.route('/register', methods=['GET', 'POST'])
def register():
    from flask import render_template, request, redirect, url_for, flash
    from db_model import db, User

    if request.method == 'POST':
        username = request.form['username']
        password = request.form['password']

        # Check if user exists
        if User.query.filter_by(username=username).first():
            flash("Username already exists")
            return redirect(url_for('register'))

        # Create new user
        new_user = User(username=username)
        new_user.set_password(password)
        db.session.add(new_user)
        db.session.commit()

        flash("Registration successful! Please login.")
        return redirect(url_for('login'))

    return render_template('Register.html')

from flask import session

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        name = request.form['username']
        password = request.form['password']

        user = User.query.filter_by(username=name).first()
        if user and user.check_password(password):
            # store username in session
            session['username'] = name
            return redirect(url_for('index'))
        else:
            flash("Invalid username or password")
            return redirect(url_for('login'))
    return render_template('Login.html')

@app.route('/')
@app.route('/index')
def index():
    username = session.get('username', 'Guest')
    return render_template('index.html', username=username, current_time=datetime.now().strftime("%Y-%m-%d %H:%M:%S"))



pump_state = False  # global variable to track pump

@app.route('/toggle-pump', methods=['POST'])
def toggle_pump():
    global pump_state
    pump_state = not pump_state
    print(f"Pump state changed: {'ON' if pump_state else 'OFF'}")
    return jsonify({'status': 'ON' if pump_state else 'OFF'})