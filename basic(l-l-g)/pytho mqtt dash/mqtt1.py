import tkinter as tk
import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC = "ravindra/lora/temp"

# Tkinter setup
root = tk.Tk()
root.title("LoRa MQTT Dashboard")
root.geometry("500x250")

label_title = tk.Label(root, text="LoRa Temperature & Humidity", font=("Arial", 18))
label_title.pack(pady=10)

label_value = tk.Label(root, text="Waiting for LoRa data...", font=("Arial", 24), fg="blue")
label_value.pack(pady=20)

# MQTT callbacks
def on_connect(client, userdata, flags, rc):
    print("Connected with result code", rc)
    result, mid = client.subscribe(TOPIC)
    print("Subscribe result:", result)

def on_message(client, userdata, msg):
    try:
        # Decode safely, ignore bad bytes
        data = msg.payload.decode("utf-8", errors="ignore")
    except Exception as e:
        print("Decode error:", e)
        data = str(msg.payload)  # fallback to raw bytes

    print(f"LoRa Data Received: {data}")
    # Update Tkinter label with latest LoRa data
    label_value.config(text=data)

# MQTT client
client = mqtt.Client("TkinterDashboard")
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, 60)
client.loop_start()

# Run Tkinter GUI
root.mainloop()