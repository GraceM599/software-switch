from mininet.net import Mininet
from mininet.link import Intf
from mininet.cli import CLI
from mininet.log import setLogLevel
import argparse
import subprocess
import os
import time
import signal
from utils.py import *


def run(cmd):
    print("$ {}".format(cmd))
    subprocess.run(cmd, shell=True, check=True)

def setup_config(hosts, vlan_config):
    #run allt he setup commands needed
    pass


def test_mininet(config, filename):
    net = Mininet()

    hosts = [net.addHost(name, ip=ip) for name, mode, vlan, ip in config]
    
    switch_process = None
    root_ifs = []

    try:
        net.start()

        for i, host in enumerate(hosts):
            name, mode, vlan, ip = config[i]
            
            host_if = add_host(i, host)
            
            ip_if = host_if
            if mode == "trunk":
                ip_if = "{}.{}".format(host_if, vlan)
                out, err, status = host.pexec(
                    "ip", "link", "add", "link", host_if, "name", ip_if,
                    "type", "vlan", "id", vlan
                )
                if status:
                    raise RuntimeError("Could not create {}: {}".format(ip_if, err.strip()))
                Intf(ip_if, node=host, port=0, moveIntfFn=lambda name, node: None)
            host.setIP(ip, intf=ip_if)

            # Bring up the root end so the switch can use it.
            run("ip link set {} up".format(root_if))

        print("Launching userspace switch...")
        switch_process = subprocess.Popen(
            ["./build/switch", filename] + root_ifs,
            start_new_session=True
        )

        time.sleep(0.5)

        if switch_process.poll() is not None:
            raise RuntimeError(
                "Switch exited early with code {}".format(
                    switch_process.returncode
                )
            )

        CLI(net)

    finally:
        if switch_process is not None:
            switch_process.terminate()
            try:
                switch_process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                switch_process.kill()
                switch_process.wait()

        net.stop()

        for root_if in root_ifs:
            subprocess.run(
                ["ip", "link", "del", root_if],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
        run("ip link delete swm0")



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("config", help="config filename in config/")
    args = parser.parse_args()
    config = parse_configfile(args.config)
    setLogLevel("info")
    config_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config")
    test_mininet(config, os.path.join(config_dir, args.config))