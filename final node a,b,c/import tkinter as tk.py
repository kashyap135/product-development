import tkinter as tk
from tkinter import ttk
import paho.mqtt.client as mqtt
import re

# --- Configuration (Must match Node C) ---
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883
MQTT_TOPIC = "ravindra/lora/temp"

class SensorDashboard:
    def __init__(self, root):
        self.root = root
        self.root.title("IoT Air Quality Monitor - Node C Gateway")
        self.root.geometry("500x550")
        self.root.configure(bg="#2c3e50")

        # Dictionary to store label objects for updating
        self.labels = {}
        
        # Define the sensors we expect to see in the string
        self.sensor_keys = [
            ("o2", "Oxygen (%)"), ("co2", "CO2 (ppm)"), 
            ("s_t", "SCD Temp (C)"), ("s_h", "SCD Hum (%)"),
            ("b_t", "BME Temp (C)"), ("b_h", "BME Hum (%)"),
            ("b_p", "Pressure (hPa)"), ("b_g", "Gas (KOhms)"),
            ("h2s", "H2S (V)"), ("co", "CO (V)"), ("ch4", "Methane (V)")
        ]

        self.setup_ui()
        self.setup_mqtt()

    def setup_ui(self):
        # Header
        header = tk.Label(self.root, text="LIVE SENSOR DATA", font=("Arial", 18, "bold"), 
                         bg="#2c3e50", fg="#ecf0f1", pady=20)
        header.pack()

        # Container for the grid
        main_frame = tk.Frame(self.root, bg="#2c3e50")
        main_frame.pack(padx=20, pady=10, fill="both", expand=True)

        # Create rows for each sensor
        for i, (key, display_name) in enumerate(self.sensor_keys):
            # Name Label
            tk.Label(main_frame, text=display_name, font=("Arial", 12), 
                     bg="#2c3e50", fg="#bdc3c7").grid(row=i, column=0, sticky="w", pady=5)
            
            # Value Label (The one that changes)
            val_lbl = tk.Label(main_frame, text="--", font=("Arial", 12, "bold"), 
                               bg="#2c3e50", fg="#2ecc71", width=15, anchor="e")
            val_lbl.grid(row=i, column=1, sticky="e", pady=5)
            
            self.labels[key] = val_lbl

        # Footer Status
        self.status_var = tk.StringVar(value="Status: Connecting to MQTT...")
        status_bar = tk.Label(self.root, textvariable=self.status_var, bd=1, relief=tk.SUNKEN, 
                              anchor="w", bg="#34495e", fg="#ecf0f1")
        status_bar.pack(side=tk.BOTTOM, fill=tk.X)

    def setup_mqtt(self):
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        
        try:
            self.client.connect(MQTT_BROKER, MQTT_PORT, 60)
            self.client.loop_start() # Run MQTT in a background thread
        except Exception as e:
            self.status_var.set(f"Connection Error: {e}")

    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.status_var.set(f"Status: Connected to {MQTT_BROKER}")
            client.subscribe(MQTT_TOPIC)
        else:
            self.status_var.set(f"Status: Connection Failed (Code {rc})")

    def on_message(self, client, userdata, msg):
        payload = msg.payload.decode()
        print(f"Received: {payload}")
        
        # Parse the string (e.g., "o2:25.43 co2:1010 ...")
        # Uses regex to find 'key:value' pairs
        pairs = re.findall(r"(\w+):([\d\.]+)", payload)
        
        for key, value in pairs:
            if key in self.labels:
                self.labels[key].config(text=value)
        
        self.status_var.set(f"Last update: {msg.topic} at {msg.timestamp if hasattr(msg, 'timestamp') else 'now'}")

if __name__ == "__main__":
    root = tk.Tk()
    app = SensorDashboard(root)
    root.mainloop()