#include <iostream>
#include <cstring>
#include <cerrno>

#include "packet_struct.h"
#include "switch_port.h"
#include "macTable.h" 
#include <poll.h>
#include <vector>
#include <memory>
#include <csignal>
 
volatile sig_atomic_t continue_running = 1;

void handle_signal(int signal_num){
    if (signal_num == SIGTERM || signal_num == SIGINT){
        continue_running = 0;
    }
    return;
}

void pollPort(std::vector<std::unique_ptr<Port>>& portList, std::vector<pollfd>& ports, uint8_t*& buffer, size_t buffer_size, uint8_t& ingress, ssize_t& pkt_len, size_t& next_port){
    //fills buffer with the next packet to be processed. Assigns ingress
    //to the incoming interface.
    for (size_t checked = 0; checked < portList.size(); ++checked) {
        size_t i = (next_port + checked) % portList.size();
        if (portList[i]->has_pending_packet()) {
            ingress = i;
            next_port = (i + 1) % portList.size();
            pkt_len = portList[i]->get(buffer, buffer_size);
            return;
        }
    }

    int result = poll(ports.data(), ports.size(), 1000);
    if (result < 0) {
        if (errno != EINTR) {
            std::cerr << "poll failed: " << std::strerror(errno) << std::endl;
        }
        return;
    }
    if (result == 0) {
        return;
    }

    for (size_t checked = 0; checked < ports.size(); ++checked) {
        size_t i = (next_port + checked) % ports.size();
        if (ports[i].revents & POLLIN) {
            ingress = i;
            next_port = (i + 1) % ports.size();
            pkt_len = portList[i]->get(buffer, buffer_size);
            return;
        }
    }
}


int main(int argc, char** argv) {
    std::cout << "Switch main() starting" << std::endl;
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <interface> [interface ...]"
                  << std::endl;
        return 2;
    }

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    std::vector<std::unique_ptr<Port>> portList;
    std::vector<pollfd> ports;
    for (int i = 0; i < argc - 1; ++i) {
        std::string intf_name = argv[1 + i];
        portList.push_back(std::make_unique<Port>(intf_name));
        if (!portList.back()->is_valid()) {
            std::cerr << "Failed to initialize switch port " << intf_name << std::endl;
            return 1;
        }
        pollfd pfd{};
        pfd.fd = portList.back()->get_fd();
        pfd.events = POLLIN;
        ports.push_back(pfd);
    }
    MacTable mac_table;

    uint8_t* buffer;
    uint8_t ingress_port;
    size_t next_port = 0;
    ssize_t pkt_len = 0; 
    uint8_t packet_pool[65536];

    while (continue_running) { //fc
        buffer = packet_pool;
        pkt_len = 0;

        pollPort(portList, ports, buffer, sizeof(packet_pool),
                 ingress_port, pkt_len, next_port);

        if (pkt_len <= 0) continue; 
        if (pkt_len < static_cast<ssize_t>(sizeof(EthernetHeader))) {
            std::cerr << "Dropped undersized Ethernet frame ("
                      << pkt_len << " bytes)" << std::endl;
            continue;
        }

        EthernetHeader* frame = reinterpret_cast<EthernetHeader*>(buffer);
        mac_table.insert(frame->src_mac, ingress_port);
        uint8_t egress_port = mac_table.destination(frame->dest_mac);

        uint8_t broadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

        if (memcmp(frame->dest_mac, broadcast, 6) == 0 || egress_port == 255) { 
            for (int i = 0; i < portList.size(); ++i) {
                if (ingress_port != i) {
                    portList[i]->send(buffer, pkt_len);
                }
            }
        } else {
            portList[egress_port]->send(buffer, pkt_len);
        }
    } 
}