import subprocess
import time
import os
from mininet.net import Mininet
from mininet.link import Intf
from scapy.all import Ether, IP, AsyncSniffer, sendp
from utils import add_host, kill_switch, parse_configfile


def test_monitor():
    """
    Test that monitor interfaces receive copies of traffic flowing through the switch.
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
        
        sniffer = AsyncSniffer(iface="swm0", count=0, timeout=2.0)
        sniffer.start()
        time.sleep(0.3)
        
        #Send packet from host0 to host1 through switch
        test_packet = Ether(src=hosts[0].MAC(), dst=hosts[1].MAC()) / IP(src=hosts[0].IP(), dst=hosts[1].IP())
        sendp(test_packet, iface=root_ifs[0], verbose=False)
        
        sniffer.join()
        captured_frames = sniffer.results
        
        assert len(captured_frames) > 0, "Monitor interface did not capture any packets"
        
    finally:
        kill_switch(switch_process)
        net.stop()