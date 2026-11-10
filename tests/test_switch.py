import pytest
import subprocess
import time
import os
from mininet.net import Mininet
from mininet.link import Intf
from scapy.all import AsyncSniffer, Ether, sendp, sniff, IP
from utils import add_host, kill_switch, parse_configfile

class TestSwitch:
    def test_interfaces_bringup(self):
        config_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config")
        config = parse_configfile("basic_config.txt")
        filename = os.path.join(config_dir, "basic_config.txt")
        
        net = Mininet()
        hosts = [net.addHost(name, ip=ip) for name, mode, vlan, ip in config]
        
        switch_process = None
        root_ifs = []

        try:
            net.start()

            for i, host in enumerate(hosts):
                name, mode, vlan, ip = config[i]
                
                host_if, root_if = add_host(i, host)
                root_ifs.append(root_if)
                
                Intf(host_if, node=host, port=0, moveIntfFn=lambda name, node: None)
                host.setIP(ip, intf=host_if)
                
                subprocess.run(["ip", "link", "set", root_if, "up"], check=True)

            switch_process = subprocess.Popen(
                ["./build/switch", filename] + root_ifs,
                start_new_session=True
            )

            time.sleep(0.5)

            if switch_process.poll() is not None:
                raise RuntimeError("Switch exited early")

            for host in hosts:
                result = host.cmd("ip link show")
                assert "UP" in result
                
                result = host.cmd("ping -c 1 127.0.0.1")
                assert "1 received" in result

        finally:
            kill_switch(switch_process)

            net.stop()

            for root_if in root_ifs:
                subprocess.run(
                    ["ip", "link", "del", root_if],
                    check=False,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                )
        
    def test_MAC_learning(self):
        config_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config")
        config = parse_configfile("basic_config.txt")
        filename = os.path.join(config_dir, "basic_config.txt")
        
        net = Mininet()
        hosts = [net.addHost(name, ip=ip) for name, mode, vlan, ip in config]
        
        switch_process = None
        root_ifs = []

        try:
            net.start()

            for i, host in enumerate(hosts):
                name, mode, vlan, ip = config[i]
                
                host_if, root_if = add_host(i, host)
                root_ifs.append(root_if)
                
                Intf(host_if, node=host, port=0, moveIntfFn=lambda name, node: None)
                host.setIP(ip, intf=host_if)
                
                subprocess.run(["ip", "link", "set", root_if, "up"], check=True)

            switch_process = subprocess.Popen(
                ["./build/switch", filename] + root_ifs,
                start_new_session=True
            )

            time.sleep(0.5)

            if switch_process.poll() is not None:
                raise RuntimeError("Switch exited early")

            hosts[0].cmd("ping -c 1 {}".format(hosts[1].IP()))
            time.sleep(0.2)
            
            sniffer = AsyncSniffer(iface=root_ifs[2], count=0, timeout=1.0)
            sniffer.start()
            time.sleep(0.2)
            test_packet = Ether(src="00:11:22:33:44:55", dst=hosts[1].MAC()) / IP()
            sendp(test_packet, iface=root_ifs[0], verbose=False)
            
            sniffer.join()
            captured_frames = sniffer.results
            assert len(captured_frames) == 0, "Packet should not be flooded to sw2 after MAC learning"

        finally:
            kill_switch(switch_process)

            net.stop()

            for root_if in root_ifs:
                subprocess.run(
                    ["ip", "link", "del", root_if],
                    check=False,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                )
    
    def test_connectivity(self):
        config_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config")
        config = parse_configfile("basic_config.txt")
        filename = os.path.join(config_dir, "basic_config.txt")
        
        net = Mininet()
        hosts = [net.addHost(name, ip=ip) for name, mode, vlan, ip in config]
        
        switch_process = None
        root_ifs = []

        try:
            net.start()

            for i, host in enumerate(hosts):
                name, mode, vlan, ip = config[i]
                
                host_if, root_if = add_host(i, host)
                root_ifs.append(root_if)
                
                Intf(host_if, node=host, port=0, moveIntfFn=lambda name, node: None)
                host.setIP(ip, intf=host_if)
                
                subprocess.run(["ip", "link", "set", root_if, "up"], check=True)

            switch_process = subprocess.Popen(
                ["./build/switch", filename] + root_ifs,
                start_new_session=True
            )

            time.sleep(0.5)

            if switch_process.poll() is not None:
                raise RuntimeError("Switch exited early")

            net.pingAll()

        finally:
            kill_switch(switch_process)

            net.stop()

            for root_if in root_ifs:
                subprocess.run(
                    ["ip", "link", "del", root_if],
                    check=False,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                )
        
    def test_switch_works(self):
        """
        Test Objective: Verify that when a packet with an unlearned destination 
        MAC address hits sw0, the switch replicates it out of sw1, sw2, and sw3.
        """
        config_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "config")
        config = parse_configfile("basic_config.txt")
        filename = os.path.join(config_dir, "basic_config.txt")
        
        net = Mininet()
        hosts = [net.addHost(name, ip=ip) for name, mode, vlan, ip in config]
        
        switch_process = None
        root_ifs = []

        try:
            net.start()

            for i, host in enumerate(hosts):
                name, mode, vlan, ip = config[i]
                
                host_if, root_if = add_host(i, host)
                root_ifs.append(root_if)
                
                Intf(host_if, node=host, port=0, moveIntfFn=lambda name, node: None)
                host.setIP(ip, intf=host_if)
                
                subprocess.run(["ip", "link", "set", root_if, "up"], check=True)

            switch_process = subprocess.Popen(
                ["./build/switch", filename] + root_ifs,
                start_new_session=True
            )

            time.sleep(0.5)

            if switch_process.poll() is not None:
                raise RuntimeError("Switch exited early")

            test_packet = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:aa:bb") / IP()     
            
            sniffer = AsyncSniffer(iface=root_ifs[1], count=1, timeout=2.0)
            sniffer.start()
            time.sleep(0.5)
            sendp(test_packet, iface=root_ifs[0], verbose=False)

            sniffer.join()
            captured_frames = sniffer.results
            assert len(captured_frames) == 1, "The packet was not flooded out of {}".format(root_ifs[1])

        finally:
            kill_switch(switch_process)

            net.stop()

            for root_if in root_ifs:
                subprocess.run(
                    ["ip", "link", "del", root_if],
                    check=False,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                )