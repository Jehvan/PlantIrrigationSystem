from app import app
from db_model import db

with app.app_context():  # IMPORTANT: app context is required
    db.create_all()
    print("Database created successfully!")