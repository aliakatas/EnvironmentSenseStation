import requests
from datetime import datetime, timedelta
import time

def query_environmental_sensors(url):
   try:
      response = requests.get(url)
      response.raise_for_status()

      data = response.json()

      board_temperature = data.get("board_temperature").get("value")
      temperature = data.get("temperature").get("value")
      humidity = data.get("humidity").get("value")
      pressure = data.get("pressure").get("value")
      soil_moisture = data.get("soil_moisture").get("value")

      timestamp = data.get("timestamp").get("value")
      reference_time = data.get("timestamp").get("reference")  # e.g., [2025, 9, 6, 0, 0, 0]

      # Convert reference_time list to datetime object
      ref_dt = datetime(
         year=reference_time[0], month=reference_time[1], day=reference_time[2],
         hour=reference_time[3], minute=reference_time[4], second=reference_time[5],
         tzinfo=None)
      # Add timestamp seconds
      actual_dt = ref_dt + timedelta(seconds=timestamp)
      # Format as "YYYY-MM-DD hh:mm:ss"
      formatted_time = actual_dt.strftime("%Y-%m-%d %H:%M:%S")
      
      return {
         "board_temperature": board_temperature,
         "temperature": temperature,
         "humidity": humidity,
         "pressure": pressure,
         "soil_moisture": soil_moisture,
         "timestamp": formatted_time
      }
   except requests.RequestException as e:
      print(f"Error querying sensors: {e}")


def perform_sensor_data_averaging(url):
   print("Starting sensor data averaging...")

   # prime the sensors... and discard the first take
   sensor_data = query_environmental_sensors(url)
   if not sensor_data:
      return None
   
   # This one we can keep...
   # timestamp = sensor_data["timestamp"]
   # averaged_data = {
   #    "board_temperature": sensor_data["board_temperature"],
   #    "temperature": sensor_data["temperature"],
   #    "humidity": sensor_data["humidity"],
   #    "pressure": sensor_data["pressure"],
   #    "soil_moisture": sensor_data["soil_moisture"]
   # }
   # Instead, use current time as timestamp (microcontroler time may be off)
   timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
   averaged_data = {
      "board_temperature": 0.,
      "temperature": 0.,
      "humidity": 0.,
      "pressure": 0.,
      "soil_moisture": 0.
   }

   # Collect data another 5 times with 10 second intervals
   time.sleep(5) # Initial wait before starting averaging
   div = 0
   for _ in range(5):
      print(f"Collecting more data for averaging... {_+1}/5")
      new_data = query_environmental_sensors(url)
      if not new_data:
         continue
      averaged_data["board_temperature"] += new_data["board_temperature"]
      averaged_data["temperature"] += new_data["temperature"]
      averaged_data["humidity"] += new_data["humidity"]
      averaged_data["pressure"] += new_data["pressure"]
      averaged_data["soil_moisture"] += new_data["soil_moisture"]
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

