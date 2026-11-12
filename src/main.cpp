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

std::atomic<bool> continue_running{true};
MacTable mac_table;
Monitor monitor("swm0", AcceptedFrames::All, 1);
void handle_signal(int signal_num){
    if (signal_num == SIGTERM || signal_num == SIGINT){
        continue_running = 0;
    }
    return;
}

struct PortConfig {
    std::string host;
    std::string mode;
    uint16_t vlan;
    std::string ip;
};

std::vector<PortConfig> parseConfig(const std::string& filename){
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

int main(int argc, char** argv) {
    std::cout << "Switch main() starting" << std::endl;

    std::vector<PortConfig> config;
    config = parseConfig(argv[1]);


    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    std::vector<std::unique_ptr<Port>> portList;
    std::vector<pollfd> ports;
    for (size_t i = 0; i < config.size(); ++i) {
        PortMode pm = config[i].mode == "trunk" ? PortMode::Trunk : PortMode::Access;
        std::vector<uint16_t> vlans = {config[i].vlan};
        std::string intf_name = argv[2 + i];
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

    std::vector<std::thread> threads;
    for (size_t i = 0; i < portList.size(); ++i) {
        threads.emplace_back(processPackets, std::ref(portList), std::ref(ports[i]), i, 
        std::ref(mac_table), std::ref(continue_running), std::ref(monitor));
    }
    for (auto& thread : threads) {
        thread.join();
    }
    return 0;

    
}