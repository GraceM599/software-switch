from mininet.net import Mininet
from mininet.link import Intf
from mininet.cli import CLI
from mininet.log import setLogLevel
import subprocess
import os
import time
import signal


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

    switch_process = None
    root_ifs = []

    try:
        net.start()

        for i, host in enumerate(hosts):
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
                moveIntfFn=lambda name, node: None
            )

            host.cmd("ip link set {} up".format(host_if))
            host.cmd(
                "ip addr flush dev {}".format(host_if)
            )
            host.setIP(
                "10.0.0.{}/24".format(i + 1),
                intf=host_if
            )

            # Bring up the root end so the switch can use it.
            run("ip link set {} up".format(root_if))

        print("Launching userspace switch...")
        switch_process = subprocess.Popen(
            ["./build/switch"] + root_ifs,
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


if __name__ == "__main__":
    setLogLevel("info")
    test_mininet()