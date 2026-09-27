#!/usr/bin/env python3

from mininet.net import Mininet
from mininet.link import Intf
from mininet.cli import CLI
from mininet.log import setLogLevel
import subprocess
import os
import time


def run(cmd):
    print("$ {}".format(cmd))
    subprocess.run(cmd, shell=True, check=True)


def test_mininet():
    net = Mininet()

    hosts = [
        net.addHost("h1", ip="10.0.0.1/24"),
        net.addHost("h2", ip="10.0.0.2/24"),
        net.addHost("h3", ip="10.0.0.3/24"),
        net.addHost("h4", ip="10.0.0.4/24"),
    ]

    net.start()

    try:
        root_ifs = []

        for i, host in enumerate(hosts):
            host_if = "{}-eth0".format(host.name)
            root_if = "sw{}".format(i)

            run("ip link add {} type veth peer name {}".format(
                    root_if, host_if
                ))
            run("ip link set {} netns {}".format(
                    host_if, host.pid
                ))

            Intf(
                host_if,
                node=host,
                moveIntfFn=lambda name, node: None
            )

            host.setIP(
                "10.0.0.{}/24".format(i + 1),
                intf=host_if
            )

            root_ifs.append(root_if)
    finally:
        pass

        print("launching switch")
        switch_process = subprocess.Popen(["./build/switch"], preexec_fn=os.setpgrp)
        switch_pgid = os.getpgid(switch_process.pid)

        time.sleep(0.5)

        CLI(net)


        net.stop()


if __name__ == "__main__":
    setLogLevel("info")
    test_mininet()