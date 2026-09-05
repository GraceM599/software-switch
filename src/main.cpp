#include <iostream>
#include <cstring>

#include "packet_struct.h"
#include "switch_port.h"
#include "macTable.h" 
#include <poll.h>
#include <vector>
 
void initPorts(Port* portList, pollfd* ports, int numPorts){
    for (int i = 0; i< numPorts; ++i){
        portList[i] = Port("port" + std::to_string(i));
        ports[i] = pollfd();
        ports[i].fd = portList[i].get_fd();
        ports[i].events = POLLIN;

    }
}
void pollPort(Port* portList, pollfd* ports, uint8_t*& buffer, uint8_t& ingress, ssize_t& pkt_len){
    //fills buffer with the next packet to be processed. Assigns ingress
    //to the incoming interface.
    //poll all ports with actually scheduling algos but for now
    std::vector<int> ready_ports;

    if (poll(ports, 4, 1000)){
        //msg ready
        for (int i = 0; i < 4; ++i){
            if(ports[i].revents & POLLIN){
                ready_ports.push_back(i);
            }
        }
    }
    //implement a scheduling algo but for now
    if (!ready_ports.empty()){
        ingress = ready_ports[0];
        pkt_len = portList[ingress].get(buffer);
    }

}

void mac(uint8_t* addr){
    //gets the mac address given ip using arp ig?
    //like resolving interface cause ip is usally provided on startup?
}

int main() {
    std::cout << "Switch starting" << std::endl;
    Port portList[4];
    pollfd ports[4];
    initPorts(portList, ports, 4);
    MacTable mac_table;

    uint8_t* buffer;
    uint8_t ingress_port;
    ssize_t pkt_len = 0; 
    uint8_t packet_pool[1518];

    while (true) { //fc
        buffer = packet_pool;
        pkt_len = 0;

        pollPort(portList, ports, buffer, ingress_port, pkt_len);
        
        if (pkt_len <= 0) continue; 

        EthernetHeader* frame = reinterpret_cast<EthernetHeader*>(buffer);
        mac_table.insert(frame->src_mac, ingress_port);
        uint8_t egress_port = mac_table.destination(frame->dest_mac);

        uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

        if (memcmp(frame->dest_mac, broadcast, 6) == 0 || egress_port == 255) { 
            for (int i = 0; i < 4; ++i) {
                if (ingress_port != i) {
                    portList[i].send(buffer, pkt_len); 
                }
            }
        } else {
            portList[egress_port].send(buffer, pkt_len);
        }
    } 
}