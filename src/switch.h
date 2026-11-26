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


std::vector<PortConfig> parseConfig(const std::string& filename);


class Switch{
public:
Switch(std::string filename, std::atomic<bool>& continue_running, 
    char**& argv, PacketProcessor dp, uint32_t switch_id);
~Switch();

void manageControlPackets(ringBuffer<PacketInfo, 256>& rb);
inline void start(){
    *continue_running = 1;
}
inline void stop(){
    *continue_running = 0;
}


std::vector<uint8_t> buildLldpframe(uint8_t* switch_id, uint16_t port_id,
                                            uint8_t* src_mac, uint16_t ttl);

bool parseLldpFrame(std::vector<uint8_t>& frame, LldpInfo& ll_info);

std::vector<std::string> show_ports();

std::string show_vlans();


private:
std::vector<std::unique_ptr<Port>> portList;
MacTable mac_table;
Monitor monitor{"swm0", AcceptedFrames::All, 1};
std::vector<pollfd> ports;
std::atomic<bool>* continue_running;
PacketProcessor dp;
uint8_t* switch_id;
std::vector<std::thread> threads;
ringBuffer<PacketInfo, 256> rb;

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