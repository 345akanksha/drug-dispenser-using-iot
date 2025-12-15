from flask import Flask, request, jsonify, session
from flask_cors import CORS
from flask_mail import Mail, Message
import mysql.connector
import os
from dotenv import load_dotenv
from datetime import datetime

load_dotenv()

app = Flask(__name__)
app.secret_key = os.getenv('SECRET_KEY', 'ashuannu')
CORS(app, supports_credentials=True, origins=["http://localhost:8080"])

# ---------- Mail Configuration ----------
app.config.update(
    MAIL_SERVER='smtp.gmail.com',
    MAIL_PORT=587,
    MAIL_USE_TLS=True,
    MAIL_USERNAME=os.getenv('EMAIL_USER'),
    MAIL_PASSWORD=os.getenv('EMAIL_PASS')
)
mail = Mail(app)


# ---------- Database Connection ----------
def get_db():
    return mysql.connector.connect(
        host='localhost', user='root', password='ashuannu', database='medicinedb'
    )


# ---------- Authentication ----------
@app.route('/signup', methods=['POST'])
def signup():
    data = request.json or {}
    name = data.get('name')
    email = data.get('email')
    password = data.get('password')
    if not (name and email and password):
        return jsonify({"error": "missing fields"}), 400

    db = get_db()
    cur = db.cursor()
    cur.execute("SELECT id FROM caretakers WHERE email=%s", (email,))
    if cur.fetchone():
        cur.close(); db.close()
        return jsonify({"success": False, "error": "Email already registered"}), 400

    cur.execute("INSERT INTO caretakers (name, email, password, dispenser_password) VALUES (%s, %s, %s, %s)",
                (name, email, password, "1234"))
    db.commit(); cur.close(); db.close()
    return jsonify({"success": True})


@app.route('/login', methods=['POST'])
def login():
    data = request.json or {}
    email = data.get('email')
    password = data.get('password')
    if not (email and password):
        return jsonify({"error": "missing fields"}), 400

    db = get_db()
    cur = db.cursor(dictionary=True)
    cur.execute("SELECT id, name, email FROM caretakers WHERE email=%s AND password=%s",
                (email, password))
    user = cur.fetchone()
    cur.close(); db.close()
    if user:
        session['caretaker_id'] = user['id']
        session['caretaker_name'] = user['name']
        session['caretaker_email'] = user['email']
        return jsonify({"success": True, "name": user['name'], "email": user['email']})
    return jsonify({"success": False}), 401


@app.route('/logout')
def logout():
    session.clear()
    return jsonify({"success": True})


@app.route('/whoami')
def whoami():
    if 'caretaker_id' in session:
        return jsonify({"logged_in": True, "name": session.get('caretaker_name'), "email": session.get('caretaker_email')})
    return jsonify({"logged_in": False})


# ---------- Schedule CRUD ----------
@app.route('/schedule', methods=['GET'])
def get_schedule():
    db = get_db()
    cur = db.cursor(dictionary=True)
    cur.execute("SELECT id, medicine_name, TIME_FORMAT(time, '%H:%i') AS time, taken FROM schedule")
    rows = cur.fetchall()
    cur.close(); db.close()
    return jsonify(rows)


@app.route('/schedule', methods=['POST'])
def add_schedule():
    if 'caretaker_id' not in session:
        return jsonify({"error": "Auth required"}), 401
    data = request.json or {}
    name = data.get('medicine_name')
    time_val = data.get('time')
    if not (name and time_val):
        return jsonify({"error": "missing fields"}), 400
    db = get_db()
    cur = db.cursor()
    cur.execute("INSERT INTO schedule (medicine_name, time, taken, notified, caretaker_id) VALUES (%s, %s, %s, %s, %s)",
                (name, time_val, False, False, session['caretaker_id']))
    db.commit(); cur.close(); db.close()
    return jsonify({"status": "ok"})


@app.route('/schedule/<int:med_id>', methods=['DELETE'])
def delete_schedule(med_id):
    if 'caretaker_id' not in session:
        return jsonify({"error": "Auth required"}), 401
    db = get_db()
    cur = db.cursor()
    cur.execute("DELETE FROM schedule WHERE id=%s AND caretaker_id=%s", (med_id, session['caretaker_id']))
    db.commit()
    cur.close(); db.close()
    return jsonify({"status": "deleted"})


@app.route('/schedule/<int:med_id>', methods=['PUT'])
def update_schedule(med_id):
    if 'caretaker_id' not in session:
        return jsonify({"error": "Auth required"}), 401
    data = request.json or {}
    name = data.get('medicine_name')
    time_val = data.get('time')
    db = get_db()
    cur = db.cursor()
    update_fields = []
    values = []
    if name:
        update_fields.append("medicine_name=%s")
        values.append(name)
    if time_val:
        update_fields.append("time=%s")
        values.append(time_val)
    if not update_fields:
        cur.close(); db.close()
        return jsonify({"error": "Nothing to update"}), 400
    values.append(med_id)
    values.append(session['caretaker_id'])
    cur.execute(f"UPDATE schedule SET {', '.join(update_fields)} WHERE id=%s AND caretaker_id=%s", tuple(values))
    db.commit()
    cur.close(); db.close()
    return jsonify({"status": "updated"})


@app.route('/notify_taken', methods=['POST'])
def notify_taken():
    data = request.json or {}
    sid = data.get('id')
    status = data.get('status')
    if not sid or status not in ('taken', 'missed'):
        return jsonify({"error": "invalid payload"}), 400

    db = get_db()
    cur = db.cursor()
    if status == "taken":
        cur.execute("UPDATE schedule SET taken=TRUE WHERE id=%s", (sid,))
    else:
        cur.execute("UPDATE schedule SET notified=TRUE WHERE id=%s", (sid,))
    db.commit()
    cur.close(); db.close()
    return jsonify({"status": "ok"})


@app.route('/overdue', methods=['GET'])
def overdue_alerts():
    if 'caretaker_id' not in session:
        return jsonify([]), 401
    db = get_db()
    cur = db.cursor(dictionary=True)
    cur.execute("SELECT id, medicine_name, TIME_FORMAT(time, '%H:%i') AS time FROM schedule WHERE taken=FALSE AND caretaker_id=%s",
                (session['caretaker_id'],))
    overdue = cur.fetchall()
    cur.close(); db.close()
    return jsonify(overdue)


# ---------- Dispenser Password ----------
@app.route('/password', methods=['GET', 'POST'])
def dispenser_password():
    db = get_db()
    cur = db.cursor()
    if request.method == 'GET':
        cur.execute("SELECT dispenser_password FROM caretakers LIMIT 1")  # first caretaker
        pw = cur.fetchone()[0]
        cur.close(); db.close()
        return jsonify({"password": pw})
    else:
        # Optional: still require caretaker session for update
        if 'caretaker_id' not in session:
            return jsonify({"error": "Auth required"}), 401
        data = request.json or {}
        new_pw = data.get('password')
        if not new_pw:
            return jsonify({"error": "Missing password"}), 400
        cur.execute("UPDATE caretakers SET dispenser_password=%s WHERE id=%s",
                    (new_pw, session['caretaker_id']))
        db.commit()
        cur.close(); db.close()
        return jsonify({"status": "updated"})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
