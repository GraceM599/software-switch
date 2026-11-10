import subprocess
import time
from mininet.net import Mininet
from mininet.link import Intf
from utils import add_host, kill_switch

def test_vlan(config, filename):
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
            
            Intf(host_if, node=host, port=1 if mode == "trunk" else 0, 
                moveIntfFn=lambda name, node: None)
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
            
            subprocess.run(["ip", "link", "set", root_if, "up"], check=True)

        switch_process = subprocess.Popen(
            ["./build/switch", filename] + root_ifs,
            start_new_session=True
        )

        time.sleep(0.5)

        if switch_process.poll() is not None:
            raise RuntimeError("Switch exited early")

        vlan_groups = {}
        for i, (name, mode, vlan, ip) in enumerate(config):
            if vlan not in vlan_groups:
                vlan_groups[vlan] = []
            vlan_groups[vlan].append(hosts[i])
        
        for vlan, vlan_hosts in vlan_groups.items():
            for src in vlan_hosts:
                for dst in vlan_hosts:
                    if src != dst:
                        result = src.cmd("ping -c 1 {}".format(dst.IP()))
                        assert "1 received" in result
        
        for vlan1, hosts1 in vlan_groups.items():
            for vlan2, hosts2 in vlan_groups.items():
                if vlan1 != vlan2:
                    src = hosts1[0]
                    dst = hosts2[0]
                    result = src.cmd("ping -c 1 {}".format(dst.IP()))
                    assert "0 received" in result

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