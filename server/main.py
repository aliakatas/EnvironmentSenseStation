from secrets import *
from mc_sensing import perform_sensor_data_averaging
from write_to_database import write_data_to_postgres
import sys
import datetime

if __name__ == "__main__":

   comment = '-'
   if len(sys.argv) > 1:
      comment = sys.argv[1]
   
   now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
   try:
      sample_data = perform_sensor_data_averaging(URL)

      if sample_data is None:
         raise RuntimeError("No valid data collected for averaging. Skipping database write.")
      
      # Write data to database
      success = write_data_to_postgres(
         host=HOST,
         database=DATABASE,
         user=DBUSER,
         password=DBUSERPASS,
         table_name=TABLENAME,
         data_records=sample_data,
         comment=comment,
         port=PORT,
      )
      if success:
         print(f"Data written successfully at {now}")
      else:
         print(f"Failed to write data at {now}")

   except Exception as e:
      print(f"[{now}] An error occurred: {e}")

   print("*************************")

