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
#include "dp.h"




struct PortConfig {
    std::string host;
    std::string mode;
    uint16_t vlan;
    std::string ip;
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
Switch(std::string filename, std::atomic<bool>& continue_running, char**& argv){
    //basically to make a switch all that it needs is a config filename and it will set everything else up
    //makes it easy to make multipel switches
    this->continue_running = &continue_running;
    std::cout << "Switch starting" << std::endl;

    std::vector<PortConfig> config;
    config = parseConfig(filename);

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

    std::vector<std::thread> threads;
    for (size_t i = 0; i < portList.size(); ++i) {
        threads.emplace_back(processPackets, std::ref(portList), std::ref(ports[i]), i, 
        std::ref(mac_table), this->continue_running, std::ref(monitor));
    } //prob should change continue_running to a shared pointer
    for (auto& thread : threads) {
        thread.join();
    }

}
inline void start(){
    *continue_running = 1;
}
inline void stop(){
    *continue_running = 0;
}

void show_ports();
void show_vlans();


private:
std::vector<std::unique_ptr<Port>> portList;
MacTable mac_table;
Monitor monitor{"swm0", AcceptedFrames::All, 1};
std::vector<pollfd> ports;
std::atomic<bool>* continue_running;


};
//config
//dataplane -> function ptr to processPackets function in use