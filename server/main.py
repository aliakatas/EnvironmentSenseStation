from secrets import *
from mc_sensing import perform_sensor_data_averaging
from write_to_database import write_data_to_postgres
import sys

if __name__ == "__main__":

   comment = '-'
   if len(sys.argv) > 1:
      comment = sys.argv[1]
   
   sample_data = perform_sensor_data_averaging(URL)
   
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
      print("Data written successfully!")
   else:
      print("Failed to write data.")
