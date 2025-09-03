from machine import Pin
import time

pico_led = Pin("LED", Pin.OUT)

while True:
    pico_led.on()
    time.sleep(0.5)
    pico_led.off()
    time.sleep(0.5)
    