import socket
from secrets import URL, URLPORT

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(2)

sock.sendto(b"SENSORS", (URL, URLPORT))
rdata, _ = sock.recvfrom(256)
sock.close()

print(rdata.decode())

