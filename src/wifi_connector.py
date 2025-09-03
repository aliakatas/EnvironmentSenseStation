import network
import rp2
from machine import Pin
import socket
import time
import sys

ssid = 'YOUR_SSID'
password = 'YOUR_WIFI_PASSWORD'
MAX_NUM_ATTEMPTS = 50           # Max number of attempts to connect before giving up
pico_led = Pin("LED", Pin.OUT)  # Onboard LED

def translate_status(stat):
    if stat == network.STAT_IDLE:
        return "no connection and no activity"
    elif stat == network.STAT_CONNECTING:
        return "connecting in progress"
    elif stat == network.STAT_WRONG_PASSWORD:
        return "failed due to incorrect password"
    elif stat == network.STAT_NO_AP_FOUND:
        return "failed because no access point replied"
    elif stat == network.STAT_CONNECT_FAIL:
        return "failed due to other problems"
    elif stat == network.STAT_GOT_IP:
        return "connection successful"
    
    return f"unknown error {stat}"

class WiFiConnector:
    
    def __init__(self):
        self.con_attempts = 0
        self.MAX_CON_ATTEMPTS = MAX_NUM_ATTEMPTS
        self.connected = False
        
        wlan = network.WLAN(network.STA_IF)
        wlan.active(True)
        wlan.connect(ssid, password)
            
        # Wait for connect or fail
        while self.con_attempts < self.MAX_CON_ATTEMPTS:
            if rp2.bootsel_button() == 1:
                sys.exit()
                
            if wlan.status() < 0 or wlan.status() >= 3:
                print(translate_status(wlan.status()))
                break
            self.con_attempts += 1
            print('waiting for connection...')
            pico_led.on()
            time.sleep(0.5)
            pico_led.off()
            time.sleep(0.5)
            
        # Handle connection error
        if wlan.status() != 3:
            raise RuntimeError('network connection failed')

        print('connected')
        pico_led.on()
        self.connected = True
    
    def open_socket(self):
        if not self.connected:
            raise RuntimeError('System has not been connected to WiFi')
        
        # Open a socket
        addr = socket.getaddrinfo('0.0.0.0', 80)[0][-1]
        s = socket.socket()
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(addr)
        s.listen(1)

        print('Server listening on', addr)
        return s
        
    
        
