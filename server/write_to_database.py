from typing import Dict
# install as psycopg2-binary
# see https://stackoverflow.com/a/73175055 on the difference between psycopg2 and its *-binary counterpart
import psycopg2


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
      table_name: The name of the table to write data to
      data_records: Dictionary of the record to relate columns with the data
      comment: Additional information associated with the record
      port: Database port (default: 5432)
      
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
      INSERT INTO {table_name} (date_time, temperature, humidity, pressure, comment, board_temperature)
      VALUES (%s, %s, %s, %s, %s, %s, %s);
      """
      
      cursor.execute(insert_query, 
                     (data_records["timestamp"], 
                        data_records["temperature"],
                        data_records["humidity"],
                        data_records["pressure"],
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
