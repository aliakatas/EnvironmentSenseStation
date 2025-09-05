def get_soil_moisture(adc):
    # Conversion factor for 16-bit ADC (Pico 2 uses 12-bit, so 65535 = 3.3V)
   conversion_factor = 3.3 / 65535

   # Read raw ADC value
   raw_value = adc.read_u16()
   
   # Convert to voltage
   voltage = raw_value * conversion_factor
   
   # Convert to percentage 
   # Typical range: dry soil = higher voltage, wet soil = lower voltage
   moisture_percentage = 100 - ((voltage / 3.3) * 100)
   
   return moisture_percentage
