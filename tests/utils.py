import argparse
import subprocess
import os
import time
import signal
from mininet.link import Intf


def kill_switch(switch_process):
    if switch_process is not None:
        switch_process.terminate()
        try:
            switch_process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            switch_process.kill()
            switch_process.wait()


def add_host(line, host):
    host_if = f"{host.name}-eth0"
    root_if = f"sw{line}"
    
    #veth pair to connect the host and root namespace
    subprocess.run(["ip", "link", "add", root_if, "type", "veth", "peer", "name", host_if], check=True)
    #Move host if into host namespace
    subprocess.run(["ip", "link", "set", host_if, "netns", str(host.pid)], check=True)
    
    #disable TX checksum offload
    host.cmd(f"ethtool -K {host_if} tx off")
    host.cmd(f"ip addr flush dev {host_if}")
        
    return host_if, root_if
    


def parse_configfile(filename):
    config = []
    config_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config")
    with open(os.path.join(config_dir, filename)) as file:
        for line in file:
            config.append(line.split())
    return config
