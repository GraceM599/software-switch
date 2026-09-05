#include <iostream>
#include <cstring>

#include "packet_struct.h"
#include "switch_port.h"
#include "macTable.h" 

void pollPort(Port* portList, uint8_t*& buffer, uint8_t& ingress){
    //fills buffer with the next packet to be processed. Assigns ingress
    //to the incoming interface.
    //poll all ports with actually scheduling algos but for now
    ingress = 0;
    buffer = portList[0].get();
}

void mac(uint8_t* addr){
    //gets the mac address given ip using arp ig?
    //like resolving interface cause ip is usally provided on startup?
}

int main() {
    std::cout << "Switch starting" << std::endl;
    Port portList[4];
    MacTable mac_table;
    uint8_t* buffer; //1518 is the MTU for now
    uint8_t ingress_port;
    while (true) { //needs to fork a child for each packet 
        buffer = nullptr;

        pollPort(portList, buffer, ingress_port);
        EthernetHeader* frame = reinterpret_cast<EthernetHeader*>(buffer);
        uint8_t egress_port = mac_table.destination(frame->dest_mac);
        if (0 == 255){ //frame->dest_mac == 255? 
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
        portList[egress_port].send(buffer);
        break;
    } 
}