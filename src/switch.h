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
#include <map>
#include <sstream>
#include <stdexcept>
#include <optional>
#include "dp.h"
#include "packet_struct.h"
#include "ring_buffer.h"
using PacketProcessor = void (*)(std::vector<std::unique_ptr<Port>>&, 
    pollfd&, int, MacTable&, std::atomic<bool>*, Monitor&, ringBuffer<PacketInfo, 256>& rb);


struct PortConfig {
    std::string host;
    std::string mode;
    uint16_t vlan;
    std::string ip;
};
struct NeighborEntry {
    uint32_t switch_id;
    uint16_t port_id;
    std::chrono::steady_clock::time_point expires_at;
};

inline std::vector<PortConfig> parseConfig(const std::string& filename){
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open config file: " + filename);
    }

    std::vector<PortConfig> config;
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream input(line);
        PortConfig port{};
        if (!(input >> port.host >> port.mode >> port.vlan >> port.ip) ||
            (port.mode != "access" && port.mode != "trunk") ||
            port.vlan == 0 || port.vlan > 4094) {
            throw std::runtime_error("Invalid config line: " + line);
        }
        config.push_back(port);
    }
    if (file.bad()) {
        throw std::runtime_error("Could not read config file: " + filename);
    }
    return config;
}


class Switch{
public:
Switch(std::string filename, std::atomic<bool>& continue_running, 
    char**& argv, PacketProcessor dp, uint32_t switch_id){
    //basically to make a switch all that it needs is a config filename and it will set everything else up
    //makes it easy to make multipel switches
    this->continue_running = &continue_running;
    std::cout << "Switch starting" << std::endl;

    std::vector<PortConfig> config;
    config = parseConfig(filename);
    ringBuffer<PacketInfo, 256> rb;

    std::vector<pollfd> ports;
    for (size_t i = 0; i < config.size(); ++i) {
        PortMode pm = config[i].mode == "trunk" ? PortMode::Trunk : PortMode::Access;
        std::vector<uint16_t> vlans = {config[i].vlan};
        std::string intf_name = argv[2+i];
        portList.push_back(std::make_unique<Port>(intf_name, pm, vlans));
        if (!portList.back()->is_valid()) {
            std::cerr << "Failed to initialize switch port " << intf_name << std::endl;
        }
        pollfd pfd{};
        pfd.fd = portList.back()->get_fd();
        pfd.events = POLLIN;
        ports.push_back(pfd);
    }
    this->dp = dp;

    std::vector<std::thread> threads;
    for (size_t i = 0; i < portList.size(); ++i) {
        threads.emplace_back(dp, std::ref(portList), std::ref(ports[i]), i, 
        std::ref(mac_table), this->continue_running, std::ref(monitor), std::ref(rb));
    } //prob should change continue_running to a shared pointer

    threads.emplace_back(manageControlPackets, std::ref(rb), this);
    for (auto& thread : threads) {
        thread.join();
    }

}

inline void manageControlPackets(ringBuffer<PacketInfo, 256>& rb, Switch* sw){
    using clock = std::chrono::steady_clock;
    std::unordered_map<uint16_t, NeighborEntry> neighbor_table;

    while (continue_running){
        PacketInfo pkt_info;
        bool success = rb.wait_and_pop(pkt_info, continue_running);

        auto now = clock::now();
        if (success) {
            LldpInfo ll_info;
            parseLldpFrame(pkt_info.data, ll_info);
        }
        else{
            std::vector<uint8_t> frame = buildLldpframe();
            for (int i = 0; i < portList.size(); ++i){
                portList[i]->send(frame.data(), frame.size(), 0)
            }
        }
        
        //every 2 seconds flood packets
    }
    
}
inline void start(){
    *continue_running = 1;
}
inline void stop(){
    *continue_running = 0;
}


inline std::vector<uint8_t> buildLldpframe(uint32_t switch_id, uint16_t port_id,
                                            uint8_t* src_mac, uint16_t ttl)
{
    std::vector<uint8_t> frame;
    frame.reserve(64);

    frame.insert(frame.end(), {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E});
    frame.insert(frame.end(), src_mac, src_mac+6);
    appendU16(frame, 0x88CC);

    appendU16(frame, TLV_HEADER(1, 5));
    frame.push_back(7);
    appendU32(frame, switch_id);

    appendU16(frame, TLV_HEADER(2, 3));
    frame.push_back(7);
    appendU16(frame, port_id);

    appendU16(frame, TLV_HEADER(3, 2));
    appendU16(frame, ttl);
    appendU16(frame, TLV_HEADER(0, 0));

    frame.resize(60, 0x00);
    return frame;

}

inline int parseLldpFrame(std::vector<uint8_t>& frame, LldpInfo& ll_info){
    //switch id, port id, ttl
    if (frame.size() < 16){
        return -1;
    }
    uint16_t ethertype = (uint16_t)((frame[12] << 8) | frame[13]);
    if (ethertype != 0x88CC){
        return -1;
    }

    int offset = 14; //start of tlv's
    while (offset + 2 <= frame.size()){
        int type = frame[14];
        int length = frame[15];
        if (type == 1){
            //switch_id
            memcpy(&ll_info.switch_id, &frame[offset+2], length);
            ll_info.switch_id = ntohs(ll_info.switch_id);
        }
        if (type == 2){
            //port id
            memcpy(&ll_info.port_id, &frame[offset+2], length);
            ll_info.port_id = ntohs(ll_info.port_id);
        }
        if (type == 3){
            //ttl
            memcpy(&ll_info.ttl, &frame[offset+2], length);
            ll_info.ttl = ntohs(ll_info.ttl);
        }
        //
        offset += length;

    }
    return 0;

}

std::vector<std::string> show_ports(){
    std::vector<std::string> p;
    for (int i = 0; i< portList.size(); ++i){
        p.push_back(portList[i]->getInterface());
    }
    return p;
}

std::string show_vlans(){
    std::map<uint16_t, std::vector<std::string>> vlan_interface_map = {}; 
    std::vector<std::string> trunk_interfaces = {};
    for (int i = 0; i<portList.size(); ++i){
        if (portList[i]->get_mode() == PortMode::Trunk){
            trunk_interfaces.push_back(portList[i]->getInterface());
        }
        else{
            std::unique_ptr<Port>& p = portList[i];
            if (vlan_interface_map.find(p->get_vlan_id()) == vlan_interface_map.end()){
                //vlan id has no entries yet
                vlan_interface_map.insert({p->get_vlan_id(), {p->getInterface()}});
            }
            else{
                //add interface onto existing vector
                vlan_interface_map.at(p->get_vlan_id()).push_back(p->getInterface());
            }
        }
    }
    for (std::string intf : trunk_interfaces){
        for (auto it = vlan_interface_map.begin(); it != vlan_interface_map.end(); ++it){
            it->second.push_back(intf);
        }
    }

    //format output
    std::string output = "";
    for (std::pair<uint16_t, std::vector<std::string>> vlan_pair : vlan_interface_map){
        output += vlan_pair.first;
        output += ": ";
        for (std::string intf : vlan_pair.second){
            output += intf + ", ";
        }
        output += '\n';
    }
    return output;


}


private:
std::vector<std::unique_ptr<Port>> portList;
MacTable mac_table;
Monitor monitor{"swm0", AcceptedFrames::All, 1};
std::vector<pollfd> ports;
std::atomic<bool>* continue_running;
PacketProcessor dp;
uint32_t switch_id;

inline void appendU16(std::vector<uint8_t>& buf, uint16_t val) {
    uint16_t net_val = htons(val);
    buf.push_back(static_cast<uint8_t>(net_val & 0xFF));
    buf.push_back(static_cast<uint8_t>((net_val >> 8) & 0xFF));
}
inline void appendU32(std::vector<uint8_t>& buf, uint32_t val) {
    uint32_t net_val = htonl(val);
    buf.push_back(static_cast<uint8_t>(net_val & 0xFF));
    buf.push_back(static_cast<uint8_t>((net_val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((net_val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((net_val >> 24) & 0xFF));
}
//config
//dataplane -> function ptr to processPackets function in use
};