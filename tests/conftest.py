import pytest
import subprocess
import time
import os
import signal

@pytest.fixture(scope="session", autouse=True)
def network_enviroment():
    print("creating tap interfaces")
    interfaces = ["tap0", "tap1", "tap2", "tap3"]

    for iface in interfaces:
        subprocess.run(["sudo", "ip", "tuntap", "del", "mode", "tap", iface], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    
    for iface in interfaces:
        subprocess.run(["sudo", "ip", "tuntap", "add", "mode", "tap", iface], check=True)
        subprocess.run(["sudo", "ip", "link", "set", iface, "up"], check=True)
    
    print("launching switch")
    switch_process = subprocess.Popen(["sudo", "./build/switch"], preexec_fn=os.setpgrp)

    time.sleep(0.5)

    yield

    print("Shutting down switch")
    try:
        os.killpg(os.getpgid(switch_process.pid), signal.SIGTERM)
        switch_process.wait(timeout=2)
    except Exception:
        os.killpg(os.getpgid(switch_process.pid), signal.SIGKILL)

    for iface in interfaces:
        subprocess.run(["sudo", "ip", "tuntap", "del", "mode", "tap", iface], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
