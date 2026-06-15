from machine import Pin
import time

SAFE = Pin(15, Pin.IN, Pin.PULL_UP)

if not SAFE.value():
    print("SAFE MODE: main.py skipped")
    while True:
        time.sleep(1)
