import socket
import struct
import subprocess
import time

def test_monitor():
    sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW)
    sock.bind(("swm0", 0))
    sock.settimeout(1.0)
    
    dst = bytes.fromhex("ffffffffffff")
    src = bytes.fromhex("020000000001")
    ethertype = struct.pack("!H", 0x88B5)
    payload = bytes("Hello", 'utf-8')
    frame = dst + src + ethertype + payload
    
    sock.send(frame)
    
    try:
        data, addr = sock.recvfrom(65535)
        assert len(data) > 0, "Monitor did not receive packet"
    except socket.timeout:
        raise AssertionError("Monitor did not receive packet within timeout")
    finally:
        sock.close()