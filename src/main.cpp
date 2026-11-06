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
volatile sig_atomic_t continue_running = 1;
MacTable mac_table;
Monitor monitor("swm0", AcceptedFrames::All, 1);
void handle_signal(int signal_num){
    if (signal_num == SIGTERM || signal_num == SIGINT){
        continue_running = 0;
    }
    return;
}


void pollPorts(std::vector<std::unique_ptr<Port>>& portList, std::vector<pollfd>& ports, uint8_t*& buffer, size_t buffer_size, uint8_t& ingress, ssize_t& pkt_len, size_t& next_port){
    //UNUSED
    //fills buffer with the next packet to be processed. Assigns ingress
    //to the incoming interface.
    uint16_t vlan_id;
    for (size_t checked = 0; checked < portList.size(); ++checked) {
        size_t i = (next_port + checked) % portList.size();
        if (portList[i]->mtx.try_lock() && portList[i]->has_pending_packet()) {
            ingress = i;
            next_port = (i + 1) % portList.size();
            pkt_len = portList[i]->get(buffer, buffer_size, vlan_id);
            return;
        }
        portList[i]->mtx.unlock();
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
        if (portList[i]->mtx.try_lock() && ports[i].revents & POLLIN) {
            ingress = i;
            next_port = (i + 1) % ports.size();
            pkt_len = portList[i]->get(buffer, buffer_size, vlan_id);
            return;
        }
        portList[i]->mtx.unlock();
    }
    
}

void pollPort(std::unique_ptr<Port>& port, int port_idx, uint8_t*& buffer, size_t buffer_size, uint8_t& ingress, size_t& pkt_len, uint16_t& vlan_id){
    //returns a filled buffer and ingress index if a pkt exists on the port
    if (port->has_pending_packet()){
        ingress = port_idx;
        pkt_len = port->get(buffer, buffer_size, vlan_id);
        return;
    }

}
void processPackets(std::vector<std::unique_ptr<Port>>& portList, pollfd& port_fd, int port_idx){
    uint8_t* buffer;
    uint8_t ingress_port;
    size_t next_port = 0;
    size_t pkt_len = 0; 
    uint8_t packet_pool[65536];
    uint16_t vlan_id = 0;

    while (continue_running) { //fc
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
                    portList[i]->send(buffer, pkt_len, vlan_id);
                    monitor.write(buffer, pkt_len);
                }
            }
        } else {
            portList[egress_port]->send(buffer, pkt_len, vlan_id);
            monitor.write(buffer, pkt_len);
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
    PortMode pm = PortMode::Trunk;
    std::vector<uint16_t> vlans = {1};
    for (int i = 0; i < argc - 1; ++i) {
        if (i == 1){
            vlans = {2};
        }
        if (i >= 2){
            pm = PortMode::Access;
            vlans.push_back(1);
        }
        std::string intf_name = argv[1 + i];
        portList.push_back(std::make_unique<Port>(intf_name, pm, vlans));
        if (!portList.back()->is_valid()) {
            std::cerr << "Failed to initialize switch port " << intf_name << std::endl;
            return 1;
        }
        pollfd pfd{};
        pfd.fd = portList.back()->get_fd();
        pfd.events = POLLIN;
        ports.push_back(pfd);
    }

    std::thread t0(processPackets, std::ref(portList), std::ref(ports[0]), 0);
    std::thread t1(processPackets, std::ref(portList), std::ref(ports[1]), 1);
    std::thread t2(processPackets, std::ref(portList), std::ref(ports[2]), 2);
    std::thread t3(processPackets, std::ref(portList), std::ref(ports[3]), 3);
    t0.join();
    t1.join();
    t2.join();
    t3.join();
    return 0;

    
}