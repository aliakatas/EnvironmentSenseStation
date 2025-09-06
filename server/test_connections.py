import requests
import json
from datetime import datetime, timedelta
import time
from typing import List, Tuple, Dict
from secrets import *

# install as psycopg2-binary
# see https://stackoverflow.com/a/73175055 on the difference between psycopg2 and its *-binary counterpart
import psycopg2


def get_and_print_json(url):
   response = requests.get(url)
   response.raise_for_status()  # Raises an error for bad responses
   json_data = response.json()
   print(json.dumps(json_data, indent=4))  # Pretty-print JSON


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


def write_data_to_postgres(
   host: str,
   database: str,
   user: str,
   password: str,
   table_name: str,
   data_records: Dict[str, float] | None,
   comment: str,
   port: int = 5432
) -> bool:
   """
   Connect to PostgreSQL database and write data records.
   
   Args:
      host: Database host address
      database: Database name
      user: Username for database connection
      password: Password for database connection
      port: Database port (default: 5432)
      data_records: List of tuples containing (datetime_string, real_numbers_list, text_string)
   
   Returns:
      bool: True if successful, False otherwise
   """
   
   if data_records is None:
      return True
   
   connection = None
   cursor = None
   
   try:
      # Establish connection
      connection = psycopg2.connect(
         host=host,
         database=database,
         user=user,
         password=password,
         port=port
      )
      
      cursor = connection.cursor()
      
      # Insert data records
      insert_query = f"""
      INSERT INTO {table_name} (date_time, temperature, humidity, pressure, soil_moisture, comment, board_temperature)
      VALUES (%s, %s, %s, %s, %s, %s, %s);
      """
      
      cursor.execute(insert_query, 
                     (data_records["timestamp"], 
                        data_records["temperature"],
                        data_records["humidity"],
                        data_records["pressure"],
                        data_records["soil_moisture"],
                        comment,
                        data_records["board_temperature"]))
      
      # Commit the transaction
      connection.commit()
      
      print(f"Successfully inserted {len(data_records)} records into the database.")
      return True
      
   except psycopg2.Error as e:
      print(f"Database error: {e}")
      if connection:
         connection.rollback()
      return False
   
   except Exception as e:
      print(f"Unexpected error: {e}")
      if connection:
         connection.rollback()
      return False
   
   finally:
      # Clean up connections
      if cursor:
         cursor.close()
      if connection:
         connection.close()


# Example usage:
if __name__ == "__main__":
   # get_and_print_json(URL)
   
   averaged_sensor_data = perform_sensor_data_averaging(URL)

   print()
   if averaged_sensor_data is None:
      print("Failed to collect data!")
   else:
      print(f"""Averaged Sensor Data: 
         Timestamp: {averaged_sensor_data['timestamp']}
         Board Temperature: {averaged_sensor_data['board_temperature']:.2f} °C
         Temperature: {averaged_sensor_data['temperature']:.2f} °C
         Humidity: {averaged_sensor_data['humidity']:.2f} %
         Pressure: {averaged_sensor_data['pressure']:.2f} hPa
         Soil Moisture: {averaged_sensor_data['soil_moisture']:.2f} %
         """)
      
   time.sleep(5)

   # Sample data to insert
   sample_data = perform_sensor_data_averaging(URL)
   
   # Write data to database
   success = write_data_to_postgres(
      host=HOST,
      database=DATABASE,
      user=DBUSER,
      password=DBUSERPASS,
      table_name=TABLENAME,
      data_records=sample_data,
      comment='-',
      port=PORT,
   )
   
   if success:
      print("Data written successfully!")
   else:
      print("Failed to write data.")

