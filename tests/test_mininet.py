#!/usr/bin/env python3

from mininet.net import Mininet
from mininet.cli import CLI
from mininet.log import setLogLevel
import subprocess

#todo, topology setup works alr but the mininet api doesn't see the interfaces so
#like h1 ping h2 crashes. :)
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

            run(
                "ip link add {} type veth peer name {}".format(
                    root_if, host_if
                )
            )

            run(
                "ip link set {} netns {}".format(
                    host_if, host.pid
                )
            )

            host.cmd("ip link set {} up".format(host_if))
            root_ifs.append(root_if)

        for i, root_if in enumerate(root_ifs):
            bridge = "br{}".format(i)
            tap = "tap{}".format(i)

            run("ip link add name {} type bridge".format(bridge))
            run("ip link set {} up".format(bridge))

            run("ip link set {} master {}".format(root_if, bridge))
            run("ip link set {} master {}".format(tap, bridge))

        CLI(net)

    finally:
        for i in range(4):
            subprocess.run(
                "ip link del br{}".format(i),
                shell=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )

        net.stop()


if __name__ == "__main__":
    setLogLevel("info")
    test_mininet()