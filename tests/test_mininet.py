from mininet.net import Mininet
from mininet.link import Intf
from mininet.cli import CLI
from mininet.log import setLogLevel
import argparse
import subprocess
import os
import time
import signal


def run(cmd):
    print("$ {}".format(cmd))
    subprocess.run(cmd, shell=True, check=True)

def parse_config(filename):
    config = []
    config_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config")
    with open(os.path.join(config_dir, filename)) as file:
        for line in file:
            config.append(line.split())
    return config


def test_mininet(config, filename):
    net = Mininet()

    hosts = [net.addHost(name, ip=ip) for name, mode, vlan, ip in config]

    #for the monitor
    #sudo ip link add swm0 type veth peer name swm1
    #sudo ip link set dev swm0 up
    #sudo ip link set dev swm1 up but we run with sudo so no sudo
    
    run("ip link add swm0 type veth peer name swm1")
    run("ip link set dev swm0 up")
    run("ip link set dev swm1 up")
    
    switch_process = None
    root_ifs = []

    try:
        net.start()

        for i, host in enumerate(hosts):
            name, mode, vlan, ip = config[i]
            host_if = "{}-eth0".format(host.name)
            root_if = "sw{}".format(i)

            # Create the veth pair in the root namespace.
            run(
                "ip link add {} type veth peer name {}".format(
                    root_if, host_if
                )
            )
            root_ifs.append(root_if)

            # Move the host end into the Mininet host namespace.
            run("ip link set {} netns {}".format(host_if, host.pid))

            # Register the existing interface with Mininet.
            Intf(
                host_if,
                node=host,
                port=1 if mode == "trunk" else 0,
                moveIntfFn=lambda name, node: None
            )

            host.cmd("ip link set {} up".format(host_if))
            offload_status = host.cmd(
                "ethtool -K {} tx off; echo $?".format(host_if)
            ).splitlines()
            if not offload_status or offload_status[-1] != "0":
                raise RuntimeError(
                    "Could not disable TX checksum offload on {}".format(
                        host_if
                    )
                )
            host.cmd(
                "ip addr flush dev {}".format(host_if)
            )
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
    config = parse_config(args.config)
    setLogLevel("info")
    config_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config")
    test_mininet(config, os.path.join(config_dir, args.config))