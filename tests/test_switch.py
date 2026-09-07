import pytest
from scapy.all import AsyncSniffer, Ether, sendp, sniff, IP

class TestSwitch:
    def test_switch_works(self):
        """
        Test Objective: Verify that when a packet with an unlearned destination 
        MAC address hits tap0, the switch replicates it out of tap1, tap2, and tap3.
        """
        test_packet = Ether(src="00:11:22:33:44:55", dst="66:77:88:99:aa:bb") / IP()     
        print("\nInjecting test frame into tap0...")
        
        sniffer = AsyncSniffer(iface="tap1", count=1, timeout=1.0, filter="ether src 00:11:22:33:44:55")
        sniffer.start()
        time.sleep(1)
        sendp(test_packet, iface="tap0", verbose=False)

        sniffer.join() #block until 1 pkt recieved
        captured_frames = sniffer.results
        #import pdb; pdb.set_trace()
        assert len(captured_frames) == 1, "Failed: The packet was not flooded out of tap1!"
        
        rx_packet = captured_frames[0]
        assert rx_packet.src == "00:11:22:33:44:55", "Failed: Source MAC was altered!"
        assert rx_packet.dst == "66:77:88:99:aa:bb", "Failed: Destination MAC was altered!"
        print("Success: Unknown Unicast Flooding test passed!")