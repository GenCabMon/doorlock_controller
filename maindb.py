import paho.mqtt.client as mqtt
import time
import sqlite3
from datetime import datetime

# Configuración
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
DEVICE_IDS = ["ESP32_1", "ESP32_2"]
TOPICS_TO_SUBSCRIBE = ["door/status/+", "door/intruder/+"]
DB_NAME = "esp32_data.db"

# Inicialización de BD mejorada
def init_db():
    try:
        conn = sqlite3.connect(DB_NAME)
        cursor = conn.cursor()
        
        # Eliminar tabla existente si hay problemas
        cursor.execute("DROP TABLE IF EXISTS door_status")
        
        # Crear nueva tabla con constraint UNIQUE
        cursor.execute('''
            CREATE TABLE door_status (
                device_id TEXT PRIMARY KEY,
                vibsens INTEGER DEFAULT 0,
                magsens INTEGER DEFAULT 0,
                lockstate TEXT DEFAULT "",
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        
        # Insertar dispositivos iniciales
        for dev in DEVICE_IDS:
            cursor.execute("""
                INSERT OR IGNORE INTO door_status (device_id) 
                VALUES (?)
            """, (dev,))
        
        conn.commit()
        print("✅ BD inicializada correctamente")
    except Exception as e:
        print(f"❌ Error en init_db: {e}")
    finally:
        conn.close()

# Actualización mejorada
def update_db(device_id, field, value):
    try:
        print(f"device {device_id} field {field} value {value}")
        conn = sqlite3.connect(DB_NAME)
        cursor = conn.cursor()
        
        cursor.execute(f"""
            UPDATE door_status 
            SET {field} = ?, timestamp = datetime('now')
            WHERE device_id = ?
        """, (value, device_id))
        
        conn.commit()
        print(f"🔄 {device_id} | {field} actualizado a {value}")
    except Exception as e:
        print(f"❌ Error en update_db: {e}")
    finally:
        conn.close()

# Callback MQTT mejorado
def on_message(client, userdata, msg):
    try:
        topic_parts = msg.topic.split('/')
        if len(topic_parts) != 3:
            return
            
        device_id = topic_parts[2]
        payload = msg.payload.decode()

        if device_id not in DEVICE_IDS:
            return

        if "status" in msg.topic:
            status_map = {
                "Puerta abierta": 2,
                "Puerta mal cerrada": 1,
                "Puerta cerrada": 0
            }
            magsens = status_map.get(payload, 0)
            update_db(device_id, "magsens", magsens)

        elif "intruder" in msg.topic:
            vibsens = 1 if payload == "Intruso detectado" else 0
            update_db(device_id, "vibsens", vibsens)
            
    except Exception as e:
        print(f"❌ Error en on_message: {e}")

# Mostrar estado mejorado
def show_status():
    try:
        conn = sqlite3.connect(DB_NAME)
        cursor = conn.cursor()
        cursor.execute("""
            SELECT device_id, vibsens, magsens, lockstate, timestamp 
            FROM door_status 
            ORDER BY device_id
        """)
        
        print("\n" + "-"*65)
        print("| ID      | VibSens | MagnSens | LockState | Última Actualización |")
        print("-"*65)
        
        for row in cursor.fetchall():
            print(f"| {row[0]:<7} | {row[1]:^7} | {row[2]:^8} | {row[3]:^9} | {row[4]:<19} |")
        
        print("-"*65)
    except Exception as e:
        print(f"❌ Error en show_status: {e}")
    finally:
        conn.close()

# Configuración inicial
init_db()  # Esto borrará y recreará la tabla limpia

client = mqtt.Client("RPi4_Monitor")
client.on_connect = lambda client, userdata, flags, rc: (
    [client.subscribe(topic) for topic in TOPICS_TO_SUBSCRIBE] if rc == 0 else None
)
client.on_message = on_message
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_start()

# Menú interactivo
try:
    while True:
        print("\n--- Menú de Control ---")
        print("1. Ver estado actual")
        print("2. Cambiar LockState de ESP32_1")
        print("3. Cambiar LockState de ESP32_2")
        print("4. Salir")
        
        opcion = input("Opción: ")
        
        if opcion == "1":
            show_status()
        elif opcion in ["2", "3"]:
            device_num = int(opcion) - 1 # Convertir a numero entero y restar
            device = f"ESP32_{device_num}"
            try:
                estado = input(f"Nuevo estado (0/1) para {device}: ")
                # Actualizar base de datos
                update_db(device, "lockstate", estado)
                
                # Publicar comando MQTT
                topic_control = f"door/control/{device}"
                client.publish(topic_control, estado)
                print(f"📤 Publicado en {topic_control}: {estado}")
            except ValueError:
                print("⚠️ Entrada inválida")
        elif opcion == "4":
            break
            
        time.sleep(1)
        
except KeyboardInterrupt:
    print("\n🔌 Cerrando aplicación...")
finally:
    client.disconnect()