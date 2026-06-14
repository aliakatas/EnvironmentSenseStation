import requests
from datetime import datetime, timedelta
import time


def query_environmental_sensors(url, port):
   try:
      resp = requests.get(f"http://{url}:{port}", timeout=2)
      resp.raise_for_status()
      data = resp.json()

      board_temperature = data.get("board_temperature").get("value")
      temperature = data.get("temperature").get("value")
      humidity = data.get("humidity").get("value")
      pressure = data.get("pressure").get("value")

      # Format as "YYYY-MM-DD hh:mm:ss"
      formatted_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

      return {
         "board_temperature": board_temperature,
         "temperature": temperature,
         "humidity": humidity,
         "pressure": pressure,
         "timestamp": formatted_time
      }
   except Exception as e:
      print(f"Error querying sensors: {e}")
      return {}


def perform_sensor_data_averaging(url, port):
   print("Starting sensor data averaging...")

   # prime the sensors... and discard the first take
   sensor_data = query_environmental_sensors(url, port)
   if not sensor_data:
      return None
   
   timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
   averaged_data = {
      "board_temperature": 0.,
      "temperature": 0.,
      "humidity": 0.,
      "pressure": 0.,
   }

   # Collect data another 5 times with 10 second intervals
   time.sleep(5) # Initial wait before starting averaging
   div = 0
   for _ in range(5):
      print(f"Collecting more data for averaging... {_+1}/5")
      new_data = query_environmental_sensors(url, port)
      if not new_data:
         continue
      averaged_data["board_temperature"] += new_data["board_temperature"]
      averaged_data["temperature"] += new_data["temperature"]
      averaged_data["humidity"] += new_data["humidity"]
      averaged_data["pressure"] += new_data["pressure"]
      div += 1
      time.sleep(10)

   # Compute averages
   if div == 0:
      print("No valid data collected for averaging.")
      return None
   
   print("Computing averages...")
   for key in averaged_data:
      averaged_data[key] /= div

   averaged_data["timestamp"] = timestamp
   return averaged_data

