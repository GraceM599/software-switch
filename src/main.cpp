#include <iostream>
#include <cstring>

#include "packet_struct.h"
#include "switch_port.h"
#include "macTable.h" 

void pollPort(Port* portList, uint8_t* buffer, uint8_t& ingress){
    //poll all ports with actually scheduling algos but for now
    ingress = 0;
    buffer = portList[0].get();
}

void mac(uint8_t* addr){
    //like resolving interface cause ip is usally provided on startup?
}

int main() {
    std::cout << "Switch starting" << std::endl;
    Port portList[4];
    MacTable mac_table;
    uint8_t buffer[1518];
    uint8_t ingress_port;
    while (true) {
        std::memset(buffer, 0, sizeof(buffer));

        pollPort(portList, buffer, ingress_port);
        EthernetHeader* frame = reinterpret_cast<EthernetHeader*>(buffer);
        uint8_t interface = mac_table.destination(frame->dest_mac);
        if (interface == 255){
            //flood all ports
            uint8_t arp_request[1518] = {0};
            for (int i = 0; i<4; ++i){
                if (ingress_port != i){
                    portList[i].send(arp_request);
                    //portList[i].get(buffer, interface);
                    //portList.insert(addr, i)
                }
            }
        }
        portList[interface].send(buffer);
    } 
}