import socket
import struct

sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW)

sock.bind(("swm0", 0))
dst = bytes.fromhex("ffffffffffff")
src = bytes.fromhex("020000000001")

ethertype = struct.pack("!H", 0x88B5)

payload = bytes("Hello", 'utf-8')
frame = dst + src+ ethertype + payload
sock.send(frame)
sock.close()

