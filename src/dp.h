#pragma once
#include <iostream>
#include <cstring>
#include <cerrno>
#include "packet_struct.h"
#include "switch_port.h"
#include "macTable.h" 
#include "switch_monitor.h"
#include <poll.h>
#include <vector>
#include <memory>
#include <functional>
#include <csignal>
#include <thread>
#include <fstream>
#include <sstream>
#include <stdexcept>



inline void pollPort(std::unique_ptr<Port>& port, int port_idx, uint8_t*& buffer, size_t buffer_size, uint8_t& ingress, size_t& pkt_len, uint16_t& vlan_id){
    //returns a filled buffer and ingress index if a pkt exists on the port
    if (port->has_pending_packet()){
        ingress = port_idx;
        pkt_len = port->get(buffer, buffer_size, vlan_id);
        return;
    }

}
inline void processPackets(std::vector<std::unique_ptr<Port>>& portList, pollfd& port_fd, int port_idx, 
    MacTable& mac_table, std::atomic<bool>* continue_running, Monitor& monitor){
    uint8_t* buffer;
    uint8_t ingress_port;
    size_t next_port = 0;
    size_t pkt_len = 0; 
    uint8_t packet_pool[65536];
    uint16_t vlan_id = 0;

    while (*continue_running) { //fc
        buffer = packet_pool;
        pkt_len = 0;

        pollPort(portList[port_idx], port_idx, buffer, sizeof(packet_pool), ingress_port, pkt_len, vlan_id);
        
        if (pkt_len <= 0) continue; 
        if (pkt_len < static_cast<ssize_t>(sizeof(EthernetHeader))) {
            std::cerr << "Dropped undersized Ethernet frame ("
                      << pkt_len << " bytes)" << std::endl;
            continue;
        }

        EthernetHeader* frame = reinterpret_cast<EthernetHeader*>(buffer);
        mac_table.insert(frame->src_mac, ingress_port, vlan_id);
        uint8_t egress_port = mac_table.destination(frame->dest_mac, vlan_id);

        uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

        if (memcmp(frame->dest_mac, broadcast, 6) == 0 || egress_port == 255) { 
            for (int i = 0; i < portList.size(); ++i) {
                if (ingress_port != i) {
                    if (portList[i]->send(buffer, pkt_len, vlan_id) == 0) {
                        monitor.write(buffer, pkt_len);
                    }
                }
            }
        } else {
            if (portList[egress_port]->send(buffer, pkt_len, vlan_id) == 0) {
                monitor.write(buffer, pkt_len);
            }
        }
    } 
}
