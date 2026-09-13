#include <iostream>
#include <cstring>

#include "packet_struct.h"
#include "switch_port.h"
#include "macTable.h" 
#include <poll.h>
#include <vector>
#include <csignal>
 
volatile sig_atomic_t continue_running = 1;

void handle_signal(int signal_num){
    if (signal_num == SIGTERM || signal_num == SIGINT){
        continue_running = 0;
    }
    return;
}

void pollPort(Port* portList, pollfd* ports, uint8_t*& buffer, uint8_t& ingress, ssize_t& pkt_len){
    //fills buffer with the next packet to be processed. Assigns ingress
    //to the incoming interface.
    std::vector<int> ready_ports;

    int result = poll(ports, 4, 1000);
    if (result <= 0) {
        return;
    }
    //msg ready
    for (int i = 0; i < 4; ++i){
        if(ports[i].revents & POLLIN){
            ready_ports.push_back(i);
        }
    }
    //implement a scheduling algo but for now
    if (!ready_ports.empty()){
        ingress = ready_ports[0];
        pkt_len = portList[ingress].get(buffer);
    }

}


int main() {
    std::cout << "Switch main() starting" << std::endl;

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    Port portList[4] = {
        Port("tap0"),
        Port("tap1"),
        Port("tap2"),
        Port("tap3")
    };

    pollfd ports[4];
    for (int i = 0; i < 4; ++i) {
        ports[i].fd = portList[i].get_fd();
        ports[i].events = POLLIN;
        ports[i].revents = 0;
    }
    MacTable mac_table;

    uint8_t* buffer;
    uint8_t ingress_port;
    ssize_t pkt_len = 0; 
    uint8_t packet_pool[1518];

    while (continue_running) { //fc
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