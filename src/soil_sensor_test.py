from machine import Pin, ADC
import time

# Initialize ADC on GP26 (you can use GP27 or GP28 instead)
soil_sensor = ADC(Pin(26))

# Conversion factor for 16-bit ADC (Pico 2 uses 12-bit, so 65535 = 3.3V)
conversion_factor = 3.3 / 65535

while True:
    # Read raw ADC value
    raw_value = soil_sensor.read_u16()
    
    # Convert to voltage
    voltage = raw_value * conversion_factor
    
    # Convert to percentage (you'll need to calibrate these values)
    # Typical range: dry soil = higher voltage, wet soil = lower voltage
    moisture_percentage = 100 - ((voltage / 3.3) * 100)
    
    print(f"Raw: {raw_value}, Voltage: {voltage:.2f}V, Moisture: {moisture_percentage:.1f}%")
    
    time.sleep(1)