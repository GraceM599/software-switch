import argparse
import subprocess
import os
import time
import signal
from mininet.link import Intf


def add_host(config_line, line, host):
    host_if = f"{host.name}-eth0"
    root_if = f"sw{line}"
    
    #veth pair to connect the host and root namespace
    run(f"ip link add {root_if} type veth peer name {host_if}")
    #Move host if into host namespace
    run(f"ip link set {host_if} netns {host.pid}")
    
    #Register intf with Mininet
    Intf(host_if, node=host, port=1, if mode == "trunk" else 0, 
         moveIntfFn=lambda name, node: None)
    
    host.cmd(f"ip link set {host_if} up")
    
    #disable TX checksum offload
    host.cmd(f"ethtool -K {host_if} tx off")
    host.cmd(f"ip addr flush dev {host_if}")
    host.cmd(f"ip link set {root_if} up")
        
    return root_if
    


def parse_configfile(filename):
    config = []
    config_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config")
    with open(os.path.join(config_dir, filename)) as file:
        for line in file:
            config.append(line.split())
    return config
